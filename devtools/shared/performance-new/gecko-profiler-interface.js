/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file is for the new performance panel that targets profiler.firefox.com,
 * not the default-enabled DevTools performance panel.
 */

const { Ci } = require("chrome");
const Services = require("Services");

loader.lazyImporter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");
loader.lazyRequireGetter(
  this,
  "RecordingUtils",
  "devtools/shared/performance-new/recording-utils"
);

// Some platforms are built without the Gecko Profiler.
const IS_SUPPORTED_PLATFORM = "nsIProfiler" in Ci;

/**
 * The GeckoProfiler already has an interface to control it through the
 * nsIProfiler component. However, this class implements an interface that can
 * be used on both the actor, and the profiler popup. This allows us to share
 * the UI for the devtools front-end and the profiler popup code. The devtools
 * code needs to work through the actor system, while the popup code controls
 * the Gecko Profiler on the current browser.
 */
class ActorReadyGeckoProfilerInterface {
  /**
   * @param {Object} options
   * @param options.gzipped - This flag controls whether or not to gzip the profile when
   *   capturing it. The profiler popup wants a gzipped profile in an array buffer, while
   *   the devtools want the full object. See Bug 1581963 to perhaps provide an API
   *   to request the gzipped profile. This would then remove this configuration from
   *   the GeckoProfilerInterface.
   */
  constructor(
    options = {
      gzipped: true,
    }
  ) {
    // Only setup the observers on a supported platform.
    if (IS_SUPPORTED_PLATFORM) {
      this._observer = {
        observe: this._observe.bind(this),
      };
      Services.obs.addObserver(this._observer, "profiler-started");
      Services.obs.addObserver(this._observer, "profiler-stopped");
      Services.obs.addObserver(
        this._observer,
        "chrome-document-global-created"
      );
      Services.obs.addObserver(this._observer, "last-pb-context-exited");
    }
    this.gzipped = options.gzipped;

    EventEmitter.decorate(this);
  }

  destroy() {
    if (!IS_SUPPORTED_PLATFORM) {
      return;
    }
    Services.obs.removeObserver(this._observer, "profiler-started");
    Services.obs.removeObserver(this._observer, "profiler-stopped");
    Services.obs.removeObserver(
      this._observer,
      "chrome-document-global-created"
    );
    Services.obs.removeObserver(this._observer, "last-pb-context-exited");
  }

  startProfiler(options) {
    if (!IS_SUPPORTED_PLATFORM) {
      return false;
    }

    // For a quick implementation, decide on some default values. These may need
    // to be tweaked or made configurable as needed.
    const settings = {
      entries: options.entries || 1000000,
      duration: options.duration || 0,
      interval: options.interval || 1,
      features: options.features || [
        "js",
        "stackwalk",
        "cpu",
        "responsiveness",
        "threads",
        "leaf",
      ],
      threads: options.threads || ["GeckoMain", "Compositor"],
      activeBrowsingContextID: RecordingUtils.getActiveBrowsingContextID(),
    };

    try {
      // This can throw an error if the profiler is in the wrong state.
      Services.profiler.StartProfiler(
        settings.entries,
        settings.interval,
        settings.features,
        settings.threads,
        settings.activeBrowsingContextID,
        settings.duration
      );
    } catch (e) {
      // In case any errors get triggered, bailout with a false.
      return false;
    }

    return true;
  }

  stopProfilerAndDiscardProfile() {
    if (!IS_SUPPORTED_PLATFORM) {
      return;
    }
    Services.profiler.StopProfiler();
  }

  /**
   * @type {string} debugPath
   * @type {string} breakpadId
   * @returns {Promise<[number[], number[], number[]]>}
   */
  async getSymbolTable(debugPath, breakpadId) {
    const [addr, index, buffer] = await Services.profiler.getSymbolTable(
      debugPath,
      breakpadId
    );
    // The protocol does not support the transfer of typed arrays, so we convert
    // these typed arrays to plain JS arrays of numbers now.
    // Our return value type is declared as "array:array:number".
    return [Array.from(addr), Array.from(index), Array.from(buffer)];
  }

  async getProfileAndStopProfiler() {
    if (!IS_SUPPORTED_PLATFORM) {
      return null;
    }

    // Pause profiler before we collect the profile, so that we don't capture
    // more samples while the parent process or android threads wait for subprocess profiles.
    Services.profiler.Pause();

    let profile;
    try {
      // Attempt to pull out the data.
      if (this.gzipped) {
        profile = await Services.profiler.getProfileDataAsGzippedArrayBuffer();
      } else {
        profile = await Services.profiler.getProfileDataAsync();

        if (Object.keys(profile).length === 0) {
          console.error(
            "An empty object was received from getProfileDataAsync.getProfileDataAsync(), " +
              "meaning that a profile could not successfully be serialized and captured."
          );
          profile = null;
        }
      }
    } catch (e) {
      // Explicitly set the profile to null if there as an error.
      profile = null;
      console.error(
        `There was an error fetching a profile (gzipped: ${this.gzipped})`,
        e
      );
    }

    // Stop and discard the buffers.
    Services.profiler.StopProfiler();

    // Returns a profile when successful, and null when there is an error.
    return profile;
  }

  isActive() {
    if (!IS_SUPPORTED_PLATFORM) {
      return false;
    }
    return Services.profiler.IsActive();
  }

  isSupportedPlatform() {
    return IS_SUPPORTED_PLATFORM;
  }

  isLockedForPrivateBrowsing() {
    if (!IS_SUPPORTED_PLATFORM) {
      return false;
    }
    return !Services.profiler.CanProfile();
  }

  /**
   * Watch for events that happen within the browser. These can affect the
   * current availability and state of the Gecko Profiler.
   */
  _observe(subject, topic, _data) {
    // Note! If emitting new events make sure and update the list of bridged
    // events in the perf actor.
    switch (topic) {
      case "chrome-document-global-created":
        if (
          subject.isChromeWindow &&
          PrivateBrowsingUtils.isWindowPrivate(subject)
        ) {
          this.emit("profile-locked-by-private-browsing");
        }
        break;
      case "last-pb-context-exited":
        this.emit("profile-unlocked-from-private-browsing");
        break;
      case "profiler-started":
        const param = subject.QueryInterface(Ci.nsIProfilerStartParams);
        this.emit(
          topic,
          param.entries,
          param.interval,
          param.features,
          param.duration,
          param.activeBrowsingContextID
        );
        break;
      case "profiler-stopped":
        this.emit(topic);
        break;
    }
  }

  /**
   * Lists the supported features of the profiler for the current browser.
   * @returns {string[]}
   */
  getSupportedFeatures() {
    if (!IS_SUPPORTED_PLATFORM) {
      return [];
    }
    return Services.profiler.GetFeatures();
  }

  /**
   * @param {string} type
   * @param {() => void} listener
   */
  on(type, listener) {
    // This is a stub for TypeScript. This function is assigned by the EventEmitter
    // decorator.
  }

  /**
   * @param {string} type
   * @param {() => void} listener
   */
  off(type, listener) {
    // This is a stub for TypeScript. This function is assigned by the EventEmitter
    // decorator.
  }
}

exports.ActorReadyGeckoProfilerInterface = ActorReadyGeckoProfilerInterface;
