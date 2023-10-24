/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * The nsIAboutNewTabService is accessed by the AboutRedirector anytime
 * about:home, about:newtab or about:welcome are requested. The primary
 * job of an nsIAboutNewTabService is to tell the AboutRedirector what
 * resources to actually load for those requests.
 *
 * The nsIAboutNewTabService is not involved when the user has overridden
 * the default about:home or about:newtab pages.
 *
 * There are two implementations of this service - one for the parent
 * process, and one for content processes. Each one has some secondary
 * responsibilties that are process-specific.
 *
 * The need for two implementations is an unfortunate consequence of how
 * document loading and process redirection for about: pages currently
 * works in Gecko. The commonalities between the two implementations has
 * been put into an abstract base class.
 */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { E10SUtils } from "resource://gre/modules/E10SUtils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BasePromiseWorker: "resource://gre/modules/PromiseWorker.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});

/**
 * BEWARE: Do not add variables for holding state in the global scope.
 * Any state variables should be properties of the appropriate class
 * below. This is to avoid confusion where the state is set in one process,
 * but not in another.
 *
 * Constants are fine in the global scope.
 */

const PREF_ABOUT_HOME_CACHE_TESTING =
  "browser.startup.homepage.abouthome_cache.testing";
const ABOUT_WELCOME_URL =
  "resource://activity-stream/aboutwelcome/aboutwelcome.html";

const CACHE_WORKER_URL = "resource://activity-stream/lib/cache.worker.js";

const IS_PRIVILEGED_PROCESS =
  Services.appinfo.remoteType === E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE;

const PREF_SEPARATE_PRIVILEGEDABOUT_CONTENT_PROCESS =
  "browser.tabs.remote.separatePrivilegedContentProcess";
const PREF_ACTIVITY_STREAM_DEBUG = "browser.newtabpage.activity-stream.debug";

/**
 * The AboutHomeStartupCacheChild is responsible for connecting the
 * nsIAboutNewTabService with a cached document and script for about:home
 * if one happens to exist. The AboutHomeStartupCacheChild is only ever
 * handed the streams for those caches when the "privileged about content
 * process" first launches, so subsequent loads of about:home do not read
 * from this cache.
 *
 * See https://firefox-source-docs.mozilla.org/browser/components/newtab/docs/v2-system-addon/about_home_startup_cache.html
 * for further details.
 */
export const AboutHomeStartupCacheChild = {
  _initted: false,
  CACHE_REQUEST_MESSAGE: "AboutHomeStartupCache:CacheRequest",
  CACHE_RESPONSE_MESSAGE: "AboutHomeStartupCache:CacheResponse",
  CACHE_USAGE_RESULT_MESSAGE: "AboutHomeStartupCache:UsageResult",
  STATES: {
    UNAVAILABLE: 0,
    UNCONSUMED: 1,
    PAGE_CONSUMED: 2,
    PAGE_AND_SCRIPT_CONSUMED: 3,
    FAILED: 4,
    DISQUALIFIED: 5,
  },
  REQUEST_TYPE: {
    PAGE: 0,
    SCRIPT: 1,
  },
  _state: 0,
  _consumerBCID: null,

  /**
   * Called via a process script very early on in the process lifetime. This
   * prepares the AboutHomeStartupCacheChild to pass an nsIChannel back to
   * the nsIAboutNewTabService when the initial about:home document is
   * eventually requested.
   *
   * @param pageInputStream (nsIInputStream)
   *   The stream for the cached page markup.
   * @param scriptInputStream (nsIInputStream)
   *   The stream for the cached script to run on the page.
   */
  init(pageInputStream, scriptInputStream) {
    if (
      !IS_PRIVILEGED_PROCESS &&
      !Services.prefs.getBoolPref(PREF_ABOUT_HOME_CACHE_TESTING, false)
    ) {
      throw new Error(
        "Can only instantiate in the privileged about content processes."
      );
    }

    if (!lazy.NimbusFeatures.abouthomecache.getVariable("enabled")) {
      return;
    }

    if (this._initted) {
      throw new Error("AboutHomeStartupCacheChild already initted.");
    }

    Services.obs.addObserver(this, "memory-pressure");
    Services.cpmm.addMessageListener(this.CACHE_REQUEST_MESSAGE, this);

    this._pageInputStream = pageInputStream;
    this._scriptInputStream = scriptInputStream;
    this._initted = true;
    this.setState(this.STATES.UNCONSUMED);
  },

  /**
   * A function that lets us put the AboutHomeStartupCacheChild back into
   * its initial state. This is used by tests to let us simulate the startup
   * behaviour of the module without having to manually launch a new privileged
   * about content process every time.
   */
  uninit() {
    if (!Services.prefs.getBoolPref(PREF_ABOUT_HOME_CACHE_TESTING, false)) {
      throw new Error(
        "Cannot uninit AboutHomeStartupCacheChild unless testing."
      );
    }

    if (!this._initted) {
      return;
    }

    Services.obs.removeObserver(this, "memory-pressure");
    Services.cpmm.removeMessageListener(this.CACHE_REQUEST_MESSAGE, this);

    if (this._cacheWorker) {
      this._cacheWorker.terminate();
      this._cacheWorker = null;
    }

    this._pageInputStream = null;
    this._scriptInputStream = null;
    this._initted = false;
    this._state = this.STATES.UNAVAILABLE;
    this._consumerBCID = null;
  },

  /**
   * A public method called from nsIAboutNewTabService that attempts
   * return an nsIChannel for a cached about:home document that we
   * were initialized with. If we failed to be initted with the
   * cache, or the input streams that we were sent have no data
   * yet available, this function returns null. The caller should
   * fall back to generating the page dynamically.
   *
   * This function will be called when loading about:home, or
   * about:home?jscache - the latter returns the cached script.
   *
   * It is expected that the same BrowsingContext that loads the cached
   * page will also load the cached script.
   *
   * @param uri (nsIURI)
   *   The URI for the requested page, as passed by nsIAboutNewTabService.
   * @param loadInfo (nsILoadInfo)
   *   The nsILoadInfo for the requested load, as passed by
   *   nsIAboutNewWTabService.
   * @return nsIChannel or null.
   */
  maybeGetCachedPageChannel(uri, loadInfo) {
    if (!this._initted) {
      return null;
    }

    if (this._state >= this.STATES.PAGE_AND_SCRIPT_CONSUMED) {
      return null;
    }

    let requestType =
      uri.query === "jscache"
        ? this.REQUEST_TYPE.SCRIPT
        : this.REQUEST_TYPE.PAGE;

    // If this is a page request, then we need to be in the UNCONSUMED state,
    // since we expect the page request to come first. If this is a script
    // request, we expect to be in PAGE_CONSUMED state, since the page cache
    // stream should he been consumed already.
    if (
      (requestType === this.REQUEST_TYPE.PAGE &&
        this._state !== this.STATES.UNCONSUMED) ||
      (requestType === this.REQUEST_TYPE_SCRIPT &&
        this._state !== this.STATES.PAGE_CONSUMED)
    ) {
      return null;
    }

    // If by this point, we don't have anything in the streams,
    // then either the cache was too slow to give us data, or the cache
    // doesn't exist. The caller should fall back to generating the
    // page dynamically.
    //
    // We only do this on the page request, because by the time
    // we get to the script request, we should have already drained
    // the page input stream.
    if (requestType === this.REQUEST_TYPE.PAGE) {
      try {
        if (
          !this._scriptInputStream.available() ||
          !this._pageInputStream.available()
        ) {
          this.setState(this.STATES.FAILED);
          this.reportUsageResult(false /* success */);
          return null;
        }
      } catch (e) {
        this.setState(this.STATES.FAILED);
        if (e.result === Cr.NS_BASE_STREAM_CLOSED) {
          this.reportUsageResult(false /* success */);
          return null;
        }
        throw e;
      }
    }

    if (
      requestType === this.REQUEST_TYPE.SCRIPT &&
      this._consumerBCID !== loadInfo.browsingContextID
    ) {
      // Some other document is somehow requesting the script - one
      // that didn't originally request the page. This is not allowed.
      this.setState(this.STATES.FAILED);
      return null;
    }

    let channel = Cc[
      "@mozilla.org/network/input-stream-channel;1"
    ].createInstance(Ci.nsIInputStreamChannel);
    channel.QueryInterface(Ci.nsIChannel);
    channel.setURI(uri);
    channel.loadInfo = loadInfo;
    channel.contentStream =
      requestType === this.REQUEST_TYPE.PAGE
        ? this._pageInputStream
        : this._scriptInputStream;

    if (requestType === this.REQUEST_TYPE.SCRIPT) {
      this.setState(this.STATES.PAGE_AND_SCRIPT_CONSUMED);
      this.reportUsageResult(true /* success */);
    } else {
      this.setState(this.STATES.PAGE_CONSUMED);
      // Stash the BrowsingContext ID so that when the script stream
      // attempts to be consumed, we ensure that it's from the same
      // BrowsingContext that loaded the page.
      this._consumerBCID = loadInfo.browsingContextID;
    }

    return channel;
  },

  /**
   * This function takes the state information required to generate
   * the about:home cache markup and script, and then generates that
   * markup in script asynchronously. Once that's done, a message
   * is sent to the parent process with the nsIInputStream's for the
   * markup and script contents.
   *
   * @param state (Object)
   *   The Redux state of the about:home document to render.
   * @return Promise
   * @resolves undefined
   *   After the message with the nsIInputStream's have been sent to
   *   the parent.
   */
  async constructAndSendCache(state) {
    if (!IS_PRIVILEGED_PROCESS) {
      throw new Error("Wrong process type.");
    }

    let worker = this.getOrCreateWorker();

    TelemetryStopwatch.start("FX_ABOUTHOME_CACHE_CONSTRUCTION");

    let { page, script } = await worker
      .post("construct", [state])
      .finally(() => {
        TelemetryStopwatch.finish("FX_ABOUTHOME_CACHE_CONSTRUCTION");
      });

    let pageInputStream = Cc[
      "@mozilla.org/io/string-input-stream;1"
    ].createInstance(Ci.nsIStringInputStream);

    pageInputStream.setUTF8Data(page);

    let scriptInputStream = Cc[
      "@mozilla.org/io/string-input-stream;1"
    ].createInstance(Ci.nsIStringInputStream);

    scriptInputStream.setUTF8Data(script);

    Services.cpmm.sendAsyncMessage(this.CACHE_RESPONSE_MESSAGE, {
      pageInputStream,
      scriptInputStream,
    });
  },

  _cacheWorker: null,
  getOrCreateWorker() {
    if (this._cacheWorker) {
      return this._cacheWorker;
    }

    this._cacheWorker = new lazy.BasePromiseWorker(CACHE_WORKER_URL);
    return this._cacheWorker;
  },

  receiveMessage(message) {
    if (message.name === this.CACHE_REQUEST_MESSAGE) {
      let { state } = message.data;
      this.constructAndSendCache(state);
    }
  },

  reportUsageResult(success) {
    Services.cpmm.sendAsyncMessage(this.CACHE_USAGE_RESULT_MESSAGE, {
      success,
    });
  },

  observe(subject, topic, data) {
    if (topic === "memory-pressure" && this._cacheWorker) {
      this._cacheWorker.terminate();
      this._cacheWorker = null;
    }
  },

  /**
   * Transitions the AboutHomeStartupCacheChild from one state
   * to the next, where each state is defined in this.STATES.
   *
   * States can only be transitioned in increasing order, otherwise
   * an error is logged.
   */
  setState(state) {
    if (state > this._state) {
      this._state = state;
    } else {
      console.error(
        "AboutHomeStartupCacheChild could not transition from state " +
          `${this._state} to ${state}`,
        new Error().stack
      );
    }
  },

  /**
   * If the cache hasn't been used, transitions it into the DISQUALIFIED
   * state so that it cannot be used. This should be called if it's been
   * determined that about:newtab is going to be loaded, which doesn't
   * use the cache.
   */
  disqualifyCache() {
    if (this._state === this.STATES.UNCONSUMED) {
      this.setState(this.STATES.DISQUALIFIED);
      this.reportUsageResult(false /* success */);
    }
  },
};

/**
 * This is an abstract base class for the nsIAboutNewTabService
 * implementations that has some common methods and properties.
 */
class BaseAboutNewTabService {
  constructor() {
    if (!AppConstants.RELEASE_OR_BETA) {
      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        "activityStreamDebug",
        PREF_ACTIVITY_STREAM_DEBUG,
        false
      );
    } else {
      this.activityStreamDebug = false;
    }

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "privilegedAboutProcessEnabled",
      PREF_SEPARATE_PRIVILEGEDABOUT_CONTENT_PROCESS,
      false
    );

    this.classID = Components.ID("{cb36c925-3adc-49b3-b720-a5cc49d8a40e}");
    this.QueryInterface = ChromeUtils.generateQI([
      "nsIAboutNewTabService",
      "nsIObserver",
    ]);
  }

  /**
   * Returns the default URL.
   *
   * This URL depends on various activity stream prefs. Overriding
   * the newtab page has no effect on the result of this function.
   */
  get defaultURL() {
    // Generate the desired activity stream resource depending on state, e.g.,
    // "resource://activity-stream/prerendered/activity-stream.html"
    // "resource://activity-stream/prerendered/activity-stream-debug.html"
    // "resource://activity-stream/prerendered/activity-stream-noscripts.html"
    return [
      "resource://activity-stream/prerendered/",
      "activity-stream",
      // Debug version loads dev scripts but noscripts separately loads scripts
      this.activityStreamDebug && !this.privilegedAboutProcessEnabled
        ? "-debug"
        : "",
      this.privilegedAboutProcessEnabled ? "-noscripts" : "",
      ".html",
    ].join("");
  }

  get welcomeURL() {
    /*
     * Returns the about:welcome URL
     *
     * This is calculated in the same way the default URL is.
     */

    lazy.NimbusFeatures.aboutwelcome.recordExposureEvent({ once: true });
    if (lazy.NimbusFeatures.aboutwelcome.getVariable("enabled") ?? true) {
      return ABOUT_WELCOME_URL;
    }
    return this.defaultURL;
  }

  aboutHomeChannel(uri, loadInfo) {
    throw Components.Exception(
      "AboutHomeChannel not implemented for this process.",
      Cr.NS_ERROR_NOT_IMPLEMENTED
    );
  }
}

/**
 * The child-process implementation of nsIAboutNewTabService,
 * which also does the work of redirecting about:home loads to
 * the about:home startup cache if its available.
 */
class AboutNewTabChildService extends BaseAboutNewTabService {
  aboutHomeChannel(uri, loadInfo) {
    if (IS_PRIVILEGED_PROCESS) {
      let cacheChannel = AboutHomeStartupCacheChild.maybeGetCachedPageChannel(
        uri,
        loadInfo
      );
      if (cacheChannel) {
        return cacheChannel;
      }
    }

    let pageURI = Services.io.newURI(this.defaultURL);
    let fileChannel = Services.io.newChannelFromURIWithLoadInfo(
      pageURI,
      loadInfo
    );
    fileChannel.originalURI = uri;
    return fileChannel;
  }

  get defaultURL() {
    if (IS_PRIVILEGED_PROCESS) {
      // This is a bit of a hack, but attempting to load about:newtab will
      // enter this code path in order to get at the expected URL, and we
      // can use that to disqualify the about:home cache, since we don't
      // use it for about:newtab loads, and we don't want the about:home
      // cache to be wildly out of date when about:home is eventually
      // loaded (for example, in the first new window).
      AboutHomeStartupCacheChild.disqualifyCache();
    }

    return super.defaultURL;
  }
}

/**
 * The AboutNewTabStubService is a function called in both the main and
 * content processes when trying to get at the nsIAboutNewTabService. This
 * function does the job of choosing the appropriate implementation of
 * nsIAboutNewTabService for the process type.
 */
export function AboutNewTabStubService() {
  if (Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT) {
    return new BaseAboutNewTabService();
  }
  return new AboutNewTabChildService();
}
