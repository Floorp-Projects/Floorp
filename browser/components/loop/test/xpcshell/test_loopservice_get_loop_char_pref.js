/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/*global XPCOMUtils, Services, Assert */

var fakePrefName = "color";
var fakePrefValue = "green";

function test_getLoopCharPref()
{
  Services.prefs.setCharPref("loop." + fakePrefName, fakePrefValue);

  var returnedPref = MozLoopService.getLoopCharPref(fakePrefName);

  Assert.equal(returnedPref, fakePrefValue,
    "Should return a char pref under the loop. branch");
  Services.prefs.clearUserPref("loop." + fakePrefName);
}

function test_getLoopCharPref_not_found()
{
  var returnedPref = MozLoopService.getLoopCharPref(fakePrefName);

  Assert.equal(returnedPref, null,
    "Should return null if a preference is not found");
}

function test_getLoopCharPref_non_coercible_type()
{
  Services.prefs.setBoolPref("loop." + fakePrefName, false );

  var returnedPref = MozLoopService.getLoopCharPref(fakePrefName);

  Assert.equal(returnedPref, null,
    "Should return null if the preference exists & is of a non-coercible type");
}


function run_test()
{
  test_getLoopCharPref();
  test_getLoopCharPref_not_found();
  test_getLoopCharPref_non_coercible_type();

  do_register_cleanup(function() {
    Services.prefs.clearUserPref("loop." + fakePrefName);
  });
}
