/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ROLE_SYSTEM_DOCUMENT = 15;
const ROLE_SYSTEM_GROUPING = 20;
const IA2_ROLE_PARAGRAPH = 1054;

addAccessibleTask(
  `
<p id="p">p</p>
  `,
  async function (browser, docAcc) {
    let role = await runPython(`
      global doc
      doc = getDocIa2()
      return doc.accRole(CHILDID_SELF)
    `);
    is(role, ROLE_SYSTEM_DOCUMENT, "doc has correct MSAA role");
    role = await runPython(`doc.role()`);
    is(role, ROLE_SYSTEM_DOCUMENT, "doc has correct IA2 role");
    ok(
      await runPython(`
        global p
        p = findIa2ByDomId(doc, "p")
        firstChild = toIa2(doc.accChild(1))
        return p == firstChild
      `),
      "doc's first child is p"
    );
    role = await runPython(`p.accRole(CHILDID_SELF)`);
    is(role, ROLE_SYSTEM_GROUPING, "p has correct MSAA role");
    role = await runPython(`p.role()`);
    is(role, IA2_ROLE_PARAGRAPH, "p has correct IA2 role");
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);
