/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/states.js */
/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "states.js", dir: MOCHITESTS_DIR });
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

addAccessibleTask(
  `<input id="textbox" value="hello"/>`,
  async function (browser, iframeDocAcc, contentDocAcc) {
    info(
      "Check that the IFRAME and the IFRAME document are accessible initially."
    );
    let iframeAcc = findAccessibleChildByID(contentDocAcc, DEFAULT_IFRAME_ID);
    ok(!isDefunct(iframeAcc), "IFRAME should be accessible");
    ok(!isDefunct(iframeDocAcc), "IFRAME document should be accessible");

    info(
      "Hide the IFRAME and check that it's gone along with the IFRAME document."
    );
    let onEvents = waitForEvent(EVENT_REORDER, contentDocAcc);
    await SpecialPowers.spawn(browser, [DEFAULT_IFRAME_ID], contentId => {
      content.document.getElementById(contentId).style.display = "none";
    });
    await onEvents;

    ok(
      isDefunct(iframeAcc),
      "IFRAME accessible should be defunct when hidden."
    );
    if (gIsRemoteIframe) {
      ok(
        !isDefunct(iframeDocAcc),
        "IFRAME document's accessible is not defunct when the IFRAME is hidden and fission is enabled."
      );
    } else {
      ok(
        isDefunct(iframeDocAcc),
        "IFRAME document's accessible is defunct when the IFRAME is hidden and fission is not enabled."
      );
    }
    ok(
      !findAccessibleChildByID(contentDocAcc, DEFAULT_IFRAME_ID),
      "No accessible for an IFRAME present."
    );
    ok(
      !findAccessibleChildByID(contentDocAcc, DEFAULT_IFRAME_DOC_BODY_ID),
      "No accessible for the IFRAME document present."
    );

    info(
      "Show the IFRAME and check that a new accessible is created for it as " +
        "well as the IFRAME document."
    );

    const events = [[EVENT_REORDER, contentDocAcc]];
    if (!gIsRemoteIframe) {
      events.push([
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
      ]);
    }
    onEvents = waitForEvents(events);
    await SpecialPowers.spawn(browser, [DEFAULT_IFRAME_ID], contentId => {
      content.document.getElementById(contentId).style.display = "block";
    });
    await onEvents;

    iframeAcc = findAccessibleChildByID(contentDocAcc, DEFAULT_IFRAME_ID);
    const newiframeDocAcc = iframeAcc.firstChild;

    ok(!isDefunct(iframeAcc), "IFRAME should be accessible");
    is(iframeAcc.childCount, 1, "IFRAME accessible should have a single child");
    ok(newiframeDocAcc, "IFRAME document exists");
    ok(!isDefunct(newiframeDocAcc), "IFRAME document should be accessible");
    if (gIsRemoteIframe) {
      ok(
        !isDefunct(iframeDocAcc),
        "Original IFRAME document accessible should not be defunct when fission is enabled."
      );
      is(
        iframeAcc.firstChild,
        iframeDocAcc,
        "Existing accessible is used for a IFRAME document."
      );
    } else {
      ok(
        isDefunct(iframeDocAcc),
        "Original IFRAME document accessible should be defunct when fission is not enabled."
      );
      isnot(
        iframeAcc.firstChild,
        iframeDocAcc,
        "A new accessible is created for a IFRAME document."
      );
    }
    is(
      iframeAcc.firstChild,
      newiframeDocAcc,
      "A new accessible for a IFRAME document is the child of the IFRAME accessible"
    );
  },
  { topLevel: false, iframe: true, remoteIframe: true }
);
