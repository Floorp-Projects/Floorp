/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const EXPORTED_SYMBOLS = [
  "AboutNewTabStubService",
  "AboutHomeStartupCacheChild",
];

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

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);

/**
 * BEWARE: Do not add variables for holding state in the global scope.
 * Any state variables should be properties of the appropriate class
 * below. This is to avoid confusion where the state is set in one process,
 * but not in another.
 *
 * Constants are fine in the global scope.
 */

const PREF_ABOUT_HOME_CACHE_ENABLED =
  "browser.startup.homepage.abouthome_cache.enabled";
const PREF_SEPARATE_ABOUT_WELCOME = "browser.aboutwelcome.enabled";
const SEPARATE_ABOUT_WELCOME_URL =
  "resource://activity-stream/aboutwelcome/aboutwelcome.html";

ChromeUtils.defineModuleGetter(
  this,
  "BasePromiseWorker",
  "resource://gre/modules/PromiseWorker.jsm"
);

const CACHE_WORKER_URL = "resource://activity-stream/lib/cache-worker.js";

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
const AboutHomeStartupCacheChild = {
  _initted: false,
  CACHE_REQUEST_MESSAGE: "AboutHomeStartupCache:CacheRequest",
  CACHE_RESPONSE_MESSAGE: "AboutHomeStartupCache:CacheResponse",

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
    if (!IS_PRIVILEGED_PROCESS) {
      throw new Error(
        "Can only instantiate in the privileged about content processes."
      );
    }

    if (!Services.prefs.getBoolPref(PREF_ABOUT_HOME_CACHE_ENABLED, false)) {
      return;
    }

    if (this._initted) {
      throw new Error("AboutHomeStartupCacheChild already initted.");
    }

    Services.cpmm.addMessageListener(this.CACHE_REQUEST_MESSAGE, this);

    this._pageInputStream = pageInputStream;
    this._scriptInputStream = scriptInputStream;
    this._initted = true;
  },

  /**
   * A public method called from nsIAboutNewTabService that attempts
   * return an nsIChannel for a cached about:home document that we
   * were initialized with. If we failed to be initted with the
   * cache, or the input streams that we were sent have no data
   * yet available, this function returns null. The caller should =
   * fall back to generating the page dynamically.
   *
   * This function will be called when loading about:home, or
   * about:home?jscache - the latter returns the cached script.
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

    let isScriptRequest = uri.query === "jscache";

    // If by this point, we don't have anything in the streams,
    // then either the cache was too slow to give us data, or the cache
    // doesn't exist. The caller should fall back to generating the
    // page dynamically.
    //
    // We only do this on the page request, because by the time
    // we get to the script request, we should have already drained
    // the page input stream.
    if (!isScriptRequest) {
      try {
        if (
          !this._scriptInputStream.available() ||
          !this._pageInputStream.available()
        ) {
          return null;
        }
      } catch (e) {
        if (e.result === Cr.NS_BASE_STREAM_CLOSED) {
          return null;
        }
        throw e;
      }
    }

    let channel = Cc[
      "@mozilla.org/network/input-stream-channel;1"
    ].createInstance(Ci.nsIInputStreamChannel);
    channel.QueryInterface(Ci.nsIChannel);
    channel.setURI(uri);
    channel.loadInfo = loadInfo;
    channel.contentStream = isScriptRequest
      ? this._scriptInputStream
      : this._pageInputStream;

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

    let { page, script } = await worker.post("construct", [state]);

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

    this._cacheWorker = new BasePromiseWorker(CACHE_WORKER_URL);
    return this._cacheWorker;
  },

  receiveMessage(message) {
    if (message.name === this.CACHE_REQUEST_MESSAGE) {
      let { state } = message.data;
      this.constructAndSendCache(state);
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
      "isSeparateAboutWelcome",
      PREF_SEPARATE_ABOUT_WELCOME,
      false
    );

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "privilegedAboutProcessEnabled",
      PREF_SEPARATE_PRIVILEGEDABOUT_CONTENT_PROCESS,
      false
    );

    this.classID = Components.ID("{cb36c925-3adc-49b3-b720-a5cc49d8a40e}");
    this.QueryInterface = ChromeUtils.generateQI([
      Ci.nsIAboutNewTabService,
      Ci.nsIObserver,
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

  /*
   * Returns the about:welcome URL
   *
   * This is calculated in the same way the default URL is.
   */
  get welcomeURL() {
    if (this.isSeparateAboutWelcome) {
      return SEPARATE_ABOUT_WELCOME_URL;
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
}

/**
 * The AboutNewTabStubService is a function called in both the main and
 * content processes when trying to get at the nsIAboutNewTabService. This
 * function does the job of choosing the appropriate implementation of
 * nsIAboutNewTabService for the process type.
 */
function AboutNewTabStubService() {
  if (Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT) {
    return new BaseAboutNewTabService();
  }
  return new AboutNewTabChildService();
}
