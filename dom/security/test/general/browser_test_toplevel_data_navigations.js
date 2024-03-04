/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const kDataBody = "toplevel navigation to data: URI allowed";
const kDataURI = "data:text/html,<body>" + kDataBody + "</body>";
const kTestPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const kRedirectURI = kTestPath + "file_toplevel_data_navigations.sjs";
const kMetaRedirectURI = kTestPath + "file_toplevel_data_meta_redirect.html";

add_task(async function test_nav_data_uri() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.block_toplevel_data_uri_navigations", true]],
  });
  await BrowserTestUtils.withNewTab(kDataURI, async function () {
    await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [{ kDataBody }],
      async function ({ kDataBody }) {
        // eslint-disable-line
        is(
          content.document.body.innerHTML,
          kDataBody,
          "data: URI navigation from system should be allowed"
        );
      }
    );
  });
});

add_task(async function test_nav_data_uri_redirect() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.block_toplevel_data_uri_navigations", true]],
  });
  let tab = BrowserTestUtils.addTab(gBrowser, kRedirectURI);
  registerCleanupFunction(async function () {
    BrowserTestUtils.removeTab(tab);
  });
  // wait to make sure data: URI did not load before checking that it got blocked
  await new Promise(resolve => setTimeout(resolve, 500));
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    is(
      content.document.body.innerHTML,
      "",
      "data: URI navigation after server redirect should be blocked"
    );
  });
});

add_task(async function test_nav_data_uri_meta_redirect() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.block_toplevel_data_uri_navigations", true]],
  });
  let tab = BrowserTestUtils.addTab(gBrowser, kMetaRedirectURI);
  registerCleanupFunction(async function () {
    BrowserTestUtils.removeTab(tab);
  });
  // wait to make sure data: URI did not load before checking that it got blocked
  await new Promise(resolve => setTimeout(resolve, 500));
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    is(
      content.document.body.innerHTML,
      "",
      "data: URI navigation after meta redirect should be blocked"
    );
  });
});
