/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function runTests(browser, accDoc) {
  const dpr = await getContentDPR(browser);
  let componentAcc = findAccessibleChildByID(accDoc, "component1");
  testChildAtPoint(
    dpr,
    1,
    1,
    componentAcc,
    componentAcc.firstChild,
    componentAcc.firstChild
  );

  componentAcc = findAccessibleChildByID(accDoc, "component2");
  testChildAtPoint(
    dpr,
    1,
    1,
    componentAcc,
    componentAcc.firstChild,
    componentAcc.firstChild
  );
}

addAccessibleTask(
  `
  <div role="group" class="components" id="component1" style="display: inline-block;">
  <!--
    <div role="button" id="component-child"
         style="width: 100px; height: 100px; background-color: pink;">
    </div>
  -->
  </div>
  <div role="group" class="components"  id="component2" style="display: inline-block;">
  <!--
    <button>Hello world</button>
  -->
  </div>
  <script>
    // This routine adds the comment children of each 'component' to its
    // shadow root.
    var components = document.querySelectorAll(".components");
    for (var i = 0; i < components.length; i++) {
      var component = components[i];
      var shadow = component.attachShadow({mode: "open"});
      for (var child = component.firstChild; child; child = child.nextSibling) {
        if (child.nodeType === 8)
          // eslint-disable-next-line no-unsanitized/property
          shadow.innerHTML = child.data;
      }
    }
  </script>
  `,
  runTests,
  { iframe: true, remoteIframe: true }
);
