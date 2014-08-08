/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This is an integration test from navigator.mozLoop through to the end
 * effects - rather than just testing MozLoopAPI alone.
 */

add_task(loadLoopPanel);

add_task(function* test_mozLoop_appVersionInfo() {
  Assert.ok(gMozLoopAPI, "mozLoop should exist");

  let appVersionInfo = gMozLoopAPI.appVersionInfo;

  Assert.ok(appVersionInfo, "should have appVersionInfo");

  Assert.equal(appVersionInfo.channel,
               Services.prefs.getCharPref("app.update.channel"),
               "appVersionInfo.channel should match the application channel");

  var appInfo = Cc["@mozilla.org/xre/app-info;1"]
                  .getService(Ci.nsIXULAppInfo)
                  .QueryInterface(Ci.nsIXULRuntime);

  Assert.equal(appVersionInfo.version,
               appInfo.version,
               "appVersionInfo.version should match the application version");

  Assert.equal(appVersionInfo.OS,
               appInfo.OS,
               "appVersionInfo.os should match the running os");
});
