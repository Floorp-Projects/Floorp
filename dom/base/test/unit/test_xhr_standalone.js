/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test setting .responseType and .withCredentials is allowed
// in non-window non-Worker context

function run_test() {
  Services.prefs.setBoolPref(
    "network.fetch.systemDefaultsToOmittingCredentials",
    false
  );
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "data:,", false);
  var exceptionThrown = false;
  try {
    xhr.responseType = "";
    xhr.withCredentials = false;
  } catch (e) {
    console.error(e);
    exceptionThrown = true;
  }
  Assert.equal(false, exceptionThrown);
}
