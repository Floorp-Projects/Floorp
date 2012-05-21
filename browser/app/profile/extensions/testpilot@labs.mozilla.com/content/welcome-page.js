/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var TestPilotWelcomePage = {
  surveyId: "basic_panel_survey_2",

  onLoad: function() {
    // Show link to pilot background survey only if user hasn't already
    // taken it.
    Components.utils.import("resource://testpilot/modules/setup.js");
    Components.utils.import("resource://testpilot/modules/tasks.js");
    this._setStrings();
    let survey = TestPilotSetup.getTaskById(this.surveyId);
    if (!survey) {
      // Can happen if page loaded before all tasks loaded
      window.setTimeout(function() { TestPilotWelcomePage.onLoad(); }, 2000);
      return;
    }
    if (survey.status == TaskConstants.STATUS_NEW) {
      document.getElementById("survey-link-p").setAttribute("style",
                                                            "display:block");
    }
  },

  openPilotSurvey: function() {
    let url =
      "chrome://testpilot/content/take-survey.html?eid=" + this.surveyId;
    TestPilotWindowUtils.openChromeless(url);
  },

  _setStrings: function() {
    let stringBundle =
      Components.classes["@mozilla.org/intl/stringbundle;1"].
        getService(Components.interfaces.nsIStringBundleService).
	  createBundle("chrome://testpilot/locale/main.properties");
    let map = [
      { id: "page-title", stringKey: "testpilot.fullBrandName" },
      { id: "thank-you-text",
        stringKey: "testpilot.welcomePage.thankYou" },
      { id: "getting-started-text",
        stringKey: "testpilot.welcomePage.gettingStarted" },
      { id: "please-take-text",
        stringKey: "testpilot.welcomePage.pleaseTake" },
      { id: "background-survey-text",
        stringKey: "testpilot.welcomePage.backgroundSurvey" },
      { id: "open-studies-window-link",
        stringKey: "testpilot.welcomePage.clickToOpenStudiesWindow" },
      { id: "testpilot-addon-text",
        stringKey: "testpilot.welcomePage.testpilotAddon" },
      { id: "icon-explanation-text",
        stringKey: "testpilot.welcomePage.iconExplanation" },
      { id: "icon-explanation-more-text",
        stringKey: "testpilot.welcomePage.moreIconExplanation" },
      { id: "notification-info-text",
	stringKey: "testpilot.welcomePage.notificationInfo" },
      { id: "privacy-policy-link",
	stringKey: "testpilot.welcomePage.privacyPolicy" },
      { id: "legal-notices-link",
	stringKey: "testpilot.welcomePage.legalNotices" }
      ];

    let mapLength = map.length;
    for (let i = 0; i < mapLength; i++) {
      let entry = map[i];
      document.getElementById(entry.id).innerHTML =
        stringBundle.GetStringFromName(entry.stringKey);
    }
  }
};