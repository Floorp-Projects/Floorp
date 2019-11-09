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
  async function(browser, fissionDocAcc, contentDocAcc) {
    info(
      "Check that the IFRAME and the fission document are accessible initially."
    );
    let iframeAcc = findAccessibleChildByID(contentDocAcc, FISSION_IFRAME_ID);
    ok(!isDefunct(iframeAcc), "IFRAME should be accessible");
    ok(!isDefunct(fissionDocAcc), "fission document should be accessible");

    info(
      "Hide the IFRAME and check that it's gone along with the fission document."
    );
    let onEvents = waitForEvent(EVENT_REORDER, contentDocAcc);
    await SpecialPowers.spawn(browser, [FISSION_IFRAME_ID], contentId => {
      content.document.getElementById(contentId).style.display = "none";
    });
    await onEvents;

    ok(
      isDefunct(iframeAcc),
      "IFRAME accessible should be defunct when hidden."
    );
    if (gFissionBrowser) {
      ok(
        !isDefunct(fissionDocAcc),
        "IFRAME document's accessible is not defunct when the IFRAME is hidden and fission is enabled."
      );
    } else {
      ok(
        isDefunct(fissionDocAcc),
        "IFRAME document's accessible is defunct when the IFRAME is hidden and fission is not enabled."
      );
    }
    ok(
      !findAccessibleChildByID(contentDocAcc, FISSION_IFRAME_ID),
      "No accessible for an IFRAME present."
    );
    ok(
      !findAccessibleChildByID(contentDocAcc, DEFAULT_FISSION_DOC_BODY_ID),
      "No accessible for the fission document present."
    );

    info(
      "Show the IFRAME and check that a new accessible is created for it as " +
        "well as the fission document."
    );

    const events = [[EVENT_REORDER, contentDocAcc]];
    if (!gFissionBrowser) {
      events.push([
        EVENT_STATE_CHANGE,
        event => {
          const scEvent = event.QueryInterface(nsIAccessibleStateChangeEvent);
          const id = getAccessibleDOMNodeID(event.accessible);
          return (
            id === DEFAULT_FISSION_DOC_BODY_ID &&
            scEvent.state === STATE_BUSY &&
            scEvent.isEnabled === false
          );
        },
      ]);
    }
    onEvents = waitForEvents(events);
    await SpecialPowers.spawn(browser, [FISSION_IFRAME_ID], contentId => {
      content.document.getElementById(contentId).style.display = "block";
    });
    await onEvents;

    iframeAcc = findAccessibleChildByID(contentDocAcc, FISSION_IFRAME_ID);
    const newFissionDocAcc = iframeAcc.firstChild;

    ok(!isDefunct(iframeAcc), "IFRAME should be accessible");
    is(iframeAcc.childCount, 1, "IFRAME accessible should have a single child");
    ok(newFissionDocAcc, "fission document exists");
    ok(!isDefunct(newFissionDocAcc), "fission document should be accessible");
    if (gFissionBrowser) {
      ok(
        !isDefunct(fissionDocAcc),
        "Original IFRAME document accessible should not be defunct when fission is enabled."
      );
      is(
        iframeAcc.firstChild,
        fissionDocAcc,
        "Existing accessible is used for a fission document."
      );
    } else {
      ok(
        isDefunct(fissionDocAcc),
        "Original IFRAME document accessible should be defunct when fission is not enabled."
      );
      isnot(
        iframeAcc.firstChild,
        fissionDocAcc,
        "A new accessible is created for a fission document."
      );
    }
    is(
      iframeAcc.firstChild,
      newFissionDocAcc,
      "A new accessible for a fission document is the child of the IFRAME accessible"
    );
  },
  { topLevel: false, iframe: true }
);
