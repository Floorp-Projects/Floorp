/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

addAccessibleTask(
  "e10s/doc_treeupdate_whitespace.html",
  async function (browser, docAcc) {
    info("Testing top level doc");
    queryInterfaces(docAcc, [nsIAccessibleDocument]);
    const topUrl =
      (browser.isRemoteBrowser ? CURRENT_CONTENT_DIR : CURRENT_DIR) +
      "e10s/doc_treeupdate_whitespace.html";
    is(docAcc.URL, topUrl, "Initial URL correct");
    is(docAcc.mimeType, "text/html", "Mime type is correct");
    info("Changing URL");
    await invokeContentTask(browser, [], () => {
      content.history.pushState(
        null,
        "",
        content.document.location.href + "/after"
      );
    });
    is(docAcc.URL, topUrl + "/after", "URL correct after change");

    // We can't use the harness to manage iframes for us because it uses data
    // URIs for in-process iframes, but data URIs don't support
    // history.pushState.

    async function testIframe() {
      queryInterfaces(iframeDocAcc, [nsIAccessibleDocument]);
      is(iframeDocAcc.URL, src, "Initial URL correct");
      is(iframeDocAcc.mimeType, "text/html", "Mime type is correct");
      info("Changing URL");
      await invokeContentTask(browser, [], async () => {
        await SpecialPowers.spawn(content.iframe, [], () => {
          content.history.pushState(
            null,
            "",
            content.document.location.href + "/after"
          );
        });
      });
      is(iframeDocAcc.URL, src + "/after", "URL correct after change");
    }

    info("Testing same origin (in-process) iframe");
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    let src = "https://example.com/initial.html";
    let loaded = waitForEvent(
      EVENT_DOCUMENT_LOAD_COMPLETE,
      evt => evt.accessible.parent.parent == docAcc
    );
    await invokeContentTask(browser, [src], cSrc => {
      content.iframe = content.document.createElement("iframe");
      content.iframe.src = cSrc;
      content.document.body.append(content.iframe);
    });
    let iframeDocAcc = (await loaded).accessible;
    await testIframe();

    info("Testing different origin (out-of-process) iframe");
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    src = "https://example.net/initial.html";
    loaded = waitForEvent(
      EVENT_DOCUMENT_LOAD_COMPLETE,
      evt => evt.accessible.parent.parent == docAcc
    );
    await invokeContentTask(browser, [src], cSrc => {
      content.iframe.src = cSrc;
    });
    iframeDocAcc = (await await loaded).accessible;
    await testIframe();
  },
  { chrome: true, topLevel: true }
);
