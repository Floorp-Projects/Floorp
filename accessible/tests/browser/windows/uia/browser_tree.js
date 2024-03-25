/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function testIsControl(pyVar, isControl) {
  const result = await runPython(`bool(${pyVar}.CurrentIsControlElement)`);
  if (isControl) {
    ok(result, `${pyVar} is a control element`);
  } else {
    ok(!result, `${pyVar} isn't a control element`);
  }
}

addUiaTask(
  `
<p id="p">paragraph</p>
<div id="div">div</div>
<!-- The spans are because the UIA -> IA2 proxy seems to remove a single text
   leaf child from even the raw tree.
  -->
<a id="link" href="#">link<span> </span>></a>
<h1 id="h1">h1<span> </span></h1>
<h1 id="h1WithDiv"><div>h1 with div<span> </span></div></h1>
<input id="range" type="range">
<div onclick=";" id="clickable">clickable</div>
<div id="editable" contenteditable>editable</div>
<table id="table"><tr><th>th</th></tr></table>
  `,
  async function () {
    await definePyVar("doc", `getDocUia()`);
    await assignPyVarToUiaWithId("p");
    await testIsControl("p", false);
    await definePyVar(
      "pTextLeaf",
      `uiaClient.RawViewWalker.GetFirstChildElement(p)`
    );
    await testIsControl("pTextLeaf", true);
    await assignPyVarToUiaWithId("div");
    await testIsControl("div", false);
    await definePyVar(
      "divTextLeaf",
      `uiaClient.RawViewWalker.GetFirstChildElement(div)`
    );
    await testIsControl("divTextLeaf", true);
    await assignPyVarToUiaWithId("link");
    await testIsControl("link", true);
    await assignPyVarToUiaWithId("range");
    await testIsControl("range", true);
    await assignPyVarToUiaWithId("editable");
    await testIsControl("editable", true);
    await assignPyVarToUiaWithId("table");
    await testIsControl("table", true);
    if (!gIsUiaEnabled) {
      // The remaining tests are broken with the UIA -> IA2 proxy.
      return;
    }
    await definePyVar(
      "linkTextLeaf",
      `uiaClient.RawViewWalker.GetFirstChildElement(link)`
    );
    await testIsControl("linkTextLeaf", false);
    await assignPyVarToUiaWithId("h1");
    await testIsControl("h1", true);
    await definePyVar(
      "h1TextLeaf",
      `uiaClient.RawViewWalker.GetFirstChildElement(h1)`
    );
    await testIsControl("h1TextLeaf", false);
    await assignPyVarToUiaWithId("h1WithDiv");
    await testIsControl("h1WithDiv", true);
    // h1WithDiv's text leaf is its grandchild.
    await definePyVar(
      "h1WithDivTextLeaf",
      `uiaClient.RawViewWalker.GetFirstChildElement(
        uiaClient.RawViewWalker.GetFirstChildElement(
          h1WithDiv
        )
      )`
    );
    await testIsControl("h1WithDivTextLeaf", false);
    await assignPyVarToUiaWithId("clickable");
    await testIsControl("clickable", true);
  }
);
