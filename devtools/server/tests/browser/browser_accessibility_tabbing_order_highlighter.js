/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checks for the TabbingOrderHighlighter.

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: MAIN_DOMAIN + "doc_accessibility_infobar.html",
    },
    async function (browser) {
      await SpecialPowers.spawn(browser, [], async function () {
        const { require } = ChromeUtils.importESModule(
          "resource://devtools/shared/loader/Loader.sys.mjs"
        );
        const {
          HighlighterEnvironment,
        } = require("resource://devtools/server/actors/highlighters.js");
        const {
          TabbingOrderHighlighter,
        } = require("resource://devtools/server/actors/highlighters/tabbing-order.js");

        // Start testing. First, create highlighter environment and initialize.
        const env = new HighlighterEnvironment();
        env.initFromWindow(content.window);

        // Wait for loading highlighter environment content to complete before
        // creating the highlighter.
        await new Promise(resolve => {
          const doc = env.document;

          function onContentLoaded() {
            if (
              doc.readyState === "interactive" ||
              doc.readyState === "complete"
            ) {
              resolve();
            } else {
              doc.addEventListener("DOMContentLoaded", onContentLoaded, {
                once: true,
              });
            }
          }

          onContentLoaded();
        });

        // Now, we can test the Infobar's index content.
        const node = content.document.createElement("div");
        content.document.body.append(node);
        const highlighter = new TabbingOrderHighlighter(env);
        await highlighter.isReady;

        info("Showing tabbing order highlighter for all tabbable nodes");
        const { contentDOMReference, index } = await highlighter.show(
          content.document,
          {
            index: 0,
          }
        );

        is(
          contentDOMReference,
          null,
          "No current element when at the end of the tab order"
        );
        is(index, 2, "Current index is correct");
        is(
          highlighter._highlighters.size,
          2,
          "Number of node tabbing order highlighters is correct"
        );
        for (let i = 0; i < highlighter._highlighters.size; i++) {
          const nodeHighlighter = [...highlighter._highlighters.values()][i];
          const infoBarText = nodeHighlighter.getElement("infobar-text");

          is(
            parseInt(infoBarText.getTextContent(), 10),
            i + 1,
            "infobar text content is correct"
          );
        }

        info("Showing focus highlighting");
        const input = content.document.getElementById("input");
        highlighter.updateFocus({ node: input, focused: true });
        const nodeHighlighter = highlighter._highlighters.get(input);
        const { classList } = nodeHighlighter.getElement("root");
        ok(classList.contains("focused"), "Focus styling is applied");
        highlighter.updateFocus({ node: input, focused: false });
        ok(!classList.contains("focused"), "Focus styling is removed");

        highlighter.hide();
      });
    }
  );
});
