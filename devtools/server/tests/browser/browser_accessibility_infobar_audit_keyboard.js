/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checks for the AccessibleHighlighter's infobar component and its keyboard
// audit.

add_task(async function() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: MAIN_DOMAIN + "doc_accessibility_infobar.html",
    },
    async function(browser) {
      await ContentTask.spawn(browser, null, async function() {
        const { require } = ChromeUtils.import(
          "resource://devtools/shared/Loader.jsm"
        );
        const {
          HighlighterEnvironment,
        } = require("devtools/server/actors/highlighters");
        const {
          AccessibleHighlighter,
        } = require("devtools/server/actors/highlighters/accessible");
        const { LocalizationHelper } = require("devtools/shared/l10n");
        const L10N = new LocalizationHelper(
          "devtools/shared/locales/accessibility.properties"
        );

        const {
          accessibility: {
            AUDIT_TYPE,
            ISSUE_TYPE: {
              [AUDIT_TYPE.KEYBOARD]: {
                INTERACTIVE_NO_ACTION,
                FOCUSABLE_NO_SEMANTICS,
              },
            },
            SCORES: { FAIL, WARNING },
          },
        } = require("devtools/shared/constants");

        /**
         * Checks for updated content for an infobar.
         *
         * @param  {Object} infobar
         *         Accessible highlighter's infobar component.
         * @param  {Object} audit
         *         Audit information that is passed on highlighter show.
         */
        function checkKeyboard(infobar, audit) {
          const { issue, score } = audit || {};
          let expected = "";
          if (issue) {
            const { ISSUE_TO_INFOBAR_LABEL_MAP } = infobar.audit.reports[
              AUDIT_TYPE.KEYBOARD
            ].constructor;
            expected = L10N.getStr(ISSUE_TO_INFOBAR_LABEL_MAP[issue]);
          }

          is(
            infobar.getTextContent("keyboard"),
            expected,
            "infobar keyboard audit text content is correct"
          );
          if (score) {
            ok(infobar.getElement("keyboard").classList.contains(score));
          }
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

        // Now, we can test the Infobar's audit content.
        const node = content.document.createElement("div");
        content.document.body.append(node);
        const highlighter = new AccessibleHighlighter(env);
        const infobar = highlighter.accessibleInfobar;
        const bounds = {
          x: 0,
          y: 0,
          w: 250,
          h: 100,
        };

        const tests = [
          {
            desc:
              "Infobar is shown with no keyboard audit content when no audit.",
          },
          {
            desc:
              "Infobar is shown with no keyboard audit content when audit is null.",
            audit: null,
          },
          {
            desc:
              "Infobar is shown with no keyboard audit content when empty " +
              "keyboard audit.",
            audit: { [AUDIT_TYPE.KEYBOARD]: null },
          },
          {
            desc: "Infobar is shown with keyboard audit content for an error.",
            audit: {
              [AUDIT_TYPE.KEYBOARD]: {
                score: FAIL,
                issue: INTERACTIVE_NO_ACTION,
              },
            },
          },
          {
            desc: "Infobar is shown with keyboard audit content for a warning.",
            audit: {
              [AUDIT_TYPE.KEYBOARD]: {
                score: WARNING,
                issue: FOCUSABLE_NO_SEMANTICS,
              },
            },
          },
        ];

        for (const test of tests) {
          const { desc, audit } = test;

          info(desc);
          highlighter.show(node, { ...bounds, audit });
          checkKeyboard(infobar, audit && audit[AUDIT_TYPE.KEYBOARD]);
          highlighter.hide();
        }
      });
    }
  );
});
