/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check display of custom formatters in debugger.
const TEST_FILENAME = `doc_dbg-custom-formatters.html`;

add_task(async function() {
  // TODO: This preference can be removed once the custom formatters feature is stable enough
  await pushPref("devtools.custom-formatters", true);
  await pushPref("devtools.custom-formatters.enabled", true);

  const dbg = await initDebugger(TEST_FILENAME);
  await selectSource(dbg, TEST_FILENAME);

  info(
    "Test that custom formatted objects are displayed as expected in the Watch Expressions panel"
  );
  await addExpression(dbg, `({customFormatHeaderAndBody: "body"})`);
  const expressionsElements = findAllElements(dbg, "expressionNodes");
  is(expressionsElements.length, 1);

  const customFormattedElement = expressionsElements[0];

  const headerJsonMlNode = customFormattedElement.querySelector(
    ".objectBox-jsonml"
  );
  is(
    headerJsonMlNode.innerText,
    "CUSTOM",
    "The object is rendered with a custom formatter"
  );

  is(
    customFormattedElement.querySelector(".arrow"),
    null,
    "The expression is not expandable…"
  );
  const customFormattedElementArrow = customFormattedElement.querySelector(
    ".collapse-button"
  );
  ok(customFormattedElementArrow, "… but the custom formatter is");

  info("Expanding the Object");
  const onBodyRendered = waitFor(
    () =>
      !!customFormattedElement.querySelector(
        ".objectBox-jsonml-body-wrapper .objectBox-jsonml"
      )
  );

  customFormattedElementArrow.click();
  await onBodyRendered;

  ok(
    customFormattedElement
      .querySelector(".collapse-button")
      .classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  const bodyJsonMlNode = customFormattedElement.querySelector(
    ".objectBox-jsonml-body-wrapper > .objectBox-jsonml"
  );
  ok(bodyJsonMlNode, "The body is custom formatted");
  is(bodyJsonMlNode?.textContent, "body", "The body text is correct");
});
