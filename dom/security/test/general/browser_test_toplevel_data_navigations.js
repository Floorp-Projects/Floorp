"use strict";

const kDataBody = "toplevel navigation to data: URI allowed";
const kDataURI = "data:text/html,<body>" + kDataBody + "</body>";

add_task(async function test_nav_data_uri_click() {
  await SpecialPowers.pushPrefEnv({
    "set": [["security.data_uri.block_toplevel_data_uri_navigations", true]],
  });
  await BrowserTestUtils.withNewTab(kDataURI, async function(browser) {
    await ContentTask.spawn(gBrowser.selectedBrowser, {kDataBody}, async function({kDataBody}) { // eslint-disable-line
     is(content.document.body.innerHTML, kDataBody,
        "data: URI navigation from system should be allowed");
    });
  });
});
