/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Checks functionality around audit for the AccessibleActor. This includes
 * tests for the return value when calling the audit method, payload of the
 * corresponding event as well as the AccesibleFront state being up to date.
 */

const {
  accessibility: { AUDIT_TYPE },
} = require("devtools/shared/constants");
const EMPTY_AUDIT = Object.keys(AUDIT_TYPE).reduce((audit, key) => {
  audit[key] = null;
  return audit;
}, {});

const EXPECTED_CONTRAST_DATA = {
  value: 21,
  color: [0, 0, 0, 1],
  backgroundColor: [255, 255, 255, 1],
  isLargeText: true,
  score: "AAA",
};

const EMPTY_CONTRAST_AUDIT = {
  [AUDIT_TYPE.CONTRAST]: null,
};

const CONTRAST_AUDIT = {
  [AUDIT_TYPE.CONTRAST]: EXPECTED_CONTRAST_DATA,
};

const FULL_AUDIT = {
  ...EMPTY_AUDIT,
  [AUDIT_TYPE.CONTRAST]: EXPECTED_CONTRAST_DATA,
};

async function checkAudit(a11yWalker, node, expected, options) {
  const front = await a11yWalker.getAccessibleFor(node);
  const [textLeafNode] = await front.children();

  const onAudited = textLeafNode.once("audited");
  const audit = await textLeafNode.audit(options);
  const auditFromEvent = await onAudited;

  Assert.deepEqual(audit, expected.audit, "Audit results are correct.");
  Assert.deepEqual(textLeafNode.checks, expected.checks, "Checks are correct.");
  Assert.deepEqual(
    auditFromEvent,
    expected.audit,
    "Audit results from event are correct."
  );
}

add_task(async function() {
  const { target, walker, accessibility } = await initAccessibilityFrontForUrl(
    MAIN_DOMAIN + "doc_accessibility_infobar.html"
  );

  const a11yWalker = await accessibility.getWalker();
  await accessibility.enable();

  const headerNode = await walker.querySelector(walker.rootNode, "#h1");
  await checkAudit(
    a11yWalker,
    headerNode,
    { audit: CONTRAST_AUDIT, checks: CONTRAST_AUDIT },
    { types: [AUDIT_TYPE.CONTRAST] }
  );
  await checkAudit(a11yWalker, headerNode, {
    audit: FULL_AUDIT,
    checks: FULL_AUDIT,
  });
  await checkAudit(
    a11yWalker,
    headerNode,
    { audit: CONTRAST_AUDIT, checks: FULL_AUDIT },
    { types: [AUDIT_TYPE.CONTRAST] }
  );
  await checkAudit(
    a11yWalker,
    headerNode,
    { audit: FULL_AUDIT, checks: FULL_AUDIT },
    { types: [] }
  );

  const paragraphNode = await walker.querySelector(walker.rootNode, "#p");
  await checkAudit(
    a11yWalker,
    paragraphNode,
    { audit: EMPTY_CONTRAST_AUDIT, checks: EMPTY_CONTRAST_AUDIT },
    { types: [AUDIT_TYPE.CONTRAST] }
  );
  await checkAudit(a11yWalker, paragraphNode, {
    audit: EMPTY_AUDIT,
    checks: EMPTY_AUDIT,
  });
  await checkAudit(
    a11yWalker,
    paragraphNode,
    { audit: EMPTY_CONTRAST_AUDIT, checks: EMPTY_AUDIT },
    { types: [AUDIT_TYPE.CONTRAST] }
  );
  await checkAudit(
    a11yWalker,
    paragraphNode,
    { audit: EMPTY_AUDIT, checks: EMPTY_AUDIT },
    { types: [] }
  );

  await accessibility.disable();
  await waitForA11yShutdown();
  await target.destroy();
  gBrowser.removeCurrentTab();
});
