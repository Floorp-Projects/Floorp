/**
 * This test will check the Autoplay Policy Detection API for click-to-play
 * blocking policy (media.autoplay.blocking_policy=2) and the blocked value set
 * to BLOCKED (block audible) and BLOCKED_ALL (block audible & inaudible).
 *
 * We will create two video elements in the test page, and then click one of
 * them. After doing that, only the element has been clicked can be allowed to
 * autoplay, other elements should remain blocked depend on the default blocking
 * value.
 */
"use strict";

// TODO : remove this when it's enabled by default in bug 1812189.
add_setup(async function setSharedPrefs() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.media.autoplay-policy-detection.enabled", true]],
  });
});

async function testAutoplayPolicy(defaultPolicy) {
  await setupTestPref(defaultPolicy);
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  await createVideoElements(tab);
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [defaultPolicy],
    defaultPolicy => {
      is(
        content.navigator.getAutoplayPolicy("mediaelement"),
        defaultPolicy,
        "Check autoplay policy by media element type is correct"
      );
      let videos = content.document.getElementsByTagName("video");
      for (let video of videos) {
        is(
          content.navigator.getAutoplayPolicy(video),
          defaultPolicy,
          "Check autoplay policy by element is correct"
        );
      }
    }
  );

  info("click on one video to make it play");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#will-be-clicked",
    { button: 0 },
    tab.linkedBrowser
  );

  info("only the element has been clicked can be allowed to autoplay");
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [defaultPolicy],
    defaultPolicy => {
      is(
        content.navigator.getAutoplayPolicy("mediaelement"),
        defaultPolicy,
        "Check autoplay policy by media element type is correct"
      );
      let videos = content.document.getElementsByTagName("video");
      for (let video of videos) {
        is(
          content.navigator.getAutoplayPolicy(video),
          video.id === "will-be-clicked" ? "allowed" : defaultPolicy,
          "Check autoplay policy by element is correct"
        );
      }
    }
  );

  BrowserTestUtils.removeTab(tab);
}

add_task(async function testAutoplayPolicyDetectionForClickToPlay() {
  await testAutoplayPolicy("allowed-muted");
  await testAutoplayPolicy("disallowed");
});

// Following are helper functions
async function setupTestPref(defaultPolicy) {
  function policyToBlockedValue(defaultPolicy) {
    // Value for media.autoplay.default
    if (defaultPolicy === "allowed") {
      return 0 /* Allowed */;
    } else if (defaultPolicy === "allowed-muted") {
      return 1 /* Blocked */;
    }
    return 5 /* Blocked All */;
  }
  const defaultBlocked = policyToBlockedValue(defaultPolicy);
  info(`Set 'media.autoplay.default' to ${defaultBlocked}`);
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", defaultBlocked],
      ["media.autoplay.blocking_policy", 2 /* click-to-play */],
    ],
  });
}

function createVideoElements(tab) {
  info("create two video elements in the page");
  let url = GetTestWebBasedURL("gizmo.mp4");
  return SpecialPowers.spawn(tab.linkedBrowser, [url], url => {
    let video1 = content.document.createElement("video");
    video1.id = "will-be-clicked";
    video1.controls = true;
    video1.src = url;

    let video2 = content.document.createElement("video");
    video2.controls = true;
    video2.src = url;

    content.document.body.appendChild(video1);
    content.document.body.appendChild(video2);
  });
}
