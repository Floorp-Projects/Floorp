// Bug 380852 - Delete permission manager entries in Clear Recent History

var tempScope = {};
Services.scriptloader.loadSubScript("chrome://browser/content/sanitize.js", tempScope);
var Sanitizer = tempScope.Sanitizer;

function countPermissions() {
  let result = 0;
  let enumerator = Services.perms.enumerator;
  while (enumerator.hasMoreElements()) {
    result++;
    enumerator.getNext();
  }
  return result;
}

add_task(async function test() {
  // sanitize before we start so we have a good baseline.
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
  s.sanitize();

  // Count how many permissions we start with - some are defaults that
  // will not be sanitized.
  let numAtStart = countPermissions();

  // Add a permission entry
  var pm = Services.perms;
  pm.add(makeURI("http://example.com"), "testing", pm.ALLOW_ACTION);

  // Sanity check
  ok(pm.enumerator.hasMoreElements(), "Permission manager should have elements, since we just added one");

  // Clear it
  await s.sanitize();

  // Make sure it's gone
  is(numAtStart, countPermissions(), "Permission manager should have the same count it started with");
});
