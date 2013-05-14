/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let gTab, gPanel, gUid;

function test() {
  waitForExplicitFinish();

  setUp(URL, function onSetUp(tab, browser, panel) {
    gTab = tab;
    gPanel = panel;

    gPanel.once("profileCreated", function (_, uid) {
      gUid = uid;
      let profile = gPanel.profiles.get(uid);

      if (profile.isReady) {
        startProfiling();
      } else {
        profile.once("ready", startProfiling);
      }
    });
    gPanel.createProfile();
  });
}

function getCleoControls(doc) {
  return [
    doc.querySelector("#startWrapper button"),
    doc.querySelector("#profilerMessage")
  ];
}

function startProfiling() {
  gPanel.profiles.get(gPanel.activeProfile.uid).once("started", function () {
    setTimeout(function () {
      sendFromProfile(2, "start");
      gPanel.profiles.get(2).once("started", function () setTimeout(stopProfiling, 50));
    }, 50);
  });
  sendFromProfile(gPanel.activeProfile.uid, "start");
}

function stopProfiling() {
  let [win, doc] = getProfileInternals(gUid);
  let [btn, msg] = getCleoControls(doc);

  is(gPanel.document.querySelector("li#profile-1 > h1").textContent,
    "Profile 1 *", "Profile 1 has a star next to it.");
  is(gPanel.document.querySelector("li#profile-2 > h1").textContent,
    "Profile 2 *", "Profile 2 has a star next to it.");

  gPanel.profiles.get(gPanel.activeProfile.uid).once("stopped", function () {
    is(gPanel.document.querySelector("li#profile-1 > h1").textContent,
      "Profile 1", "Profile 1 doesn't have a star next to it anymore.");

    sendFromProfile(2, "stop");
    gPanel.profiles.get(2).once("stopped", confirmAndFinish);
  });
  sendFromProfile(gPanel.activeProfile.uid, "stop");
}

function confirmAndFinish(ev, data) {
  let [win, doc] = getProfileInternals(gUid);
  let [btn, msg] = getCleoControls(doc);

  is(gPanel.document.querySelector("li#profile-1 > h1").textContent,
    "Profile 1", "Profile 1 doesn't have a star next to it.");
  is(gPanel.document.querySelector("li#profile-2 > h1").textContent,
    "Profile 2", "Profile 2 doesn't have a star next to it.");  

  tearDown(gTab, function onTearDown() {
    gPanel = null;
    gTab = null;
    gUid = null;
  });
}
