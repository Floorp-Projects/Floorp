/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let EXPORTED_SYMBOLS = [ "KeywordURLResetPrompter" ];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

const KEYWORD_PROMPT_REV = 1;

let KeywordURLResetPrompter = {
  get shouldPrompt() {
    let keywordURLUserSet = Services.prefs.prefHasUserValue("keyword.URL");
    let declinedRev;
    try {
      declinedRev = Services.prefs.getIntPref("browser.keywordURLPromptDeclined");
    } catch (ex) {}

    return keywordURLUserSet && declinedRev != KEYWORD_PROMPT_REV;
  },

  prompt: function KeywordURLResetPrompter_prompt(win, keywordURI) {
    let tabbrowser = win.gBrowser;
    let notifyBox = tabbrowser.getNotificationBox();

    let existingNotification = notifyBox.getNotificationWithValue("keywordURL-reset");
    if (existingNotification)
      return;

    // Find the name/URI of this build's original default engine.
    // XXX: Can't use originalDefaultEngine getter here, because that doesn't
    //      use the default pref branch.
    let defaultURI;
    let defaultEngine;
    try {
      let defaultPB = Services.prefs.getDefaultBranch(null);
      let defaultName = defaultPB.getComplexValue("browser.search.defaultenginename",
                                                  Ci.nsIPrefLocalizedString).data;
      defaultEngine = Services.search.getEngineByName(defaultName);
      defaultURI = defaultEngine.getSubmission("foo").uri;
    } catch (ex) {
      // Something went horribly wrong! bail out
      return;
    }

    // If the user-set value has the same base domain as the default, don't
    // prompt.
    let keywordBaseDomain;
    try {
      keywordBaseDomain = Services.eTLD.getBaseDomain(keywordURI);
      if (keywordBaseDomain == Services.eTLD.getBaseDomain(defaultURI))
        return;
    } catch (ex) {}

    if (!keywordBaseDomain)
      return;

    let brandBundle  = Services.strings.createBundle("chrome://branding/locale/brand.properties");
    let brandShortName = brandBundle.GetStringFromName("brandShortName");

    let browserBundle = win.gNavigatorBundle;
    let msg = browserBundle.getFormattedString("keywordPrompt.message",
                                               [brandShortName, keywordBaseDomain,
                                                defaultEngine.name]);
    let buttons = [
      {
        label: browserBundle.getFormattedString("keywordPrompt.yesButton",
                                                [defaultEngine.name]),
        accessKey: browserBundle.getString("keywordPrompt.yesButton.accessKey"),
        popup:     null,
        callback: function(aNotificationBar, aButton) {
          Services.prefs.clearUserPref("keyword.URL");
          Services.prefs.clearUserPref("browser.search.defaultenginename");
          try {
            // If the currently loaded URI still has the same base domain as the
            // keyword URI (this is used as a rough approximation of whether the
            // user is still seeing search results as opposed to having clicked
            // on a result link), load the default engine's searchForm URL so
            // that they can re-do their search.
            let currentBaseDomain = Services.eTLD.getBaseDomain(tabbrowser.currentURI);
            if (currentBaseDomain == keywordBaseDomain)
              tabbrowser.loadURI(defaultEngine.searchForm);
          } catch (ex) {}
        }
      },
      {
        label: browserBundle.getFormattedString("keywordPrompt.noButton",
                                                [keywordBaseDomain]),
        accessKey: browserBundle.getString("keywordPrompt.noButton.accessKey"),
        popup:     null,
        callback: function(aNotificationBar, aButton) {
          Services.prefs.setIntPref("browser.keywordURLPromptDeclined", KEYWORD_PROMPT_REV);
        }
      }
    ];
    
    let notification = notifyBox.appendNotification(msg, "keywordURL-reset", null, notifyBox.PRIORITY_WARNING_HIGH, buttons);
    notification.setAttribute("hideclose", true);
    // stick around for a few page loads in case there are redirects involved
    notification.persistence = 3;
  }
}
