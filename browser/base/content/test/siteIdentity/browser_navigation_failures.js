/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the site identity indicator is properly updated for navigations
// that fail for various reasons. In particular, we currently test TLS handshake
// failures and about: pages that don't actually exist.
// See bug 1492424 and bug 1493427.

const kSecureURI = getRootDirectory(gTestPath).replace("chrome://mochitests/content",
                                                       "https://example.com") + "dummy_page.html";
add_task(async function() {
  await BrowserTestUtils.withNewTab(kSecureURI, async (browser) => {
    let identityMode = window.document.getElementById("identity-box").className;
    is(identityMode, "verifiedDomain", "identity should be secure before");

    const TLS_HANDSHAKE_FAILURE_URI = "https://ssl3.example.com/";
    // Try to connect to a server where the TLS handshake will fail.
    BrowserTestUtils.loadURI(browser, TLS_HANDSHAKE_FAILURE_URI);
    await BrowserTestUtils.browserLoaded(browser, false, TLS_HANDSHAKE_FAILURE_URI, true);

    let newIdentityMode = window.document.getElementById("identity-box").className;
    is(newIdentityMode, "unknownIdentity", "identity should be unknown (not secure) after");
  });
});

add_task(async function() {
  await BrowserTestUtils.withNewTab(kSecureURI, async (browser) => {
    let identityMode = window.document.getElementById("identity-box").className;
    is(identityMode, "verifiedDomain", "identity should be secure before");

    const BAD_ABOUT_PAGE_URI = "about:somethingthatdoesnotexist";
    // Try to load an about: page that doesn't exist
    BrowserTestUtils.loadURI(browser, BAD_ABOUT_PAGE_URI);
    await BrowserTestUtils.browserLoaded(browser, false, BAD_ABOUT_PAGE_URI, true);

    let newIdentityMode = window.document.getElementById("identity-box").className;
    is(newIdentityMode, "unknownIdentity", "identity should be unknown (not secure) after");
  });
});
