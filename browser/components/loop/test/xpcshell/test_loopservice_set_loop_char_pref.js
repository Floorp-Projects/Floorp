/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/*global XPCOMUtils, Services, Assert */

var fakePrefName = "color";
var fakePrefValue = "green";

function test_setLoopCharPref()
{
  Services.prefs.setCharPref("loop." + fakePrefName, "red");
  MozLoopService.setLoopCharPref(fakePrefName, fakePrefValue);

  var returnedPref = Services.prefs.getCharPref("loop." + fakePrefName);

  Assert.equal(returnedPref, fakePrefValue,
    "Should set a char pref under the loop. branch");
  Services.prefs.clearUserPref("loop." + fakePrefName);
}

function test_setLoopCharPref_new()
{
  Services.prefs.clearUserPref("loop." + fakePrefName);
  MozLoopService.setLoopCharPref(fakePrefName, fakePrefValue);

  var returnedPref = Services.prefs.getCharPref("loop." + fakePrefName);

  Assert.equal(returnedPref, fakePrefValue,
               "Should set a new char pref under the loop. branch");
  Services.prefs.clearUserPref("loop." + fakePrefName);
}

function test_setLoopCharPref_non_coercible_type()
{
  MozLoopService.setLoopCharPref(fakePrefName, true);

  ok(true, "Setting non-coercible type should not fail");
}


function run_test()
{
  test_setLoopCharPref();
  test_setLoopCharPref_new();
  test_setLoopCharPref_non_coercible_type();

  do_register_cleanup(function() {
    Services.prefs.clearUserPref("loop." + fakePrefName);
  });
}
