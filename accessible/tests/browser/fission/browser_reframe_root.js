/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/states.js */
/* import-globals-from ../../mochitest/role.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

addAccessibleTask(
  `<input id="textbox" value="hello"/>`,
  async function (browser, iframeDocAcc, contentDocAcc) {
    info(
      "Check that the IFRAME and the IFRAME document are accessible initially."
    );
    let iframeAcc = findAccessibleChildByID(contentDocAcc, DEFAULT_IFRAME_ID);
    ok(!isDefunct(iframeAcc), "IFRAME should be accessible");
    ok(!isDefunct(iframeDocAcc), "IFRAME document should be accessible");

    info("Move the IFRAME under a new hidden root.");
    let onEvents = waitForEvent(EVENT_REORDER, contentDocAcc);
    await SpecialPowers.spawn(browser, [DEFAULT_IFRAME_ID], id => {
      const doc = content.document;
      const root = doc.createElement("div");
      root.style.display = "none";
      doc.body.appendChild(root);
      root.appendChild(doc.getElementById(id));
    });
    await onEvents;

    ok(
      isDefunct(iframeAcc),
      "IFRAME accessible should be defunct when hidden."
    );
    ok(
      isDefunct(iframeDocAcc),
      "IFRAME document's accessible should be defunct when the IFRAME is hidden."
    );
    ok(
      !findAccessibleChildByID(contentDocAcc, DEFAULT_IFRAME_ID),
      "No accessible for an IFRAME present."
    );
    ok(
      !findAccessibleChildByID(contentDocAcc, DEFAULT_IFRAME_DOC_BODY_ID),
      "No accessible for the IFRAME document present."
    );

    info("Move the IFRAME back under the content document's body.");
    onEvents = waitForEvents([
      [EVENT_REORDER, contentDocAcc],
      [
        EVENT_STATE_CHANGE,
        event => {
          const scEvent = event.QueryInterface(nsIAccessibleStateChangeEvent);
          const id = getAccessibleDOMNodeID(event.accessible);
          return (
            id === DEFAULT_IFRAME_DOC_BODY_ID &&
            scEvent.state === STATE_BUSY &&
            scEvent.isEnabled === false
          );
        },
      ],
    ]);
    await SpecialPowers.spawn(browser, [DEFAULT_IFRAME_ID], id => {
      content.document.body.appendChild(content.document.getElementById(id));
    });
    await onEvents;

    iframeAcc = findAccessibleChildByID(contentDocAcc, DEFAULT_IFRAME_ID);
    const newiframeDocAcc = iframeAcc.firstChild;

    ok(!isDefunct(iframeAcc), "IFRAME should be accessible");
    is(iframeAcc.childCount, 1, "IFRAME accessible should have a single child");
    ok(!isDefunct(newiframeDocAcc), "IFRAME document should be accessible");
    ok(
      isDefunct(iframeDocAcc),
      "Original IFRAME document accessible should be defunct."
    );
    isnot(
      iframeAcc.firstChild,
      iframeDocAcc,
      "A new accessible is created for a IFRAME document."
    );
    is(
      iframeAcc.firstChild,
      newiframeDocAcc,
      "A new accessible for a IFRAME document is the child of the IFRAME accessible"
    );
  },
  { topLevel: false, iframe: true, remoteIframe: true }
);
