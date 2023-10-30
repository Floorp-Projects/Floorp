/**
 * This test checks whether Autoplay Policy Detection API works correctly under
 * different situations of having global permission set for block autoplay.
 * This test only checks the sticky user gesture blocking model.
 */
"use strict";

add_setup(async function setSharedPrefs() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.media.autoplay-policy-detection.enabled", true],
      ["media.autoplay.blocking_policy", 0],
    ],
  });
});

add_task(async function testGlobalPermissionIsAllowed() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.ALLOWED]],
  });
  let tab = await createTabAndSetupPolicyAssertFunc("about:blank");
  await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    info("global setting allows any autoplay");
    content.assertAutoplayPolicy({
      resultForElementType: "allowed",
      resultForElement: "allowed",
      resultForContextType: "allowed",
      resultForContext: "allowed",
    });
  });
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testGlobalPermissionIsBlocked() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED]],
  });
  let tab = await createTabAndSetupPolicyAssertFunc("about:blank");
  await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    info(
      "global setting allows inaudible autoplay but audible autoplay is still not allowed"
    );
    content.assertAutoplayPolicy({
      resultForElementType: "allowed-muted",
      resultForElement: "allowed-muted",
      resultForContextType: "disallowed",
      resultForContext: "disallowed",
    });

    info("tweaking video's muted attribute won't change the result");
    content.video.muted = true;
    is(
      "allowed-muted",
      content.navigator.getAutoplayPolicy(content.video),
      "getAutoplayPolicy(video) returns correct value"
    );
    content.video.muted = false;
    is(
      "allowed-muted",
      content.navigator.getAutoplayPolicy(content.video),
      "getAutoplayPolicy(video) returns correct value"
    );

    info(
      "activate document by using user gesture, all autoplay will be allowed"
    );
    content.document.notifyUserGestureActivation();
    content.assertAutoplayPolicy({
      resultForElementType: "allowed",
      resultForElement: "allowed",
      resultForContextType: "allowed",
      resultForContext: "allowed",
    });
  });
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testGlobalPermissionIsBlockedAll() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED_ALL]],
  });
  let tab = await createTabAndSetupPolicyAssertFunc("about:blank");
  await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    info("global setting doesn't allow any autoplay");
    content.assertAutoplayPolicy({
      resultForElementType: "disallowed",
      resultForElement: "disallowed",
      resultForContextType: "disallowed",
      resultForContext: "disallowed",
    });

    info(
      "activate document by using user gesture, all autoplay will be allowed"
    );
    content.document.notifyUserGestureActivation();
    content.assertAutoplayPolicy({
      resultForElementType: "allowed",
      resultForElement: "allowed",
      resultForContextType: "allowed",
      resultForContext: "allowed",
    });
  });
  BrowserTestUtils.removeTab(tab);
});
