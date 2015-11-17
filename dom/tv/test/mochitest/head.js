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
                                     ["dom.ignore_webidl_scope_checks", true]]}, function() {
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
