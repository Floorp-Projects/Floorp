/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/*global XPCOMUtils, Services, Assert */

var fakeCharPrefName = "color";
var fakeBoolPrefName = "boolean";
var fakePrefValue = "green";

function test_getLoopCharPref()
{
  Services.prefs.setCharPref("loop." + fakeCharPrefName, fakePrefValue);

  var returnedPref = MozLoopService.getLoopCharPref(fakeCharPrefName);

  Assert.equal(returnedPref, fakePrefValue,
    "Should return a char pref under the loop. branch");
  Services.prefs.clearUserPref("loop." + fakeCharPrefName);
}

function test_getLoopCharPref_not_found()
{
  var returnedPref = MozLoopService.getLoopCharPref(fakeCharPrefName);

  Assert.equal(returnedPref, null,
    "Should return null if a preference is not found");
}

function test_getLoopCharPref_non_coercible_type()
{
  Services.prefs.setBoolPref("loop." + fakeCharPrefName, false);

  var returnedPref = MozLoopService.getLoopCharPref(fakeCharPrefName);

  Assert.equal(returnedPref, null,
    "Should return null if the preference exists & is of a non-coercible type");
}

function test_setLoopCharPref()
{
  Services.prefs.setCharPref("loop." + fakeCharPrefName, "red");
  MozLoopService.setLoopCharPref(fakeCharPrefName, fakePrefValue);

  var returnedPref = Services.prefs.getCharPref("loop." + fakeCharPrefName);

  Assert.equal(returnedPref, fakePrefValue,
    "Should set a char pref under the loop. branch");
  Services.prefs.clearUserPref("loop." + fakeCharPrefName);
}

function test_setLoopCharPref_new()
{
  Services.prefs.clearUserPref("loop." + fakeCharPrefName);
  MozLoopService.setLoopCharPref(fakeCharPrefName, fakePrefValue);

  var returnedPref = Services.prefs.getCharPref("loop." + fakeCharPrefName);

  Assert.equal(returnedPref, fakePrefValue,
               "Should set a new char pref under the loop. branch");
  Services.prefs.clearUserPref("loop." + fakeCharPrefName);
}

function test_setLoopCharPref_non_coercible_type()
{
  MozLoopService.setLoopCharPref(fakeCharPrefName, true);

  ok(true, "Setting non-coercible type should not fail");
}


function test_getLoopBoolPref()
{
  Services.prefs.setBoolPref("loop." + fakeBoolPrefName, true);

  var returnedPref = MozLoopService.getLoopBoolPref(fakeBoolPrefName);

  Assert.equal(returnedPref, true,
    "Should return a bool pref under the loop. branch");
  Services.prefs.clearUserPref("loop." + fakeBoolPrefName);
}

function test_getLoopBoolPref_not_found()
{
  var returnedPref = MozLoopService.getLoopBoolPref(fakeBoolPrefName);

  Assert.equal(returnedPref, null,
    "Should return null if a preference is not found");
}


function run_test()
{
  test_getLoopCharPref();
  test_getLoopCharPref_not_found();
  test_getLoopCharPref_non_coercible_type();
  test_setLoopCharPref();
  test_setLoopCharPref_new();
  test_setLoopCharPref_non_coercible_type();

  test_getLoopBoolPref();
  test_getLoopBoolPref_not_found();

  do_register_cleanup(function() {
    Services.prefs.clearUserPref("loop." + fakeCharPrefName);
    Services.prefs.clearUserPref("loop." + fakeBoolPrefName);
  });
}
