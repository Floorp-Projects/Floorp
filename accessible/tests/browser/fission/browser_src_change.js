/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

addAccessibleTask(
  `<input id="textbox" value="hello"/>`,
  async function (browser, iframeDocAcc, contentDocAcc) {
    info(
      "Check that the IFRAME and the IFRAME document are accessible initially."
    );
    let iframeAcc = findAccessibleChildByID(contentDocAcc, DEFAULT_IFRAME_ID);
    ok(isAccessible(iframeAcc), "IFRAME should be accessible");
    ok(isAccessible(iframeDocAcc), "IFRAME document should be accessible");

    info("Replace src URL for the IFRAME with one with different origin.");
    const onDocLoad = waitForEvent(
      EVENT_DOCUMENT_LOAD_COMPLETE,
      DEFAULT_IFRAME_DOC_BODY_ID
    );

    await SpecialPowers.spawn(
      browser,
      [DEFAULT_IFRAME_ID, CURRENT_CONTENT_DIR],
      (id, olddir) => {
        const { src } = content.document.getElementById(id);
        content.document.getElementById(id).src = src.replace(
          olddir,
          // eslint-disable-next-line @microsoft/sdl/no-insecure-url
          "http://example.net/browser/accessible/tests/browser/"
        );
      }
    );
    const newiframeDocAcc = (await onDocLoad).accessible;

    ok(isAccessible(iframeAcc), "IFRAME should be accessible");
    ok(
      isAccessible(newiframeDocAcc),
      "new IFRAME document should be accessible"
    );
    isnot(
      iframeDocAcc,
      newiframeDocAcc,
      "A new accessible is created for a IFRAME document."
    );
    is(
      iframeAcc.firstChild,
      newiframeDocAcc,
      "An IFRAME has a new accessible for a IFRAME document as a child."
    );
    is(
      newiframeDocAcc.parent,
      iframeAcc,
      "A new accessible for a IFRAME document has an IFRAME as a parent."
    );
  },
  { topLevel: false, iframe: true, remoteIframe: true }
);
