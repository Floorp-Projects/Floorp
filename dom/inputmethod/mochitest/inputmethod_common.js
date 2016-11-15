function inputmethod_setup(callback) {
  SimpleTest.waitForExplicitFinish();
  SimpleTest.requestCompleteLog();
  let appInfo = SpecialPowers.Cc['@mozilla.org/xre/app-info;1']
                .getService(SpecialPowers.Ci.nsIXULAppInfo);
  if (appInfo.name != 'B2G') {
    SpecialPowers.Cu.import("resource://gre/modules/Keyboard.jsm", this);
  }

  let prefs = [
    ['dom.mozBrowserFramesEnabled', true],
    ['network.disable.ipc.security', true],
    // Enable navigator.mozInputMethod.
    ['dom.mozInputMethod.enabled', true]
  ];
  SpecialPowers.pushPrefEnv({set: prefs}, function() {
    SimpleTest.waitForFocus(callback);
  });
}

function inputmethod_cleanup() {
  SpecialPowers.wrap(navigator.mozInputMethod).setActive(false);
  SimpleTest.finish();
}
