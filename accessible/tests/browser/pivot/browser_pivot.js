/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Tests Pivot API
 */
addAccessibleTask(
  `
  <h1 id="heading-1-1">Main Title</h1>
  <h2 id="heading-2-1" aria-hidden="true">First Section Title</h2>
  <p id="paragraph-1">
    Lorem ipsum <strong>dolor</strong> sit amet. Integer vitae urna
    leo, id <a href="#">semper</a> nulla.
  </p>
  <h2 id="heading-2-2" aria-hidden="undefined">Second Section Title</h2>
  <p id="paragraph-2" aria-hidden="">
    Sed accumsan luctus lacus, vitae mollis arcu tristique vulputate.</p>
  <p id="paragraph-3" aria-hidden="true">
    <a href="#" id="hidden-link">Maybe</a> it was the other <i>George Michael</i>.
    You know, the <a href="#">singer-songwriter</a>.
  </p>
  <p style="opacity: 0;" id="paragraph-4">
    This is completely transparent
  </p>
  <iframe
     src="data:text/html,<html><body>An <i>embedded</i> document.</body></html>">
  </iframe>
  <div id="hide-me">Hide me</div>
  <p id="links" aria-hidden="false">
    <a href="http://mozilla.org" title="Link 1 title">Link 1</a>
    <a href="http://mozilla.org" title="Link 2 title">Link 2</a>
    <a href="http://mozilla.org" title="Link 3 title">Link 3</a>
  </p>
  <ul>
    <li>Hello<span> </span></li>
    <li>World</li>
  </ul>
  `,
  async function (browser, docAcc) {
    let pivot = gAccService.createAccessiblePivot(docAcc);
    testPivotSequence(pivot, HeadersTraversalRule, [
      "heading-1-1",
      "heading-2-2",
    ]);

    testPivotSequence(pivot, ObjectTraversalRule, [
      "Main Title",
      "Lorem ipsum ",
      "dolor",
      " sit amet. Integer vitae urna leo, id ",
      "semper",
      " nulla. ",
      "Second Section Title",
      "Sed accumsan luctus lacus, vitae mollis arcu tristique vulputate.",
      "An ",
      "embedded",
      " document.",
      "Hide me",
      "Link 1",
      "Link 2",
      "Link 3",
      "Hello",
      "World",
    ]);

    let hideMeAcc = findAccessibleChildByID(docAcc, "hide-me");
    let onHide = waitForEvent(EVENT_HIDE, hideMeAcc);
    invokeContentTask(browser, [], () => {
      content.document.getElementById("hide-me").remove();
    });

    await onHide;
    testFailsWithNotInTree(
      () => pivot.next(hideMeAcc, ObjectTraversalRule),
      "moveNext from defunct accessible should fail"
    );

    let linksAcc = findAccessibleChildByID(docAcc, "links");

    let removedRootPivot = gAccService.createAccessiblePivot(linksAcc);
    onHide = waitForEvent(EVENT_HIDE, linksAcc);
    invokeContentTask(browser, [], () => {
      content.document.getElementById("links").remove();
    });

    await onHide;
    testFailsWithNotInTree(
      () => removedRootPivot.last(ObjectTraversalRule),
      "moveLast with pivot with defunct root should fail"
    );

    let [x, y] = getBounds(findAccessibleChildByID(docAcc, "heading-1-1"));
    let hitacc = pivot.atPoint(x + 1, y + 1, HeadersTraversalRule);
    is(getIdOrName(hitacc), "heading-1-1", "Matching accessible at point");

    hitacc = pivot.atPoint(x - 1, y - 1, HeadersTraversalRule);
    ok(!hitacc, "No heading at given point");
  },
  { iframe: true, remoteIframe: true, topLevel: true, chrome: true }
);
