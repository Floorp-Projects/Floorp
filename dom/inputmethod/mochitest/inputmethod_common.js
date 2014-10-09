function inputmethod_setup(callback) {
  SimpleTest.waitForExplicitFinish();
  SimpleTest.requestCompleteLog();
  let appInfo = SpecialPowers.Cc['@mozilla.org/xre/app-info;1']
                .getService(SpecialPowers.Ci.nsIXULAppInfo);
  if (appInfo.name != 'B2G') {
    SpecialPowers.Cu.import("resource://gre/modules/Keyboard.jsm", this);
  }

  let permissions = [];
  ['input-manage', 'browser'].forEach(function(name) {
    permissions.push({
      type: name,
      allow: true,
      context: document
    });
  });

  SpecialPowers.pushPermissions(permissions, function() {
    let prefs = [
      ['dom.mozBrowserFramesEnabled', true],
      // Enable navigator.mozInputMethod.
      ['dom.mozInputMethod.enabled', true],
      // Bypass the permission check for mozInputMethod API.
      ['dom.mozInputMethod.testing', true]
    ];
    SpecialPowers.pushPrefEnv({set: prefs}, function() {
      SimpleTest.waitForFocus(callback);
    });
  });
}

function inputmethod_cleanup() {
  SpecialPowers.wrap(navigator.mozInputMethod).setActive(false);
  SimpleTest.finish();
}
