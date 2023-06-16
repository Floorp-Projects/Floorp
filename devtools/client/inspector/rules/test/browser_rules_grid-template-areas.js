/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the text editor correctly display the grid-template-areas value.
// The CSS Grid spec allows to create grid-template-areas in an ascii-art style matrix.
// It should show each string on its own line, when displaying rules for grid-template-areas.

const TEST_URI = `
<style type='text/css'>
  #testid {
    display: grid;
    grid-template-areas: "a   a   bb"
                         'a   a   bb'
                         "ccc ccc bb";
  }

  #testid-quotes {
    quotes: "«" "»" "‹" "›";
  }

  #testid-invalid-strings {
    grid-template-areas: "a   a   b"
                      "a   a";
  }

  #testid-valid-quotefree-value {
    grid-template-areas: inherit;
  }

  .a {
		grid-area: a;
	}

	.b {
		grid-area: bb;
	}

	.c {
		grid-area: ccc;
	}
</style>
<div id="testid">
  <div class="a">cell a</div>
  <div class="b">cell b</div>
  <div class="c">cell c</div>
</div>
<q id="testid-quotes">
  Show us the wonder-working <q>Brothers,</q> let them come out publicly—and we will believe in them!
</q>
<div id="testid-invalid-strings">
  <div class="a">cell a</div>
  <div class="b">cell b</div>
</div>
<div id="testid-valid-quotefree-value">
  <div class="a">cell a</div>
  <div class="b">cell b</div>
</div>
`;

const multiLinesInnerText = '\n"a   a   bb" \n\'a   a   bb\' \n"ccc ccc bb"';
const typedAndCopiedMultiLinesString = '"a a bb ccc" "a a bb ccc"';
const typedAndCopiedMultiLinesInnerText = '\n"a a bb ccc" \n"a a bb ccc"';

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  info("Selecting the test node");
  await selectNode("#testid", inspector);

  info(
    "Tests display of grid-template-areas value in an ascii-art style," +
      "displaying each string on its own line"
  );

  const gridRuleProperty = await getRuleViewProperty(
    view,
    "#testid",
    "grid-template-areas"
  ).valueSpan;
  is(
    gridRuleProperty.innerText,
    multiLinesInnerText,
    "the grid-area is displayed with each string in its own line, and sufficient spacing for areas to align vertically"
  );

  // copy/paste the current value inside, to also make sure of the value copied is useful as text

  // Calculate offsets to click in the value line which is below the property name line .
  const rect = gridRuleProperty.getBoundingClientRect();
  const previousProperty = await getRuleViewProperty(
    view,
    "#testid",
    "display"
  ).nameSpan.getBoundingClientRect();

  const x = rect.width / 2;
  const y = rect.y - previousProperty.y + 1;

  info("Focusing the css property editable value");
  await focusEditableField(view, gridRuleProperty, x, y);
  info("typing a new value");
  [...typedAndCopiedMultiLinesString].map(char =>
    EventUtils.synthesizeKey(char, {}, view.styleWindow)
  );
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  is(
    gridRuleProperty.innerText,
    typedAndCopiedMultiLinesInnerText,
    "the typed value is correct, and a single quote is displayed on its own line"
  );
  info("copy-paste the 'grid-template-areas' property value to duplicate it");
  const onDone = view.once("ruleview-changed");
  await focusEditableField(view, gridRuleProperty, x, y);
  EventUtils.synthesizeKey("C", { accelKey: true }, view.styleWindow);
  EventUtils.synthesizeKey("KEY_ArrowRight", {}, view.styleWindow);
  EventUtils.synthesizeKey("V", { accelKey: true }, view.styleWindow);
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);

  await onDone;

  info("Check copy-pasting the property value is not breaking it");

  is(
    gridRuleProperty.innerText,
    typedAndCopiedMultiLinesInnerText + " " + typedAndCopiedMultiLinesInnerText,
    "copy-pasting the current value duplicate the correct value, with each string of the multi strings grid-template-areas value is displayed on a new line"
  );

  // test that when "non grid-template-area", like quotes for example, its multi-string value is not displayed over multiple lines
  await selectNode("#testid-quotes", inspector);

  info(
    "Tests display of content string value is NOT in an ascii-art style," +
      "displaying each string on a single line"
  );

  const contentRuleProperty = await getRuleViewProperty(
    view,
    "#testid-quotes",
    "quotes"
  ).valueSpan;
  is(
    contentRuleProperty.innerText,
    '"«" "»" "‹" "›"',
    "the quotes strings values are all displayed on the same single line"
  );

  // test that when invalid strings values do not get formatted
  info("testing it does not try to format invalid values");
  await selectNode("#testid-invalid-strings", inspector);
  const invalidGridRuleProperty = await getRuleViewProperty(
    view,
    "#testid-invalid-strings",
    "grid-template-areas"
  ).valueSpan;
  is(
    invalidGridRuleProperty.innerText,
    '"a a b" "a a"',
    "the invalid strings values do not get formatted"
  );

  // test that when a valid value without quotes such as `inherit` it does not get formatted
  info("testing it does not try to format valid non-quote values");
  await selectNode("#testid-valid-quotefree-value", inspector);
  const validGridRuleNoQuoteProperty = await getRuleViewProperty(
    view,
    "#testid-valid-quotefree-value",
    "grid-template-areas"
  ).valueSpan;
  is(
    validGridRuleNoQuoteProperty.innerText,
    "inherit",
    "the valid quote-free values do not get formatted"
  );
});
