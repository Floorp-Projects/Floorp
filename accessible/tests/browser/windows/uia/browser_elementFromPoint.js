/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

addAccessibleTask(
  `
<button id="button">button</p>
<a id="a" href="#">a</a>
  `,
  async function (browser, docAcc) {
    ok(
      await runPython(`
        global doc
        doc = getDocUia()
        button = findUiaByDomId(doc, "button")
        rect = button.CurrentBoundingRectangle
        found = uiaClient.ElementFromPoint(POINT(rect.left + 1, rect.top + 1))
        return uiaClient.CompareElements(button, found)
      `),
      "ElementFromPoint on button returns button"
    );
    ok(
      await runPython(`
        a = findUiaByDomId(doc, "a")
        rect = a.CurrentBoundingRectangle
        found = uiaClient.ElementFromPoint(POINT(rect.left + 1, rect.top + 1))
        return uiaClient.CompareElements(a, found)
      `),
      "ElementFromPoint on a returns a"
    );
  }
);
