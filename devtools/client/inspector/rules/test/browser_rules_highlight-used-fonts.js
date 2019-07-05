/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a used font-family is highlighted in the rule-view.

const TEST_URI = `
  <style type="text/css">
    #id1 {
      font-family: foo, bar, sans-serif;
    }
    #id2 {
      font-family: serif;
    }
    #id3 {
      font-family: foo, monospace, monospace, serif;
    }
    #id4 {
      font-family: foo, bar;
    }
    #id5 {
      font-family: "monospace";
    }
    #id6 {
      font-family: georgia, arial;
    }
    #id7 {
      font-family: foo, serif !important;
    }
    #id8 {
      font-family: important;
    }
    #id9::before {
      content: ' ';
      font-family: foo, monospace;
    }
  </style>
  <div id="id1">Text</div>
  <div id="id2">Text</div>
  <div id="id3">Text</div>
  <div id="id4">Text</div>
  <div id="id5">Text</div>
  <div id="id6">A &#586;</div>
  <div id="id7">Text</div>
  <div id="id8">Text</div>
  <div id="id9">Text</div>
`;

// Tests that font-family properties in the rule-view correctly
// indicates which font is in use.
// Each entry in the test array should contain:
// {
//   baseSelector: the rule-view selector to look for font-family in
//   nb: the number of fonts this property should have
//   used: the indexes of all the fonts that should be highlighted, or null if none should
//         be highlighted
//   selectBeforePseudoElement: Whether the before pseudo element should be selectd or not
// }
const TESTS = [
  { baseSelector: "#id1", nb: 3, used: [2] }, // sans-serif
  { baseSelector: "#id2", nb: 1, used: [0] }, // serif
  { baseSelector: "#id3", nb: 4, used: [1] }, // monospace
  { baseSelector: "#id4", nb: 2, used: null },
  { baseSelector: "#id5", nb: 1, used: [0] }, // monospace
  { baseSelector: "#id7", nb: 2, used: [1] }, // serif
  { baseSelector: "#id8", nb: 1, used: null },
  { baseSelector: "#id9", nb: 2, used: [1], selectBeforePseudoElement: true }, // monospace
];

if (Services.appinfo.OS !== "Linux") {
  // Both georgia and arial are used because the second character can't be rendered with
  // georgia, so the browser falls back. Also, skip this on Linux which has neither of
  // these fonts.
  TESTS.push({ baseSelector: "#id6", nb: 2, used: [0, 1] });
}

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  for (const { baseSelector, nb, used, selectBeforePseudoElement } of TESTS) {
    const onFontHighlighted = view.once("font-highlighted");

    if (selectBeforePseudoElement) {
      // Query the first children node to get the before pseudo element:
      const baseNode = await getNodeFront(baseSelector, inspector);
      const pseudoElement = (await inspector.walker.children(baseNode))
        .nodes[0];
      await selectNode(pseudoElement, inspector);
    } else {
      await selectNode(baseSelector, inspector);
    }
    await onFontHighlighted;

    const selector = !selectBeforePseudoElement
      ? baseSelector
      : `${baseSelector}::before`;
    info(`Looking for fonts in font-family property for: <${selector}>`);

    const prop = getRuleViewProperty(view, selector, "font-family").valueSpan;
    const fonts = prop.querySelectorAll(".ruleview-font-family");

    ok(fonts.length, "Fonts found in the property");
    is(fonts.length, nb, "Correct number of fonts found in the property");

    const highlighted = [...fonts].filter(span =>
      span.classList.contains("used-font")
    );
    const expectedHighlightedNb = used === null ? 0 : used.length;
    is(
      highlighted.length,
      expectedHighlightedNb,
      "Correct number of used fonts found"
    );

    let highlightedIndex = 0;
    [...fonts].forEach((font, index) => {
      if (font === highlighted[highlightedIndex]) {
        is(index, used[highlightedIndex], "The right font is highlighted");
        highlightedIndex++;
      }
    });
  }
});
