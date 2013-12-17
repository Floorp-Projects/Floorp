function inputmethod_setup(callback) {
  SimpleTest.waitForExplicitFinish();
  SpecialPowers.Cu.import("resource://gre/modules/Keyboard.jsm", this);

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
    SpecialPowers.pushPrefEnv({set: prefs}, callback);
  });
}

function inputmethod_cleanup() {
  SimpleTest.finish();
}
