/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ATSPI_ROLE_DOCUMENT_WEB = 95;
const ATSPI_ROLE_PARAGRAPH = 73;

addAccessibleTask(
  `
<p id="p">p</p>
  `,
  async function (browser, docAcc) {
    let role = await runPython(`
      global doc
      doc = getDoc()
      return doc.getRole()
    `);
    is(role, ATSPI_ROLE_DOCUMENT_WEB, "doc has correct ATSPI role");
    ok(
      await runPython(`
        global p
        p = findByDomId(doc, "p")
        return p == doc[0]
      `),
      "doc's first child is p"
    );
    role = await runPython(`p.getRole()`);
    is(role, ATSPI_ROLE_PARAGRAPH, "p has correct ATSPI role");
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);
