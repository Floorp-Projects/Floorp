/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checks for the AccessibleHighlighter's and XULWindowHighlighter's infobar components.

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
          AccessibleHighlighter,
        } = require("resource://devtools/server/actors/highlighters/accessible.js");

        /**
         * Get whether or not infobar container is hidden.
         *
         * @param  {Object} infobar
         *         Accessible highlighter's infobar component.
         * @return {String|null} If the infobar container is hidden.
         */
        function isContainerHidden(infobar) {
          return !!infobar
            .getElement("infobar-container")
            .getAttribute("hidden");
        }

        /**
         * Get name of accessible object.
         *
         * @param  {Object} infobar
         *         Accessible highlighter's infobar component.
         * @return {String} The text content of the infobar-name element.
         */
        function getName(infobar) {
          return infobar.getTextContent("infobar-name");
        }

        /**
         * Get role of accessible object.
         *
         * @param  {Object} infobar
         *         Accessible highlighter's infobar component.
         * @return {String} The text content of the infobar-role element.
         */
        function getRole(infobar) {
          return infobar.getTextContent("infobar-role");
        }

        /**
         * Checks for updated content for an infobar with valid bounds.
         *
         * @param  {Object} infobar
         *         Accessible highlighter's infobar component.
         * @param  {Object} options
         *         Options to pass for the highlighter's show method.
         *         Available options:
         *         - {String} role
         *           Role value of the accessible.
         *         - {String} name
         *           Name value of the accessible.
         *         - {Boolean} shouldBeHidden
         *           If the infobar component should be hidden.
         */
        function checkInfobar(infobar, { shouldBeHidden, role, name }) {
          is(
            isContainerHidden(infobar),
            shouldBeHidden,
            "Infobar's hidden state is correct."
          );

          if (shouldBeHidden) {
            return;
          }

          is(getRole(infobar), role, "infobarRole text content is correct");
          is(
            getName(infobar),
            `"${name}"`,
            "infoBarName text content is correct"
          );
        }

        /**
         * Checks for updated content of an infobar with valid bounds.
         *
         * @param  {Element} node
         *         Node to check infobar content on.
         * @param  {Object}  highlighter
         *         Accessible highlighter.
         */
        function testInfobar(node, highlighter) {
          const infobar = highlighter.accessibleInfobar;
          const bounds = {
            x: 0,
            y: 0,
            w: 250,
            h: 100,
          };

          info("Check that infobar is shown with valid bounds.");
          highlighter.show(node, {
            ...bounds,
            role: "button",
            name: "Accessible Button",
          });

          checkInfobar(infobar, {
            role: "button",
            name: "Accessible Button",
            shouldBeHidden: false,
          });
          highlighter.hide();

          info("Check that infobar is hidden after .hide() is called.");
          checkInfobar(infobar, { shouldBeHidden: true });

          info("Check to make sure content is updated with new options.");
          highlighter.show(node, {
            ...bounds,
            name: "Test link",
            role: "link",
          });
          checkInfobar(infobar, {
            name: "Test link",
            role: "link",
            shouldBeHidden: false,
          });
          highlighter.hide();
        }

        // Start testing. First, create highlighter environment and initialize.
        const env = new HighlighterEnvironment();
        env.initFromWindow(content.window);

        // Wait for loading highlighter environment content to complete before creating the
        // highlighter.
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

        // Now, we can test the Infobar and XULWindowInfobar components with their
        // respective highlighters.
        const node = content.document.createElement("div");
        content.document.body.append(node);

        info("Checks for Infobar's show method");
        const highlighter = new AccessibleHighlighter(env);
        await highlighter.isReady;
        testInfobar(node, highlighter);
      });
    }
  );
});
