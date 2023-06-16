/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

const NESTED_IFRAME_DOC_BODY_ID = "nested-iframe-body";
const NESTED_IFRAME_ID = "nested-iframe";
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const nestedURL = new URL(`http://example.com/document-builder.sjs`);
nestedURL.searchParams.append(
  "html",
  `<html>
      <head>
        <meta charset="utf-8"/>
        <title>Accessibility Nested Iframe Frame Test</title>
      </head>
      <body id="${NESTED_IFRAME_DOC_BODY_ID}">
        <table id="table">
          <tr>
            <td>cell1</td>
            <td>cell2</td>
          </tr>
        </table>
        <ul id="ul">
          <li id="li">item1</li>
        </ul>
      </body>
    </html>`
);

function getOsPid(browsingContext) {
  return browsingContext.currentWindowGlobal.osPid;
}

addAccessibleTask(
  `<iframe id="${NESTED_IFRAME_ID}" src="${nestedURL.href}"/>`,
  async function (browser, iframeDocAcc, contentDocAcc) {
    ok(iframeDocAcc, "IFRAME document accessible is present");
    let nestedDocAcc = findAccessibleChildByID(
      iframeDocAcc,
      NESTED_IFRAME_DOC_BODY_ID
    );
    let waitForNestedDocLoad = false;
    if (nestedDocAcc) {
      const state = {};
      nestedDocAcc.getState(state, {});
      if (state.value & STATE_BUSY) {
        info("Nested IFRAME document accessible is present but busy");
        waitForNestedDocLoad = true;
      } else {
        ok(true, "Nested IFRAME document accessible is present and ready");
      }
    } else {
      info("Nested IFRAME document accessible is not present yet");
      waitForNestedDocLoad = true;
    }
    if (waitForNestedDocLoad) {
      info("Waiting for doc load complete on nested iframe document");
      nestedDocAcc = (
        await waitForEvent(
          EVENT_DOCUMENT_LOAD_COMPLETE,
          NESTED_IFRAME_DOC_BODY_ID
        )
      ).accessible;
    }

    if (gIsRemoteIframe) {
      isnot(
        getOsPid(browser.browsingContext),
        getOsPid(browser.browsingContext.children[0]),
        `Content and IFRAME documents are in separate processes.`
      );
      isnot(
        getOsPid(browser.browsingContext),
        getOsPid(browser.browsingContext.children[0].children[0]),
        `Content and nested IFRAME documents are in separate processes.`
      );
      isnot(
        getOsPid(browser.browsingContext.children[0]),
        getOsPid(browser.browsingContext.children[0].children[0]),
        `IFRAME and nested IFRAME documents are in separate processes.`
      );
    } else {
      is(
        getOsPid(browser.browsingContext),
        getOsPid(browser.browsingContext.children[0]),
        `Content and IFRAME documents are in same processes.`
      );
      if (gFissionBrowser) {
        isnot(
          getOsPid(browser.browsingContext.children[0]),
          getOsPid(browser.browsingContext.children[0].children[0]),
          `IFRAME and nested IFRAME documents are in separate processes.`
        );
      } else {
        is(
          getOsPid(browser.browsingContext),
          getOsPid(browser.browsingContext.children[0].children[0]),
          `Content and nested IFRAME documents are in same processes.`
        );
      }
    }

    const tree = {
      DOCUMENT: [
        {
          INTERNAL_FRAME: [
            {
              DOCUMENT: [
                {
                  INTERNAL_FRAME: [
                    {
                      DOCUMENT: [
                        {
                          TABLE: [
                            {
                              ROW: [
                                { CELL: [{ TEXT_LEAF: [] }] },
                                { CELL: [{ TEXT_LEAF: [] }] },
                              ],
                            },
                          ],
                        },
                        {
                          LIST: [
                            {
                              LISTITEM: [
                                { LISTITEM_MARKER: [] },
                                { TEXT_LEAF: [] },
                              ],
                            },
                          ],
                        },
                      ],
                    },
                  ],
                },
              ],
            },
          ],
        },
      ],
    };
    testAccessibleTree(contentDocAcc, tree);

    const nestedIframeAcc = iframeDocAcc.getChildAt(0);
    is(
      nestedIframeAcc.getChildAt(0),
      nestedDocAcc,
      "Document for nested IFRAME matches."
    );

    is(
      nestedDocAcc.parent,
      nestedIframeAcc,
      "Nested IFRAME document's parent matches the nested IFRAME."
    );
  },
  { topLevel: false, iframe: true, remoteIframe: true }
);
