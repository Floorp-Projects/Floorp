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
  is(getSidebarItem(1).attachment.state, PROFILE_RUNNING);
  is(getSidebarItem(2).attachment.state, PROFILE_RUNNING);

  gPanel.profiles.get(gPanel.activeProfile.uid).once("stopped", function () {
    is(getSidebarItem(1).attachment.state, PROFILE_COMPLETED);

    sendFromProfile(2, "stop");
    gPanel.profiles.get(2).once("stopped", confirmAndFinish);
  });

  sendFromProfile(gPanel.activeProfile.uid, "stop");
}

function confirmAndFinish(ev, data) {
  is(getSidebarItem(1).attachment.state, PROFILE_COMPLETED);
  is(getSidebarItem(2).attachment.state, PROFILE_COMPLETED);

  tearDown(gTab, function onTearDown() {
    gPanel = null;
    gTab = null;
    gUid = null;
  });
}
