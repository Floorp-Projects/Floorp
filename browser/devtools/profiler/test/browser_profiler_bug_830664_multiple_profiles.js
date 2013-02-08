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

function sendFromActiveProfile(msg) {
  let [win, doc] = getProfileInternals();

  win.parent.postMessage({
    uid: gPanel.activeProfile.uid,
    status: msg
  }, "*");
}

function startProfiling() {
  gPanel.profiles.get(gUid).once("disabled", stopProfiling);
  sendFromActiveProfile("start");
}

function stopProfiling() {
  let [win, doc] = getProfileInternals(gUid);
  let [btn, msg] = getCleoControls(doc);

  ok(msg.textContent.match("Profile 1") !== null, "Message is visible");
  ok(btn.hasAttribute("disabled"), "Button is disabled");

  is(gPanel.document.querySelector("li#profile-1 > h1").textContent,
    "Profile 1 *", "Profile 1 has a star next to it.");
  is(gPanel.document.querySelector("li#profile-2 > h1").textContent,
    "Profile 2", "Profile 2 doesn't have a star next to it.");

  gPanel.profiles.get(gUid).once("enabled", confirmAndFinish);
  sendFromActiveProfile("stop");
}

function confirmAndFinish() {
  let [win, doc] = getProfileInternals(gUid);
  let [btn, msg] = getCleoControls(doc);

  ok(msg.style.display === "none", "Message is hidden");
  ok(!btn.hasAttribute("disabled"), "Button is enabled");

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