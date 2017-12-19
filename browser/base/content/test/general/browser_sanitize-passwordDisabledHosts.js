// Bug 474792 - Clear "Never remember passwords for this site" when
// clearing site-specific settings in Clear Recent History dialog

var tempScope = {};
Services.scriptloader.loadSubScript("chrome://browser/content/sanitize.js", tempScope);
var Sanitizer = tempScope.Sanitizer;

add_task(async function() {
  // getLoginSavingEnabled always returns false if password capture is disabled.
  await SpecialPowers.pushPrefEnv({"set": [["signon.rememberSignons", true]]});

  // Add a disabled host
  Services.logins.setLoginSavingEnabled("http://example.com", false);
  // Sanity check
  is(Services.logins.getLoginSavingEnabled("http://example.com"), false,
     "example.com should be disabled for password saving since we haven't cleared that yet.");

  // Set up the sanitizer to just clear siteSettings
  let s = new Sanitizer();
  s.ignoreTimespan = false;
  s.prefDomain = "privacy.cpd.";
  var itemPrefs = Services.prefs.getBranch(s.prefDomain);
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
  await s.sanitize();

  // Make sure it's gone
  is(Services.logins.getLoginSavingEnabled("http://example.com"), true,
     "example.com should be enabled for password saving again now that we've cleared.");

  await SpecialPowers.popPrefEnv();
});
