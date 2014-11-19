/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/*global XPCOMUtils, Services, Assert */

var fakeCharPrefName = "color";
var fakeBoolPrefName = "boolean";
var fakePrefValue = "green";

function test_getLoopPref()
{
  Services.prefs.setCharPref("loop." + fakeCharPrefName, fakePrefValue);

  var returnedPref = MozLoopService.getLoopPref(fakeCharPrefName, Ci.nsIPrefBranch.PREF_STRING);

  Assert.equal(returnedPref, fakePrefValue,
    "Should return a char pref under the loop. branch");
  Services.prefs.clearUserPref("loop." + fakeCharPrefName);
}

function test_getLoopPref_not_found()
{
  var returnedPref = MozLoopService.getLoopPref(fakeCharPrefName);

  Assert.equal(returnedPref, null,
    "Should return null if a preference is not found");
}

function test_getLoopPref_non_coercible_type()
{
  Services.prefs.setBoolPref("loop." + fakeCharPrefName, false);

  var returnedPref = MozLoopService.getLoopPref(fakeCharPrefName, Ci.nsIPrefBranch.PREF_STRING);

  Assert.equal(returnedPref, null,
    "Should return null if the preference exists & is of a non-coercible type");
}

function test_setLoopPref()
{
  Services.prefs.setCharPref("loop." + fakeCharPrefName, "red");
  MozLoopService.setLoopPref(fakeCharPrefName, fakePrefValue);

  var returnedPref = Services.prefs.getCharPref("loop." + fakeCharPrefName);

  Assert.equal(returnedPref, fakePrefValue,
    "Should set a char pref under the loop. branch");
  Services.prefs.clearUserPref("loop." + fakeCharPrefName);
}

function test_setLoopPref_new()
{
  Services.prefs.clearUserPref("loop." + fakeCharPrefName);
  MozLoopService.setLoopPref(fakeCharPrefName, fakePrefValue, Ci.nsIPrefBranch.PREF_STRING);

  var returnedPref = Services.prefs.getCharPref("loop." + fakeCharPrefName);

  Assert.equal(returnedPref, fakePrefValue,
               "Should set a new char pref under the loop. branch");
  Services.prefs.clearUserPref("loop." + fakeCharPrefName);
}

function test_setLoopPref_non_coercible_type()
{
  MozLoopService.setLoopPref(fakeCharPrefName, true);

  ok(true, "Setting non-coercible type should not fail");
}


function test_getLoopPref_bool()
{
  Services.prefs.setBoolPref("loop." + fakeBoolPrefName, true);

  var returnedPref = MozLoopService.getLoopPref(fakeBoolPrefName);

  Assert.equal(returnedPref, true,
    "Should return a bool pref under the loop. branch");
  Services.prefs.clearUserPref("loop." + fakeBoolPrefName);
}

function test_getLoopPref_not_found_bool()
{
  var returnedPref = MozLoopService.getLoopPref(fakeBoolPrefName);

  Assert.equal(returnedPref, null,
    "Should return null if a preference is not found");
}


function run_test()
{
  setupFakeLoopServer();

  test_getLoopPref();
  test_getLoopPref_not_found();
  test_getLoopPref_non_coercible_type();
  test_setLoopPref();
  test_setLoopPref_new();
  test_setLoopPref_non_coercible_type();

  test_getLoopPref_bool();
  test_getLoopPref_not_found_bool();

  do_register_cleanup(function() {
    Services.prefs.clearUserPref("loop." + fakeCharPrefName);
    Services.prefs.clearUserPref("loop." + fakeBoolPrefName);
  });
}
