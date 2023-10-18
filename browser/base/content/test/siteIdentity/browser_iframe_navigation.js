/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the site identity icon and related machinery reflects the correct
// security state after navigating an iframe in various contexts.
// See bug 1490982.

const ROOT_URI = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const SECURE_TEST_URI = ROOT_URI + "iframe_navigation.html";
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const INSECURE_TEST_URI = SECURE_TEST_URI.replace("https://", "http://");

const NOT_SECURE_LABEL = Services.prefs.getBoolPref(
  "security.insecure_connection_text.enabled"
)
  ? "notSecure notSecureText"
  : "notSecure";

// From a secure URI, navigate the iframe to about:blank (should still be
// secure).
add_task(async function () {
  let uri = SECURE_TEST_URI + "#blank";
  await BrowserTestUtils.withNewTab(uri, async browser => {
    let identityMode = window.document.getElementById("identity-box").className;
    is(identityMode, "verifiedDomain", "identity should be secure before");

    await SpecialPowers.spawn(browser, [], async () => {
      content.postMessage("", "*"); // This kicks off the navigation.
      await ContentTaskUtils.waitForCondition(() => {
        return !content.document.body.classList.contains("running");
      });
    });

    let newIdentityMode =
      window.document.getElementById("identity-box").className;
    is(newIdentityMode, "verifiedDomain", "identity should be secure after");
  });
});

// From a secure URI, navigate the iframe to an insecure URI (http://...)
// (mixed active content should be blocked, should still be secure).
add_task(async function () {
  let uri = SECURE_TEST_URI + "#insecure";
  await BrowserTestUtils.withNewTab(uri, async browser => {
    let identityMode = window.document.getElementById("identity-box").className;
    is(identityMode, "verifiedDomain", "identity should be secure before");

    await SpecialPowers.spawn(browser, [], async () => {
      content.postMessage("", "*"); // This kicks off the navigation.
      await ContentTaskUtils.waitForCondition(() => {
        return !content.document.body.classList.contains("running");
      });
    });

    let newIdentityMode =
      window.document.getElementById("identity-box").classList;
    ok(
      newIdentityMode.contains("mixedActiveBlocked"),
      "identity should be blocked mixed active content after"
    );
    ok(
      newIdentityMode.contains("verifiedDomain"),
      "identity should still contain 'verifiedDomain'"
    );
    is(newIdentityMode.length, 2, "shouldn't have any other identity states");
  });
});

// From an insecure URI (http://..), navigate the iframe to about:blank (should
// still be insecure).
add_task(async function () {
  let uri = INSECURE_TEST_URI + "#blank";
  await BrowserTestUtils.withNewTab(uri, async browser => {
    let identityMode = window.document.getElementById("identity-box").className;
    is(
      identityMode,
      NOT_SECURE_LABEL,
      "identity should be 'not secure' before"
    );

    await SpecialPowers.spawn(browser, [], async () => {
      content.postMessage("", "*"); // This kicks off the navigation.
      await ContentTaskUtils.waitForCondition(() => {
        return !content.document.body.classList.contains("running");
      });
    });

    let newIdentityMode =
      window.document.getElementById("identity-box").className;
    is(
      newIdentityMode,
      NOT_SECURE_LABEL,
      "identity should be 'not secure' after"
    );
  });
});

// From an insecure URI (http://..), navigate the iframe to a secure URI
// (https://...) (should still be insecure).
add_task(async function () {
  let uri = INSECURE_TEST_URI + "#secure";
  await BrowserTestUtils.withNewTab(uri, async browser => {
    let identityMode = window.document.getElementById("identity-box").className;
    is(
      identityMode,
      NOT_SECURE_LABEL,
      "identity should be 'not secure' before"
    );

    await SpecialPowers.spawn(browser, [], async () => {
      content.postMessage("", "*"); // This kicks off the navigation.
      await ContentTaskUtils.waitForCondition(() => {
        return !content.document.body.classList.contains("running");
      });
    });

    let newIdentityMode =
      window.document.getElementById("identity-box").className;
    is(
      newIdentityMode,
      NOT_SECURE_LABEL,
      "identity should be 'not secure' after"
    );
  });
});
