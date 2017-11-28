/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
                                  "resource://gre/modules/FormHistory.jsm");

add_task(async function test() {
  // This test relies on the form history being empty to start with delete
  // all the items first.
  await new Promise((resolve, reject) => {
    FormHistory.update({ op: "remove" },
                       { handleError(error) {
                           reject(error);
                         },
                         handleCompletion(reason) {
                           if (!reason) {
                             resolve();
                           } else {
                             reject();
                           }
                         },
                       });
  });

  let tempScope = {};
  Services.scriptloader.loadSubScript("chrome://browser/content/sanitize.js", tempScope);
  let Sanitizer = tempScope.Sanitizer;
  let s = new Sanitizer();
  s.prefDomain = "privacy.cpd.";
  let prefBranch = Services.prefs.getBranch(s.prefDomain);

  prefBranch.setBoolPref("cache", false);
  prefBranch.setBoolPref("cookies", false);
  prefBranch.setBoolPref("downloads", false);
  prefBranch.setBoolPref("formdata", true);
  prefBranch.setBoolPref("history", false);
  prefBranch.setBoolPref("offlineApps", false);
  prefBranch.setBoolPref("passwords", false);
  prefBranch.setBoolPref("sessions", false);
  prefBranch.setBoolPref("siteSettings", false);

  // Sanitize now so we can test the baseline point.
  await s.sanitize();
  ok(!gFindBar.hasTransactions, "pre-test baseline for sanitizer");

  gFindBar.getElement("findbar-textbox").value = "m";
  ok(gFindBar.hasTransactions, "formdata can be cleared after input");

  await s.sanitize();
  is(gFindBar.getElement("findbar-textbox").value, "", "findBar textbox should be empty after sanitize");
  ok(!gFindBar.hasTransactions, "No transactions after sanitize");
});
