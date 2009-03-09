// Bug 474792 - Clear "Never remember passwords for this site" when
// clearing site-specific settings in Clear Recent History dialog

Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Components.interfaces.mozIJSSubScriptLoader)
                                           .loadSubScript("chrome://browser/content/sanitize.js");

function test() {

  var pwmgr = Components.classes["@mozilla.org/login-manager;1"]
             .getService(Components.interfaces.nsILoginManager);

  // Add a disabled host
  pwmgr.setLoginSavingEnabled("http://example.com", false);
  
  // Sanity check
  is(pwmgr.getLoginSavingEnabled("http://example.com"), false,
     "example.com should be disabled for password saving since we haven't cleared that yet.");

  // Set up the sanitizer to just clear siteSettings
  let s = new Sanitizer();
  s.ignoreTimespan = false;
  s.prefDomain = "privacy.cpd.";
  var itemPrefs = Cc["@mozilla.org/preferences-service;1"]
                  .getService(Components.interfaces.nsIPrefService)
                  .getBranch(s.prefDomain);
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
  s.sanitize();
  
  // Make sure it's gone
  is(pwmgr.getLoginSavingEnabled("http://example.com"), true,
     "example.com should be enabled for password saving again now that we've cleared.");
}
