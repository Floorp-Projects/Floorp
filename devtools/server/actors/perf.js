/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const protocol = require("devtools/shared/protocol");
const { ActorClassWithSpec, Actor } = protocol;
const { perfSpec } = require("devtools/shared/specs/perf");
const { Cc, Ci } = require("chrome");
const Services = require("Services");

loader.lazyGetter(this, "geckoProfiler", () => {
  return Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
});

loader.lazyImporter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

// Some platforms are built without the Gecko Profiler.
const IS_SUPPORTED_PLATFORM = "nsIProfiler" in Ci;

/**
 * The PerfActor wraps the Gecko Profiler interface
 */
exports.PerfActor = ActorClassWithSpec(perfSpec, {
  initialize(conn) {
    Actor.prototype.initialize.call(this, conn);

    // Only setup the observers on a supported platform.
    if (IS_SUPPORTED_PLATFORM) {
      this._observer = {
        observe: this._observe.bind(this)
      };
      Services.obs.addObserver(this._observer, "profiler-started");
      Services.obs.addObserver(this._observer, "profiler-stopped");
      Services.obs.addObserver(this._observer, "chrome-document-global-created");
      Services.obs.addObserver(this._observer, "last-pb-context-exited");
    }
  },

  destroy() {
    if (!IS_SUPPORTED_PLATFORM) {
      return;
    }
    Services.obs.removeObserver(this._observer, "profiler-started");
    Services.obs.removeObserver(this._observer, "profiler-stopped");
    Services.obs.removeObserver(this._observer, "chrome-document-global-created");
    Services.obs.removeObserver(this._observer, "last-pb-context-exited");
    Actor.prototype.destroy.call(this);
  },

  startProfiler() {
    if (!IS_SUPPORTED_PLATFORM) {
      return false;
    }

    // For a quick implementation, decide on some default values. These may need
    // to be tweaked or made configurable as needed.
    const settings = {
      entries: 1000000,
      interval: 1,
      features: ["js", "stackwalk", "threads", "leaf"],
      threads: ["GeckoMain", "Compositor"]
    };

    try {
      // This can throw an error if the profiler is in the wrong state.
      geckoProfiler.StartProfiler(
        settings.entries,
        settings.interval,
        settings.features,
        settings.features.length,
        settings.threads,
        settings.threads.length
      );
    } catch (e) {
      // In case any errors get triggered, bailout with a false.
      return false;
    }

    return true;
  },

  stopProfilerAndDiscardProfile() {
    if (!IS_SUPPORTED_PLATFORM) {
      return;
    }
    geckoProfiler.StopProfiler();
  },

  async getProfileAndStopProfiler() {
    if (!IS_SUPPORTED_PLATFORM) {
      return null;
    }
    let profile;
    try {
      // Attempt to pull out the data.
      profile = await geckoProfiler.getProfileDataAsync();

      // Stop and discard the buffers.
      geckoProfiler.StopProfiler();
    } catch (e) {
      // If there was any kind of error, bailout with no profile.
      return null;
    }

    // Gecko Profiler errors can return an empty object, return null for this case
    // as well.
    if (Object.keys(profile).length === 0) {
      return null;
    }
    return profile;
  },

  isActive() {
    if (!IS_SUPPORTED_PLATFORM) {
      return false;
    }
    return geckoProfiler.IsActive();
  },

  isSupportedPlatform() {
    return IS_SUPPORTED_PLATFORM;
  },

  isLockedForPrivateBrowsing() {
    if (!IS_SUPPORTED_PLATFORM) {
      return false;
    }
    return !geckoProfiler.CanProfile();
  },

  /**
   * Watch for events that happen within the browser. These can affect the current
   * availability and state of the Gecko Profiler.
   */
  _observe(subject, topic, _data) {
    switch (topic) {
      case "chrome-document-global-created":
        if (PrivateBrowsingUtils.isWindowPrivate(subject)) {
          this.emit("profile-locked-by-private-browsing");
        }
        break;
      case "last-pb-context-exited":
        this.emit("profile-unlocked-from-private-browsing");
        break;
      case "profiler-started":
      case "profiler-stopped":
        this.emit(topic);
        break;
    }
  }
});
