/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  accessibility: { AUDIT_TYPE, ISSUE_TYPE, SCORES },
} = require("devtools/shared/constants");

// Checks for the AccessibleWalkerActor audit.
add_task(async function() {
  const { target, accessibility } = await initAccessibilityFrontForUrl(
    MAIN_DOMAIN + "doc_accessibility_audit.html"
  );

  const accessibles = [
    {
      name: "",
      role: "document",
      childCount: 2,
      checks: {
        [AUDIT_TYPE.CONTRAST]: null,
        [AUDIT_TYPE.KEYBOARD]: null,
        [AUDIT_TYPE.TEXT_LABEL]: {
          score: SCORES.FAIL,
          issue: ISSUE_TYPE.DOCUMENT_NO_TITLE,
        },
      },
    },
    {
      name:
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do " +
        "eiusmod tempor incididunt ut labore et dolore magna aliqua.",
      role: "paragraph",
      childCount: 1,
      checks: {
        [AUDIT_TYPE.CONTRAST]: null,
        [AUDIT_TYPE.KEYBOARD]: null,
        [AUDIT_TYPE.TEXT_LABEL]: null,
      },
    },
    {
      name:
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do " +
        "eiusmod tempor incididunt ut labore et dolore magna aliqua.",
      role: "text leaf",
      childCount: 0,
      checks: {
        [AUDIT_TYPE.CONTRAST]: {
          value: 4.0,
          color: [255, 0, 0, 1],
          backgroundColor: [255, 255, 255, 1],
          isLargeText: false,
          score: SCORES.FAIL,
        },
        [AUDIT_TYPE.KEYBOARD]: null,
        [AUDIT_TYPE.TEXT_LABEL]: null,
      },
    },
    {
      name: "",
      role: "paragraph",
      childCount: 1,
      checks: {
        [AUDIT_TYPE.CONTRAST]: null,
        [AUDIT_TYPE.KEYBOARD]: null,
        [AUDIT_TYPE.TEXT_LABEL]: null,
      },
    },
    {
      name: "Accessible Paragraph",
      role: "text leaf",
      childCount: 0,
      checks: {
        [AUDIT_TYPE.CONTRAST]: {
          value: 4.0,
          color: [255, 0, 0, 1],
          backgroundColor: [255, 255, 255, 1],
          isLargeText: false,
          score: SCORES.FAIL,
        },
        [AUDIT_TYPE.KEYBOARD]: null,
        [AUDIT_TYPE.TEXT_LABEL]: null,
      },
    },
  ];
  const total = accessibles.length;
  const auditProgress = [
    { total, percentage: 20 },
    { total, percentage: 40 },
    { total, percentage: 60 },
    { total, percentage: 80 },
    { total, percentage: 100 },
  ];

  function findAccessible(name, role) {
    return accessibles.find(
      accessible => accessible.name === name && accessible.role === role
    );
  }

  async function checkWalkerAudit(walker, expectedSize, options) {
    info("Checking AccessibleWalker audit functionality");
    const expectedProgress = Array.from(auditProgress);
    const ancestries = await new Promise((resolve, reject) => {
      const auditEventHandler = ({ type, ancestries: response, progress }) => {
        switch (type) {
          case "error":
            walker.off("audit-event", auditEventHandler);
            reject();
            break;
          case "completed":
            walker.off("audit-event", auditEventHandler);
            resolve(response);
            is(expectedProgress.length, 0, "All progress events fired");
            break;
          case "progress":
            SimpleTest.isDeeply(
              progress,
              expectedProgress.shift(),
              "Progress data is correct"
            );
            break;
          default:
            break;
        }
      };

      walker.on("audit-event", auditEventHandler);
      walker.startAudit(options);
    });

    is(ancestries.length, expectedSize, "The size of ancestries is correct");
    for (const ancestry of ancestries) {
      for (const { accessible, children } of ancestry) {
        checkA11yFront(
          accessible,
          findAccessible(accessibles.name, accessibles.role)
        );
        for (const child of children) {
          checkA11yFront(child, findAccessible(child.name, child.role));
        }
      }
    }
  }

  const a11yWalker = await accessibility.getWalker();
  ok(a11yWalker, "The AccessibleWalkerFront was returned");
  await accessibility.enable();

  await checkWalkerAudit(a11yWalker, 3);
  await checkWalkerAudit(a11yWalker, 2, { types: [AUDIT_TYPE.CONTRAST] });

  await accessibility.disable();
  await waitForA11yShutdown();
  await target.destroy();
  gBrowser.removeCurrentTab();
});
