// Bug 380852 - Delete permission manager entries in Clear Recent History

Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Components.interfaces.mozIJSSubScriptLoader)
                                           .loadSubScript("chrome://browser/content/sanitize.js");

function test() {
  
  // Add a permission entry
  var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                    .getService(Components.interfaces.nsIPermissionManager);
  pm.add(uri("http://example.com"), "testing", Ci.nsIPermissionManager.ALLOW_ACTION);
  
  // Sanity check
  ok(pm.enumerator.hasMoreElements(), "Permission manager should have elements, since we just added one");
  
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
  ok(!pm.enumerator.hasMoreElements(), "Permission manager shouldn't have entries after Sanitizing");
}

function uri(spec) {
  return Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService)
                                                .newURI(spec, null, null);
}
