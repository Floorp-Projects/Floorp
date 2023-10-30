/**
 * This test checks whether Autoplay Policy Detection API works correctly under
 * different situations of having global permission set for block autoplay
 * along with different site permission setting. This test only checks the
 * sticky user gesture blocking model.
 */
"use strict";

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

// We can't set site permission on 'about:blank' so we use an empty page.
const PAGE_URL = GetTestWebBasedURL("file_empty.html");

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
  let tab = await createTabAndSetupPolicyAssertFunc(PAGE_URL);
  PermissionTestUtils.add(
    tab.linkedBrowser.currentURI,
    "autoplay-media",
    Services.perms.DENY_ACTION
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    info("site permission blocks audible autoplay");
    content.assertAutoplayPolicy({
      resultForElementType: "allowed-muted",
      resultForElement: "allowed-muted",
      resultForContextType: "disallowed",
      resultForContext: "disallowed",
    });
  });
  PermissionTestUtils.add(
    tab.linkedBrowser.currentURI,
    "autoplay-media",
    Ci.nsIAutoplay.BLOCKED_ALL
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    info("site permission blocks all autoplay");
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
  PermissionTestUtils.remove(tab.linkedBrowser.currentURI, "autoplay-media");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testGlobalPermissionIsBlocked() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED]],
  });
  let tab = await createTabAndSetupPolicyAssertFunc(PAGE_URL);
  PermissionTestUtils.add(
    tab.linkedBrowser.currentURI,
    "autoplay-media",
    Services.perms.ALLOW_ACTION
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    info("site permission allows all autoplay");
    content.assertAutoplayPolicy({
      resultForElementType: "allowed",
      resultForElement: "allowed",
      resultForContextType: "allowed",
      resultForContext: "allowed",
    });
  });
  PermissionTestUtils.add(
    tab.linkedBrowser.currentURI,
    "autoplay-media",
    Ci.nsIAutoplay.BLOCKED_ALL
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    info("site permission blocks all autoplay");
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
  PermissionTestUtils.remove(tab.linkedBrowser.currentURI, "autoplay-media");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testGlobalPermissionIsBlockedAll() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED_ALL]],
  });
  let tab = await createTabAndSetupPolicyAssertFunc(PAGE_URL);
  PermissionTestUtils.add(
    tab.linkedBrowser.currentURI,
    "autoplay-media",
    Services.perms.ALLOW_ACTION
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    info("site permission allows all autoplay");
    content.assertAutoplayPolicy({
      resultForElementType: "allowed",
      resultForElement: "allowed",
      resultForContextType: "allowed",
      resultForContext: "allowed",
    });
  });
  PermissionTestUtils.add(
    tab.linkedBrowser.currentURI,
    "autoplay-media",
    Services.perms.DENY_ACTION
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    info("site permission blocks audible autoplay");
    content.assertAutoplayPolicy({
      resultForElementType: "allowed-muted",
      resultForElement: "allowed-muted",
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
  PermissionTestUtils.remove(tab.linkedBrowser.currentURI, "autoplay-media");
  BrowserTestUtils.removeTab(tab);
});
