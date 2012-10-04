/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    workerURL: "https://example.com/browser/browser/base/content/test/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

var tests = {
  testProfileSet: function(next) {
    let profile = {
      portrait: "https://example.com/portrait.jpg",
      userName: "trickster",
      displayName: "Kuma Lisa",
      profileURL: "http://en.wikipedia.org/wiki/Kuma_Lisa"
    }
    Social.provider.updateUserProfile(profile);
    // check dom values
    let portrait = document.getElementById("social-statusarea-user-portrait").getAttribute("src");
    is(profile.portrait, portrait, "portrait is set");
    let userButton = document.getElementById("social-statusarea-username");
    ok(!userButton.hidden, "username is visible");
    is(userButton.label, profile.userName, "username is set");
    next();
  },
  testAmbientNotifications: function(next) {
    let ambience = {
      name: "testIcon",
      iconURL: "chrome://branding/content/icon48.png",
      contentPanel: "about:blank",
      counter: 42
    };
    Social.provider.setAmbientNotification(ambience);

    let statusIcon = document.querySelector("#social-toolbar-item > box");
    waitForCondition(function() {
      statusIcon = document.querySelector("#social-toolbar-item > box");
      return !!statusIcon;
    }, function () {
      let statusIconLabel = statusIcon.querySelector("label");
      is(statusIconLabel.value, "42", "status value is correct");

      ambience.counter = 0;
      Social.provider.setAmbientNotification(ambience);
      is(statusIconLabel.value, "", "status value is correct");
      next();
    }, "statusIcon was never found");
  },
  testProfileUnset: function(next) {
    Social.provider.updateUserProfile({});
    // check dom values
    let userButton = document.getElementById("social-statusarea-username");
    ok(userButton.hidden, "username is not visible");
    let ambientIcons = document.querySelectorAll("#social-toolbar-item > box");
    for (let ambientIcon of ambientIcons) {
      ok(ambientIcon.collapsed, "ambient icon (" + ambientIcon.id + ") is collapsed");
    }
    
    next();
  }
}

