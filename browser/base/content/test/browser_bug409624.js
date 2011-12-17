/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
                                  "resource://gre/modules/FormHistory.jsm");

function test() {
  waitForExplicitFinish();

  // This test relies on the form history being empty to start with delete
  // all the items first.
  FormHistory.update({ op: "remove" },
                     { onSuccess: function () { test2(); },
                       onFailure: function (error) {
                         do_throw("Error occurred removing form history: " + error);
                       }
                     });
}

function test2()
{
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

  // Sanitize now so we can test that canClear is correct. Formdata is cleared asynchronously.
  s.sanitize();
  s.canClearItem("formdata", clearDone1, s);
}

function clearDone1(aItemName, aResult, aSanitizer)
{
  ok(!aResult, "pre-test baseline for sanitizer");
  gFindBar.getElement("findbar-textbox").value = "m";
  aSanitizer.canClearItem("formdata", inputEntered, aSanitizer);
}

function inputEntered(aItemName, aResult, aSanitizer)
{
  ok(aResult, "formdata can be cleared after input");
  aSanitizer.sanitize();
  aSanitizer.canClearItem("formdata", clearDone2);
}

function clearDone2(aItemName, aResult)
{
  is(gFindBar.getElement("findbar-textbox").value, "", "findBar textbox should be empty after sanitize");
  ok(!aResult, "canClear now false after sanitize");
  finish();
}
