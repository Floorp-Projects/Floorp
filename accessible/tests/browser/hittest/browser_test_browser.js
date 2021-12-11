/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function runTests(browser, accDoc) {
  // Hit testing. See bug #726097
  await invokeContentTask(browser, [], () =>
    content.document.getElementById("hittest").scrollIntoView(true)
  );

  const dpr = await getContentDPR(browser);
  const hititem = findAccessibleChildByID(accDoc, "hititem");
  const hittest = findAccessibleChildByID(accDoc, "hittest");
  const outerDocAcc = accDoc.parent;
  const rootAcc = CommonUtils.getRootAccessible(document);

  const [hitX, hitY, hitWidth, hitHeight] = Layout.getBounds(hititem, dpr);
  // "hititem" node has the full screen width, so when we divide it by 2, we are
  // still way outside the inline content.
  const tgtX = hitX + hitWidth / 2;
  const tgtY = hitY + hitHeight / 2;

  let hitAcc = rootAcc.getDeepestChildAtPoint(tgtX, tgtY);
  is(
    hitAcc,
    hititem,
    `Hit match at ${tgtX},${tgtY} (root doc deepest child). Found: ${prettyName(
      hitAcc
    )}`
  );

  const hitAcc2 = accDoc.getDeepestChildAtPoint(tgtX, tgtY);
  is(
    hitAcc,
    hitAcc2,
    `Hit match at ${tgtX},${tgtY} (doc deepest child). Found: ${prettyName(
      hitAcc2
    )}`
  );

  hitAcc = outerDocAcc.getChildAtPoint(tgtX, tgtY);
  is(
    hitAcc,
    accDoc,
    `Hit match at ${tgtX},${tgtY} (outer doc child). Found: ${prettyName(
      hitAcc
    )}`
  );

  hitAcc = accDoc.getChildAtPoint(tgtX, tgtY);
  is(
    hitAcc,
    hittest,
    `Hit match at ${tgtX},${tgtY} (doc child). Found: ${prettyName(hitAcc)}`
  );
}

addAccessibleTask(
  `
  <div id="hittest">
    <div id="hititem"><span role="img">img</span>item</div>
  </div>
 `,
  runTests,
  { iframe: true, remoteIframe: true }
);
