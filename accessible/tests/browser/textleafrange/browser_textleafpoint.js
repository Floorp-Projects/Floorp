/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/text.js */

addAccessibleTask(
  `
  <p id="p" style="white-space: pre-line;">A bug
h<a href="#">id i</a>n
the <strong>big</strong> rug.</p>
`,
  function(browser, docAcc) {
    const container = findAccessibleChildByID(docAcc, "p");
    const firstPoint = createTextLeafPoint(container, 0);
    const lastPoint = createTextLeafPoint(container, kTextEndOffset);

    let charSequence = [
      ...textBoundaryGenerator(firstPoint, BOUNDARY_CHAR, DIRECTION_NEXT),
    ];

    testPointEqual(
      firstPoint,
      charSequence[0],
      "Point constructed via container and offset 0 is first character point."
    );
    testPointEqual(
      lastPoint,
      charSequence[charSequence.length - 1],
      "Point constructed via container and kTextEndOffset is last character point."
    );

    const expectedCharSequence = [
      ["A bug\nh", 0],
      ["A bug\nh", 1],
      ["A bug\nh", 2],
      ["A bug\nh", 3],
      ["A bug\nh", 4],
      ["A bug\nh", 5],
      ["A bug\nh", 6],
      ["id i", 0],
      ["id i", 1],
      ["id i", 2],
      ["id i", 3],
      ["n\nthe ", 0],
      ["n\nthe ", 1],
      ["n\nthe ", 2],
      ["n\nthe ", 3],
      ["n\nthe ", 4],
      ["n\nthe ", 5],
      ["big", 0],
      ["big", 1],
      ["big", 2],
      [" rug.", 0],
      [" rug.", 1],
      [" rug.", 2],
      [" rug.", 3],
      [" rug.", 4],
      [" rug.", 5],
    ];

    testBoundarySequence(
      firstPoint,
      BOUNDARY_CHAR,
      DIRECTION_NEXT,
      expectedCharSequence,
      "Forward BOUNDARY_CHAR sequence is correct"
    );

    testBoundarySequence(
      lastPoint,
      BOUNDARY_CHAR,
      DIRECTION_PREVIOUS,
      [...expectedCharSequence].reverse(),
      "Backward BOUNDARY_CHAR sequence is correct"
    );

    const expectedWordStartSequence = [
      ["A bug\nh", 0],
      ["A bug\nh", 2],
      ["A bug\nh", 6],
      ["id i", 3],
      ["n\nthe ", 2],
      ["big", 0],
      [" rug.", 1],
    ];
    testBoundarySequence(
      firstPoint,
      BOUNDARY_WORD_START,
      DIRECTION_NEXT,
      // Add last point in doc
      [...expectedWordStartSequence, readablePoint(lastPoint)],
      "Forward BOUNDARY_WORD_START sequence is correct"
    );

    testBoundarySequence(
      lastPoint,
      BOUNDARY_WORD_START,
      DIRECTION_PREVIOUS,
      [...expectedWordStartSequence].reverse(),
      "Backward BOUNDARY_WORD_START sequence is correct"
    );

    const expectedWordEndSequence = [
      ["A bug\nh", 1],
      ["A bug\nh", 5],
      ["id i", 2],
      ["n\nthe ", 1],
      ["n\nthe ", 5],
      [" rug.", 0],
      [" rug.", 5],
    ];
    testBoundarySequence(
      firstPoint,
      BOUNDARY_WORD_END,
      DIRECTION_NEXT,
      expectedWordEndSequence,
      "Forward BOUNDARY_WORD_END sequence is correct",
      {
        // XXX: Includes first point in doc which is not a word end
        todo: true,
      }
    );

    testBoundarySequence(
      lastPoint,
      BOUNDARY_WORD_END,
      DIRECTION_PREVIOUS,
      [readablePoint(firstPoint), ...expectedWordEndSequence].reverse(),
      "Backward BOUNDARY_WORD_END sequence is correct",
      {
        // XXX: Doesn't include last point in doc which is a word end
        todo: true,
      }
    );

    const expectedLineStartSequence = [
      ["A bug\nh", 0],
      ["A bug\nh", 6],
      ["n\nthe ", 2],
    ];
    testBoundarySequence(
      firstPoint,
      BOUNDARY_LINE_START,
      DIRECTION_NEXT,
      // Add last point in doc
      [...expectedLineStartSequence, readablePoint(lastPoint)],
      "Forward BOUNDARY_LINE_START sequence is correct"
    );

    testBoundarySequence(
      lastPoint,
      BOUNDARY_LINE_START,
      DIRECTION_PREVIOUS,
      [...expectedLineStartSequence].reverse(),
      "Backward BOUNDARY_LINE_START sequence is correct"
    );

    const expectedLineEndSequence = [
      ["A bug\nh", 5],
      ["n\nthe ", 1],
    ];
    testBoundarySequence(
      firstPoint,
      BOUNDARY_LINE_END,
      DIRECTION_NEXT,
      // Add last point in doc
      [...expectedLineEndSequence, readablePoint(lastPoint)],
      "Forward BOUNDARY_LINE_END sequence is correct",
      { todo: true }
    );

    testBoundarySequence(
      lastPoint,
      BOUNDARY_LINE_END,
      DIRECTION_PREVIOUS,
      [readablePoint(firstPoint), ...expectedLineEndSequence].reverse(),
      "Backward BOUNDARY_LINE_END sequence is correct"
    );
  },
  { chrome: true, topLevel: isCacheEnabled, iframe: false, remoteIframe: false }
);

addAccessibleTask(
  `<p id="p">
    Rob ca<input id="i1" value="n m">op up.
  </p>`,
  function(browser, docAcc) {
    const container = findAccessibleChildByID(docAcc, "p");
    const firstPoint = createTextLeafPoint(container, 0);
    const lastPoint = createTextLeafPoint(container, kTextEndOffset);

    testBoundarySequence(
      firstPoint,
      BOUNDARY_CHAR,
      DIRECTION_NEXT,
      [
        ["Rob ca", 0],
        ["Rob ca", 1],
        ["Rob ca", 2],
        ["Rob ca", 3],
        ["Rob ca", 4],
        ["Rob ca", 5],
        ["n m", 0],
        ["n m", 1],
        ["n m", 2],
        ["n m", 3],
      ],
      "Forward BOUNDARY_CHAR sequence when stopping in editable is correct",
      { flags: BOUNDARY_FLAG_STOP_IN_EDITABLE }
    );

    testBoundarySequence(
      lastPoint,
      BOUNDARY_CHAR,
      DIRECTION_PREVIOUS,
      [
        ["op up. ", 7],
        ["op up. ", 6],
        ["op up. ", 5],
        ["op up. ", 4],
        ["op up. ", 3],
        ["op up. ", 2],
        ["op up. ", 1],
        ["op up. ", 0],
        ["n m", 2],
        ["n m", 1],
        ["n m", 0],
      ],
      "Backward BOUNDARY_CHAR sequence when stopping in editable is correct",
      { flags: BOUNDARY_FLAG_STOP_IN_EDITABLE }
    );

    testBoundarySequence(
      firstPoint,
      BOUNDARY_WORD_START,
      DIRECTION_NEXT,
      [
        ["Rob ca", 0],
        ["Rob ca", 4],
        ["n m", 2],
      ],
      "Forward BOUNDARY_WORD_START sequence when stopping in editable is correct",
      {
        flags: BOUNDARY_FLAG_STOP_IN_EDITABLE,
        todo: true, //  Shouldn't consider end of input a word start
      }
    );
  },
  { chrome: true, topLevel: isCacheEnabled, iframe: false, remoteIframe: false }
);

addAccessibleTask(
  `
  <p id="p" style="white-space: pre-line;">A bug
on a <span style="display: block;">rug</span></p>
  <p id="p2">
    Numbers:
  </p>
  <ul>
    <li>One</li>
    <li>Two</li>
    <li>Three</li>
  </ul>`,
  function(browser, docAcc) {
    const container = findAccessibleChildByID(docAcc, "p");
    const firstPoint = createTextLeafPoint(container, 0);
    const lastPoint = createTextLeafPoint(container, kTextEndOffset);

    const expectedParagraphStart = [
      ["A bug\non a ", 0],
      ["A bug\non a ", 6],
      ["rug", 0],
      ["Numbers: ", 0],
      ["• ", 0],
      ["• ", 0],
      ["• ", 0],
    ];
    testBoundarySequence(
      firstPoint,
      BOUNDARY_PARAGRAPH,
      DIRECTION_NEXT,
      [...expectedParagraphStart, readablePoint(lastPoint)],
      "Forward BOUNDARY_PARAGRAPH sequence is correct",
      {
        // XXX: Missing the first paragraph start.
        todo: true,
      }
    );
  },
  { chrome: true, topLevel: isCacheEnabled, iframe: false, remoteIframe: false }
);
