/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let gTab, gPanel;

function test() {
  waitForExplicitFinish();

  setUp(URL, function onSetUp(tab, browser, panel) {
    gTab = tab;
    gPanel = panel;

    panel.once("profileCreated", onProfileCreated);
    panel.once("profileSwitched", onProfileSwitched);

    testNewProfile();
  });
}

function testNewProfile() {
  is(gPanel.profiles.size, 1, "There is only one profile");

  let btn = gPanel.document.getElementById("profiler-create");
  ok(!btn.getAttribute("disabled"), "Create Profile button is not disabled");
  btn.click();
}

function onProfileCreated(name, uid) {
  is(gPanel.profiles.size, 2, "There are two profiles now");
  ok(gPanel.activeProfile.uid !== uid, "New profile is not yet active");

  let btn = gPanel.document.getElementById("profile-" + uid);
  ok(btn, "Profile item has been added to the sidebar");
  btn.click();
}

function onProfileSwitched(name, uid) {
  ok(gPanel.activeProfile.uid === uid, "Switched to a new profile");

  tearDown(gTab, function onTearDown() {
    gPanel = null;
    gTab = null;
  });
}