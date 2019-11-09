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
  async function(browser, contentDocAcc) {
    info(
      "Check that the IFRAME and the fission document are not accessible initially."
    );
    let iframeAcc = findAccessibleChildByID(contentDocAcc, FISSION_IFRAME_ID);
    let fissionDocAcc = findAccessibleChildByID(
      contentDocAcc,
      DEFAULT_FISSION_DOC_BODY_ID
    );
    ok(!iframeAcc, "IFRAME is hidden and should not be accessible");
    ok(!fissionDocAcc, "fission document is hidden and should be accessible");

    info(
      "Show the IFRAME and check that it's now available in the accessibility tree."
    );

    const events = [[EVENT_REORDER, contentDocAcc]];
    if (!gFissionBrowser) {
      // Until this event is fired, IFRAME accessible has no children attached.
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
    const onEvents = waitForEvents(events);
    await SpecialPowers.spawn(browser, [FISSION_IFRAME_ID], contentId => {
      content.document.getElementById(contentId).style.display = "";
    });
    await onEvents;

    iframeAcc = findAccessibleChildByID(contentDocAcc, FISSION_IFRAME_ID);
    fissionDocAcc = findAccessibleChildByID(
      contentDocAcc,
      DEFAULT_FISSION_DOC_BODY_ID
    );

    ok(!isDefunct(iframeAcc), "IFRAME should be accessible");
    is(iframeAcc.childCount, 1, "IFRAME accessible should have a single child");
    ok(fissionDocAcc, "fission document exists");
    ok(!isDefunct(fissionDocAcc), "fission document should be accessible");
    is(
      iframeAcc.firstChild,
      fissionDocAcc,
      "An accessible for a fission document is the child of the IFRAME accessible"
    );
    is(
      fissionDocAcc.parent,
      iframeAcc,
      "Fission document's parent matches the IFRAME."
    );
  },
  {
    topLevel: false,
    iframe: true,
    fissionIFrameAttrs: {
      style: "display: none;",
    },
    skipFissionDocLoad: true,
  }
);
