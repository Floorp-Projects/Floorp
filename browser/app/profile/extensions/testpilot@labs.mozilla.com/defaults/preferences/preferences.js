/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pref("extensions.testpilot.indexFileName", "index.json");

pref("extensions.testpilot@labs.mozilla.com.description", "chrome://testpilot/locale/main.properties");

pref("extensions.testpilot.popup.delayAfterStartup", 180000); // 3 minutes
pref("extensions.testpilot.popup.timeBetweenChecks", 86400000); // 24 hours
pref("extensions.testpilot.uploadRetryInterval", 3600000); // 1 hour

pref("extensions.testpilot.popup.showOnNewStudy", false);
pref("extensions.testpilot.popup.showOnStudyFinished", true);
pref("extensions.testpilot.popup.showOnNewResults", false);
pref("extensions.testpilot.alwaysSubmitData", false);
pref("extensions.testpilot.runStudies", true);
pref("extensions.testpilot.alreadyCustomizedToolbar", false);

pref("extensions.testpilot.indexBaseURL", "https://testpilot.mozillalabs.com/testcases/");
pref("extensions.testpilot.firstRunUrl", "chrome://testpilot/content/welcome.html");
pref("extensions.testpilot.dataUploadURL", "https://testpilot.mozillalabs.com/submit/");
pref("extensions.testpilot.homepageURL", "https://testpilot.mozillalabs.com/");


pref("extensions.input.happyURL", "http://input.mozilla.com/happy");
pref("extensions.input.sadURL", "http://input.mozilla.com/sad");
pref("extensions.input.brokenURL", "http://input.mozilla.com/feedback#broken");
pref("extensions.input.ideaURL", "http://input.mozilla.com/feedback#idea");
