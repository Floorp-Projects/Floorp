/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let prefService = Cc["@mozilla.org/preferences-service;1"]
                    .getService(Components.interfaces.nsIPrefBranch2);

  let findBar = gFindBar;
  let textbox = gFindBar.getElement("findbar-textbox");

  let tempScope = {};
  Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader)
                                             .loadSubScript("chrome://browser/content/sanitize.js", tempScope);
  let Sanitizer = tempScope.Sanitizer;
  let s = new Sanitizer();
  s.prefDomain = "privacy.cpd.";
  let prefBranch = prefService.getBranch(s.prefDomain);

  prefBranch.setBoolPref("cache", false);
  prefBranch.setBoolPref("cookies", false);
  prefBranch.setBoolPref("downloads", false);
  prefBranch.setBoolPref("formdata", true);
  prefBranch.setBoolPref("history", false);
  prefBranch.setBoolPref("offlineApps", false);
  prefBranch.setBoolPref("passwords", false);
  prefBranch.setBoolPref("sessions", false);
  prefBranch.setBoolPref("siteSettings", false);

  // Sanitize now so we can test that canClear is correct
  s.sanitize();
  ok(!s.canClearItem("formdata"), "pre-test baseline for sanitizer");
  textbox.value = "m";
  ok(s.canClearItem("formdata"), "formdata can be cleared after input");
  s.sanitize();
  is(textbox.value, "", "findBar textbox should be empty after sanitize");
  ok(!s.canClearItem("formdata"), "canClear now false after sanitize");
  finish();
}
