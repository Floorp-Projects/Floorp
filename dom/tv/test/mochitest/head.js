"use strict";

function setupPrefsAndPermissions(callback) {
  setupPrefs(function() {
      SpecialPowers.pushPermissions([
        {"type":"tv", "allow":1, "context":document}
      ], callback);
  });
}

function setupPrefs(callback) {
  SpecialPowers.pushPrefEnv({"set": [["dom.tv.enabled", true],
                                     ["dom.testing.tv_enabled_for_hosted_apps", true]]}, function() {
    callback();
  });
}

function removePrefsAndPermissions(callback) {
  SpecialPowers.popPrefEnv(function() {
    SpecialPowers.popPermissions(callback);
  });
}

function prepareTest(callback) {
  removePrefsAndPermissions(function() {
    setupPrefsAndPermissions(callback);
  });
}
