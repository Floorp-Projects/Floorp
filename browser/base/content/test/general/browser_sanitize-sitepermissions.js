// Bug 380852 - Delete permission manager entries in Clear Recent History

let tempScope = {};
Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader)
                                           .loadSubScript("chrome://browser/content/sanitize.js", tempScope);
let Sanitizer = tempScope.Sanitizer;

function test() {
  
  // Add a permission entry
  var pm = Services.perms;
  pm.add(makeURI("http://example.com"), "testing", pm.ALLOW_ACTION);
  
  // Sanity check
  ok(pm.enumerator.hasMoreElements(), "Permission manager should have elements, since we just added one");
  
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
  s.sanitize();
  
  // Make sure it's gone
  ok(!pm.enumerator.hasMoreElements(), "Permission manager shouldn't have entries after Sanitizing");
}
