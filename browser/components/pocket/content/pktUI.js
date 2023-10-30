/*
 * LICENSE
 *
 * POCKET MARKS
 *
 * Notwithstanding the permitted uses of the Software (as defined below) pursuant to the license set forth below, "Pocket," "Read It Later" and the Pocket icon and logos (collectively, the “Pocket Marks”) are registered and common law trademarks of Read It Later, Inc. This means that, while you have considerable freedom to redistribute and modify the Software, there are tight restrictions on your ability to use the Pocket Marks. This license does not grant you any rights to use the Pocket Marks except as they are embodied in the Software.
 *
 * ---
 *
 * SOFTWARE
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Pocket UI module
 *
 * Handles interactions with Pocket buttons, panels and menus.
 *
 */

// TODO : Get the toolbar icons from Firefox's build (Nikki needs to give us a red saved icon)
// TODO : [needs clarificaiton from Fx] Firefox's plan was to hide Pocket from context menus until the user logs in. Now that it's an extension I'm wondering if we still need to do this.
// TODO : [needs clarificaiton from Fx] Reader mode (might be a something they need to do since it's in html, need to investigate their code)
// TODO : [needs clarificaiton from Fx] Move prefs within pktApi.s to sqlite or a local file so it's not editable (and is safer)
// TODO : [nice to have] - Immediately save, buffer the actions in a local queue and send (so it works offline, works like our native extensions)

/* eslint-disable no-shadow */
/* eslint-env mozilla/browser-window */

ChromeUtils.defineESModuleGetters(this, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  pktApi: "chrome://pocket/content/pktApi.sys.mjs",
  pktTelemetry: "chrome://pocket/content/pktTelemetry.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  ReaderMode: "resource://gre/modules/ReaderMode.sys.mjs",
  SaveToPocket: "chrome://pocket/content/SaveToPocket.sys.mjs",
});

const POCKET_HOME_PREF = "extensions.pocket.showHome";

var pktUI = (function () {
  let _titleToSave = "";
  let _urlToSave = "";

  // Initial sizes are only here to help visual load jank before the panel is ready.
  const initialPanelSize = {
    signup: {
      height: 315,
      width: 328,
    },
    saved: {
      height: 110,
      width: 350,
    },
    home: {
      height: 251,
      width: 328,
    },
    // This is for non English sizes, this is not for an AB experiment.
    home_no_topics: {
      height: 86,
      width: 328,
    },
  };

  var pocketHomePref;

  function initPrefs() {
    pocketHomePref = Services.prefs.getBoolPref(POCKET_HOME_PREF);
  }
  initPrefs();

  // -- Communication to API -- //

  /**
   * Either save or attempt to log the user in
   */
  function tryToSaveCurrentPage() {
    tryToSaveUrl(getCurrentUrl(), getCurrentTitle());
  }

  function tryToSaveUrl(url, title) {
    // Validate input parameter
    if (typeof url !== "undefined" && url.startsWith("about:reader?url=")) {
      url = ReaderMode.getOriginalUrl(url);
    }

    // If the user is not logged in, show the logged-out state to prompt them to authenticate
    if (!pktApi.isUserLoggedIn()) {
      showSignUp();
      return;
    }

    _titleToSave = title;
    _urlToSave = url;
    // If the user is logged in, and the url is valid, go ahead and save the current page
    if (!pocketHomePref || isValidURL()) {
      saveAndShowConfirmation();
      return;
    }
    showPocketHome();
  }

  // -- Panel UI -- //

  /**
   * Show the sign-up panel
   */
  function showSignUp() {
    getFirefoxAccountSignedInUser(function (userdata) {
      showPanel(
        "about:pocket-signup?" +
          "emailButton=" +
          NimbusFeatures.saveToPocket.getVariable("emailButton"),
        `signup`
      );
    });
  }

  /**
   * Show the logged-out state / sign-up panel
   */
  function saveAndShowConfirmation() {
    getFirefoxAccountSignedInUser(function (userdata) {
      showPanel(
        "about:pocket-saved?premiumStatus=" +
          (pktApi.isPremiumUser() ? "1" : "0") +
          "&fxasignedin=" +
          (typeof userdata == "object" && userdata !== null ? "1" : "0"),
        `saved`
      );
    });
  }

  /**
   * Show the Pocket home panel state
   */
  function showPocketHome() {
    const hideRecentSaves =
      NimbusFeatures.saveToPocket.getVariable("hideRecentSaves");
    const locale = getUILocale();
    let panel = `home_no_topics`;
    if (locale.startsWith("en-")) {
      panel = `home`;
    }
    showPanel(`about:pocket-home?hiderecentsaves=${hideRecentSaves}`, panel);
  }

  /**
   * Open a generic panel
   */
  function showPanel(urlString, panel) {
    const locale = getUILocale();
    const options = initialPanelSize[panel];

    resizePanel({
      width: options.width,
      height: options.height,
    });

    const saveToPocketExperiment = ExperimentAPI.getExperimentMetaData({
      featureId: "saveToPocket",
    });

    const saveToPocketRollout = ExperimentAPI.getRolloutMetaData({
      featureId: "saveToPocket",
    });

    const pocketNewtabExperiment = ExperimentAPI.getExperimentMetaData({
      featureId: "pocketNewtab",
    });

    const pocketNewtabRollout = ExperimentAPI.getRolloutMetaData({
      featureId: "pocketNewtab",
    });

    // We want to know if the user is in a Pocket related experiment or rollout,
    // but we have 2 Pocket related features, so we prioritize the saveToPocket feature,
    // and experiments over rollouts.
    const experimentMetaData =
      saveToPocketExperiment ||
      pocketNewtabExperiment ||
      saveToPocketRollout ||
      pocketNewtabRollout;

    let utmSource = "firefox_pocket_save_button";
    let utmCampaign = experimentMetaData?.slug;
    let utmContent = experimentMetaData?.branch?.slug;

    const url = new URL(urlString);
    // A set of params shared across all panels.
    url.searchParams.append("utmSource", utmSource);
    if (utmCampaign && utmContent) {
      url.searchParams.append("utmCampaign", utmCampaign);
      url.searchParams.append("utmContent", utmContent);
    }
    url.searchParams.append("locale", locale);

    // We don't have to hide and show the panel again if it's already shown
    // as if the user tries to click again on the toolbar button the overlay
    // will close instead of the button will be clicked
    var frame = getPanelFrame();

    // Load the frame
    frame.setAttribute("src", url.href);
  }

  function onShowSignup() {
    // Ensure opening the signup panel clears the icon state from any previous sessions.
    SaveToPocket.itemDeleted();
    pktTelemetry.submitPocketButtonPing("click", "save_button");
  }

  async function onShowHome() {
    pktTelemetry.submitPocketButtonPing("click", "home_button");

    if (!NimbusFeatures.saveToPocket.getVariable("hideRecentSaves")) {
      let recentSaves = await pktApi.getRecentSavesCache();
      if (recentSaves) {
        // We have cache, so we can use those.
        pktUIMessaging.sendMessageToPanel("PKT_renderRecentSaves", recentSaves);
      } else {
        // Let the client know we're loading fresh recs.
        pktUIMessaging.sendMessageToPanel(
          "PKT_loadingRecentSaves",
          recentSaves
        );
        // We don't have cache, so fetch fresh stories.
        pktApi.getRecentSaves({
          success(data) {
            pktUIMessaging.sendMessageToPanel("PKT_renderRecentSaves", data);
          },
          error(error) {
            pktUIMessaging.sendErrorMessageToPanel(
              "PKT_renderRecentSaves",
              error
            );
          },
        });
      }
    }
  }

  function onShowSaved() {
    var saveLinkMessageId = "PKT_saveLink";

    // Send error message for invalid url
    if (!isValidURL()) {
      let errorData = {
        localizedKey: "pocket-panel-saved-error-only-links",
      };
      pktUIMessaging.sendErrorMessageToPanel(saveLinkMessageId, errorData);
      return;
    }

    // Check online state
    if (!navigator.onLine) {
      let errorData = {
        localizedKey: "pocket-panel-saved-error-no-internet",
      };
      pktUIMessaging.sendErrorMessageToPanel(saveLinkMessageId, errorData);
      return;
    }

    pktTelemetry.submitPocketButtonPing("click", "save_button");

    // Add url
    var options = {
      success(data, request) {
        var item = data.item;
        var ho2 = data.ho2;
        var accountState = data.account_state;
        var displayName = data.display_name;
        var successResponse = {
          status: "success",
          accountState,
          displayName,
          item,
          ho2,
        };
        pktUIMessaging.sendMessageToPanel(saveLinkMessageId, successResponse);
        SaveToPocket.itemSaved();

        if (!NimbusFeatures.saveToPocket.getVariable("hideRecentSaves")) {
          // Articles saved for the first time (by anyone) won't have a resolved_id
          if (item?.resolved_id && item?.resolved_id !== "0") {
            pktApi.getArticleInfo(item.resolved_url, {
              success(data) {
                pktUIMessaging.sendMessageToPanel(
                  "PKT_articleInfoFetched",
                  data
                );
              },
              done() {
                pktUIMessaging.sendMessageToPanel(
                  "PKT_getArticleInfoAttempted"
                );
              },
            });
          } else {
            pktUIMessaging.sendMessageToPanel("PKT_getArticleInfoAttempted");
          }
        }
      },
      error(error, request) {
        // If user is not authorized show singup page
        if (request.status === 401) {
          showSignUp();
          return;
        }

        // For unknown server errors, use a generic catch-all error message
        let errorData = {
          localizedKey: "pocket-panel-saved-error-generic",
        };

        // Send error message to panel
        pktUIMessaging.sendErrorMessageToPanel(saveLinkMessageId, errorData);
      },
    };

    // Add title if given
    if (typeof _titleToSave !== "undefined") {
      options.title = _titleToSave;
    }

    // Send the link
    pktApi.addLink(_urlToSave, options);
  }

  /**
   * Resize the panel
   * options = {
   *  width: ,
   *  height: ,
   * }
   */
  function resizePanel(options = {}) {
    var frame = getPanelFrame();

    // Set an explicit size, panel will adapt.
    frame.style.width = options.width + "px";
    frame.style.height = options.height + "px";
  }

  // -- Browser Navigation -- //

  /**
   * Open a new tab with a given url and notify the frame panel that it was opened
   */

  function openTabWithUrl(url, aTriggeringPrincipal, aCsp) {
    let recentWindow = Services.wm.getMostRecentWindow("navigator:browser");
    if (!recentWindow) {
      console.error("Pocket: No open browser windows to openTabWithUrl");
      return;
    }
    closePanel();

    // If the user is in permanent private browsing than this is not an issue,
    // since the current window will always share the same cookie jar as the other
    // windows.
    if (
      !PrivateBrowsingUtils.isWindowPrivate(recentWindow) ||
      PrivateBrowsingUtils.permanentPrivateBrowsing
    ) {
      recentWindow.openWebLinkIn(url, "tab", {
        triggeringPrincipal: aTriggeringPrincipal,
        csp: aCsp,
      });
      return;
    }

    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      if (!PrivateBrowsingUtils.isWindowPrivate(win)) {
        win.openWebLinkIn(url, "tab", {
          triggeringPrincipal: aTriggeringPrincipal,
          csp: aCsp,
        });
        return;
      }
    }

    // If there were no non-private windows opened already.
    recentWindow.openWebLinkIn(url, "window", {
      triggeringPrincipal: aTriggeringPrincipal,
      csp: aCsp,
    });
  }

  // Open a new tab with a given url
  function onOpenTabWithUrl(data, contentPrincipal, csp) {
    try {
      urlSecurityCheck(
        data.url,
        contentPrincipal,
        Services.scriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL
      );
    } catch (ex) {
      return;
    }

    // We don't track every click, only clicks with a known source.
    if (data.source) {
      const { position, source, model } = data;
      pktTelemetry.submitPocketButtonPing("click", source, position, model);
    }

    var url = data.url;
    openTabWithUrl(url, contentPrincipal, csp);
  }

  // Open a new tab with a Pocket story url
  function onOpenTabWithPocketUrl(data, contentPrincipal, csp) {
    try {
      urlSecurityCheck(
        data.url,
        contentPrincipal,
        Services.scriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL
      );
    } catch (ex) {
      return;
    }

    const { url, position, model } = data;
    // Check to see if we need to and can fire valid telemetry.
    if (model && (position || position === 0)) {
      pktTelemetry.submitPocketButtonPing(
        "click",
        "on_save_recs",
        position,
        model
      );
    }

    openTabWithUrl(url, contentPrincipal, csp);
  }

  // -- Helper Functions -- //

  function getCurrentUrl() {
    return gBrowser.currentURI.spec;
  }

  function getCurrentTitle() {
    return gBrowser.contentTitle;
  }

  function closePanel() {
    // The panel frame doesn't exist until the Pocket panel is showing.
    // So we ensure it is open before attempting to hide it.
    getPanelFrame()?.closest("panel")?.hidePopup();
  }

  var toolbarPanelFrame;

  function setToolbarPanelFrame(frame) {
    toolbarPanelFrame = frame;
  }

  function getPanelFrame() {
    return toolbarPanelFrame;
  }

  function isValidURL() {
    return (
      typeof _urlToSave !== "undefined" &&
      (_urlToSave.startsWith("http") || _urlToSave.startsWith("https"))
    );
  }

  function getFirefoxAccountSignedInUser(callback) {
    fxAccounts
      .getSignedInUser()
      .then(userData => {
        callback(userData);
      })
      .then(null, error => {
        callback();
      });
  }

  function getUILocale() {
    return Services.locale.appLocaleAsBCP47;
  }

  /**
   * Public functions
   */
  return {
    setToolbarPanelFrame,
    getPanelFrame,
    initPrefs,
    showPanel,
    getUILocale,

    openTabWithUrl,
    onOpenTabWithUrl,
    onOpenTabWithPocketUrl,
    onShowSaved,
    onShowSignup,
    onShowHome,

    tryToSaveUrl,
    tryToSaveCurrentPage,
    resizePanel,
    closePanel,
  };
})();

// -- Communication to Background -- //
var pktUIMessaging = (function () {
  /**
   * Send a message to the panel's frame
   */
  function sendMessageToPanel(messageId, payload) {
    var panelFrame = pktUI.getPanelFrame();
    if (!panelFrame) {
      console.warn("Pocket panel frame is undefined");
      return;
    }

    const aboutPocketActor =
      panelFrame?.browsingContext?.currentWindowGlobal?.getActor("AboutPocket");

    // Send message to panel
    aboutPocketActor?.sendAsyncMessage(messageId, payload);
  }

  /**
   * Helper function to package an error object and send it to the panel
   * frame as a message response
   */
  function sendErrorMessageToPanel(messageId, error) {
    var errorResponse = { status: "error", error };
    sendMessageToPanel(messageId, errorResponse);
  }

  /**
   * Public
   */
  return {
    sendMessageToPanel,
    sendErrorMessageToPanel,
  };
})();
