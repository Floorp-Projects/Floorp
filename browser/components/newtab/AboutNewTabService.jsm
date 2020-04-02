/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const EXPORTED_SYMBOLS = ["AboutNewTabStubService"];

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

const PREF_SEPARATE_ABOUT_WELCOME = "browser.aboutwelcome.enabled";
const SEPARATE_ABOUT_WELCOME_URL =
  "resource://activity-stream/aboutwelcome/aboutwelcome.html";

const TOPIC_APP_QUIT = "quit-application-granted";
const TOPIC_CONTENT_DOCUMENT_INTERACTIVE = "content-document-interactive";

const BASE_URL = "resource://activity-stream/";
const ACTIVITY_STREAM_PAGES = new Set(["home", "newtab", "welcome"]);

const IS_PRIVILEGED_PROCESS =
  Services.appinfo.remoteType === E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE;

const PREF_SEPARATE_PRIVILEGEDABOUT_CONTENT_PROCESS =
  "browser.tabs.remote.separatePrivilegedContentProcess";
const PREF_ACTIVITY_STREAM_DEBUG = "browser.newtabpage.activity-stream.debug";

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
}

/**
 * The child-process implementation of nsIAboutNewTabService,
 * which also does the work of loading scripts from the ScriptPreloader
 * cache when using the privileged about content process.
 */
class AboutNewTabChildService extends BaseAboutNewTabService {
  constructor() {
    super();
    if (this.privilegedAboutProcessEnabled) {
      Services.obs.addObserver(this, TOPIC_CONTENT_DOCUMENT_INTERACTIVE);
      Services.obs.addObserver(this, TOPIC_APP_QUIT);
    }
  }

  observe(subject, topic, data) {
    switch (topic) {
      case TOPIC_APP_QUIT: {
        Services.obs.removeObserver(this, TOPIC_CONTENT_DOCUMENT_INTERACTIVE);
        Services.obs.removeObserver(this, TOPIC_APP_QUIT);
        break;
      }
      case TOPIC_CONTENT_DOCUMENT_INTERACTIVE: {
        if (!this.privilegedAboutProcessEnabled || !IS_PRIVILEGED_PROCESS) {
          return;
        }

        const win = subject.defaultView;

        // It seems like "content-document-interactive" is triggered multiple
        // times for a single window. The first event always seems to be an
        // HTMLDocument object that contains a non-null window reference
        // whereas the remaining ones seem to be proxied objects.
        // https://searchfox.org/mozilla-central/rev/d2966246905102b36ef5221b0e3cbccf7ea15a86/devtools/server/actors/object.js#100-102
        if (win === null) {
          return;
        }

        // We use win.location.pathname instead of win.location.toString()
        // because we want to account for URLs that contain the location hash
        // property or query strings (e.g. about:newtab#foo, about:home?bar).
        // Asserting here would be ideal, but this code path is also taken
        // by the view-source:// scheme, so we should probably just bail out
        // and do nothing.
        if (!ACTIVITY_STREAM_PAGES.has(win.location.pathname)) {
          return;
        }

        // Bail out early for separate about:welcome URL
        if (
          this.isSeparateAboutWelcome &&
          win.location.pathname.includes("welcome")
        ) {
          return;
        }

        const onLoaded = () => {
          const debugString = this.activityStreamDebug ? "-dev" : "";

          // This list must match any similar ones in render-activity-stream-html.js.
          const scripts = [
            "chrome://browser/content/contentSearchUI.js",
            "chrome://browser/content/contentSearchHandoffUI.js",
            "chrome://browser/content/contentTheme.js",
            `${BASE_URL}vendor/react${debugString}.js`,
            `${BASE_URL}vendor/react-dom${debugString}.js`,
            `${BASE_URL}vendor/prop-types.js`,
            `${BASE_URL}vendor/react-transition-group.js`,
            `${BASE_URL}vendor/redux.js`,
            `${BASE_URL}vendor/react-redux.js`,
            `${BASE_URL}data/content/activity-stream.bundle.js`,
          ];

          for (let script of scripts) {
            Services.scriptloader.loadSubScript(script, win); // Synchronous call
          }
        };
        win.addEventListener("DOMContentLoaded", onLoaded, { once: true });
        break;
      }
    }
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
