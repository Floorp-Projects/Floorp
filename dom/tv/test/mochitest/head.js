"use strict";

function setupPrefsAndPermissions(aCallback) {
  setupPrefs(function() {
      SpecialPowers.pushPermissions([
        {"type":"tv", "allow":1, "context":document}
      ], aCallback);
  });
}

function setupPrefs(aCallback) {
  SpecialPowers.pushPrefEnv({"set": [
                              ["dom.tv.enabled", true],
                              ["dom.ignore_webidl_scope_checks", true],
                            ]}, function() {
    aCallback();
  });
}

function removePrefsAndPermissions(aCallback) {
  SpecialPowers.popPrefEnv(function() {
    SpecialPowers.popPermissions(aCallback);
  });
}

function prepareTest(aCallback) {
  removePrefsAndPermissions(function() {
    setupPrefsAndPermissions(function() {
      initMockedData(aCallback);
    });
  });
}

function initMockedData(aCallback) {
  let xhr = new XMLHttpRequest;
  xhr.open("GET", "./mock_data.json", false);
  xhr.send(null);
  if (xhr.status == 200) {
    var gScript = SpecialPowers.loadChromeScript(SimpleTest.getTestFileURL('tv_chrome_script.js'));

    gScript.addMessageListener('init-mocked-data-complete', aCallback);

    gScript.sendAsyncMessage('init-mocked-data', xhr.responseText);
  }
}
