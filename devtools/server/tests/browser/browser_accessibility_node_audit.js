/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Checks functionality around audit for the AccessibleActor. This includes
 * tests for the return value when calling the audit method, payload of the
 * corresponding event as well as the AccesibleFront state being up to date.
 */

add_task(async function() {
  const {target, walker, accessibility} =
    await initAccessibilityFrontForUrl(MAIN_DOMAIN + "doc_accessibility_infobar.html");

  const a11yWalker = await accessibility.getWalker();
  await accessibility.enable();
  const headerNode = await walker.querySelector(walker.rootNode, "#h1");
  const headerFront = await a11yWalker.getAccessibleFor(headerNode);
  const [textLeafNode] = await headerFront.children();

  const onAudited = textLeafNode.once("audited");
  const audit = await textLeafNode.audit();
  const auditFromEvent = await onAudited;

  const expectedAudit = {
    "CONTRAST": {
      "value": 21,
      "color": [0, 0, 0, 1],
      "backgroundColor": [255, 255, 255, 1],
      "isLargeText": true,
    },
  };

  Assert.deepEqual(audit, expectedAudit, "Audit results are correct.");
  Assert.deepEqual(textLeafNode.checks, expectedAudit, "Checks are correct.");
  Assert.deepEqual(auditFromEvent, expectedAudit,
    "Audit results from event are correct.");

  await accessibility.disable();
  await waitForA11yShutdown();
  await target.destroy();
  gBrowser.removeCurrentTab();
});
