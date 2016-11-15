// Bug 474792 - Clear "Never remember passwords for this site" when
// clearing site-specific settings in Clear Recent History dialog

var tempScope = {};
Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader)
                                           .loadSubScript("chrome://browser/content/sanitize.js", tempScope);
var Sanitizer = tempScope.Sanitizer;

add_task(function*() {
  // getLoginSavingEnabled always returns false if password capture is disabled.
  yield SpecialPowers.pushPrefEnv({"set": [["signon.rememberSignons", true]]});

  var pwmgr = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);

  // Add a disabled host
  pwmgr.setLoginSavingEnabled("http://example.com", false);
  // Sanity check
  is(pwmgr.getLoginSavingEnabled("http://example.com"), false,
     "example.com should be disabled for password saving since we haven't cleared that yet.");

  // Set up the sanitizer to just clear siteSettings
  let s = new Sanitizer();
  s.ignoreTimespan = false;
  s.prefDomain = "privacy.cpd.";
  var itemPrefs = gPrefService.getBranch(s.prefDomain);
  itemPrefs.setBoolPref("history", false);
  itemPrefs.setBoolPref("downloads", false);
  itemPrefs.setBoolPref("cache", false);
  itemPrefs.setBoolPref("cookies", false);
  itemPrefs.setBoolPref("formdata", false);
  itemPrefs.setBoolPref("offlineApps", false);
  itemPrefs.setBoolPref("passwords", false);
  itemPrefs.setBoolPref("sessions", false);
  itemPrefs.setBoolPref("siteSettings", true);

  // Clear it
  yield s.sanitize();

  // Make sure it's gone
  is(pwmgr.getLoginSavingEnabled("http://example.com"), true,
     "example.com should be enabled for password saving again now that we've cleared.");

  yield SpecialPowers.popPrefEnv();
});
