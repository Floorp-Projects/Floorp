/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* globals ChromeUtils, Services, Cc, Ci, HomePage, Assert, add_task */
"use strict";

ChromeUtils.import("resource:///modules/HomePage.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

add_task(function testHomePage() {
  Assert.ok(!HomePage.overridden, "Homepage should not be overriden by default.");
  let newvalue = "about:blank|about:newtab";
  HomePage.set(newvalue);
  Assert.ok(HomePage.overridden, "Homepage should be overriden after set()");
  Assert.equal(HomePage.get(), newvalue, "Homepage should be ${newvalue}");
  Assert.notEqual(HomePage.getDefault(), newvalue, "Homepage should be ${newvalue}");
  HomePage.reset();
  Assert.ok(!HomePage.overridden, "Homepage should not be overriden by after reset.");
  Assert.equal(HomePage.get(), HomePage.getDefault(),
               "Homepage and default should be equal after reset.");
});

add_task(function readLocalizedHomepage() {
  let newvalue = "data:text/plain,browser.startup.homepage%3Dabout%3Alocalized";
  let complexvalue = Cc["@mozilla.org/pref-localizedstring;1"]
                   .createInstance(Ci.nsIPrefLocalizedString);
  complexvalue.data = newvalue;
  Services.prefs.getDefaultBranch(null).setComplexValue(
    "browser.startup.homepage",
    Ci.nsIPrefLocalizedString,
    complexvalue);
    Assert.ok(!HomePage.overridden, "Complex value only works as default");
    Assert.equal(HomePage.get(), "about:localized", "Get value from bundle");
});

add_task(function recoverEmptyHomepage() {
  Assert.ok(!HomePage.overridden, "Homepage should not be overriden by default.");
  Services.prefs.setStringPref("browser.startup.homepage", "");
  Assert.ok(HomePage.overridden, "Homepage is overriden with empty string.");
  Assert.equal(HomePage.get(), HomePage.getDefault(), "Recover is default");
  Assert.ok(!HomePage.overridden, "Recover should have set default");
});
