/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checks for the NodeTabbingOrderHighlighter.

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
          NodeTabbingOrderHighlighter,
        } = require("resource://devtools/server/actors/highlighters/node-tabbing-order.js");

        // Checks for updated content for an infobar.
        async function testShowHide(highlighter, node, index) {
          const shown = highlighter.show(node, { index });
          const infoBarText = highlighter.getElement("infobar-text");

          ok(shown, "Highlighter is shown.");
          is(
            parseInt(infoBarText.getTextContent(), 10),
            index,
            "infobar text content is correct"
          );

          highlighter.hide();
        }

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
        const highlighter = new NodeTabbingOrderHighlighter(env);
        await highlighter.isReady;

        info("Showing Node tabbing order highlighter with index");
        await testShowHide(highlighter, node, 1);

        info("Showing Node tabbing order highlighter with new index");
        await testShowHide(highlighter, node, 9);

        info(
          "Showing and highlighting focused node with the Node tabbing order highlighter"
        );
        highlighter.show(node, { index: 1 });
        highlighter.updateFocus(true);
        const { classList } = highlighter.getElement("root");
        ok(classList.contains("focused"), "Focus styling is applied");
        highlighter.updateFocus(false);
        ok(!classList.contains("focused"), "Focus styling is removed");
        highlighter.hide();
      });
    }
  );
});
