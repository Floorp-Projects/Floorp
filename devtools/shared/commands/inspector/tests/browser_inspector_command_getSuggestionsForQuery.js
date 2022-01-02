/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  // Build a test page with a remote iframe, using two distinct origins .com and .org
  const iframeHtml = encodeURIComponent(`<div id="iframe"></div>`);
  const html = encodeURIComponent(
    `<div class="foo bar">
       <div id="child"></div>
     </div>
     <iframe src="https://example.org/document-builder.sjs?html=${iframeHtml}"></iframe>`
  );
  const tab = await addTab(
    "https://example.com/document-builder.sjs?html=" + html
  );

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  info(
    "Suggestions for 'di' with tag search, will match the two <div> elements in top document and the one in the iframe"
  );
  await assertSuggestion(
    commands,
    { query: "", firstPart: "di", state: "tag" },
    [
      {
        suggestion: "div",
        count: 3, // Matches the two <div> in the top document and the one in the iframe
        type: "tag",
      },
    ]
  );

  info(
    "Suggestions for 'ifram' with id search, will only match the <div> within the iframe"
  );
  await assertSuggestion(
    commands,
    { query: "", firstPart: "ifram", state: "id" },
    [
      {
        suggestion: "#iframe",
        count: 1,
        type: "id",
      },
    ]
  );

  info(
    "Suggestions for 'fo' with tag search, will match the class of the top <div> element"
  );
  await assertSuggestion(
    commands,
    { query: "", firstPart: "fo", state: "tag" },
    [
      {
        suggestion: ".foo",
        count: 1,
        type: "class",
      },
    ]
  );

  info(
    "Suggestions for classes, based on div elements, will match the two classes of top <div> element"
  );
  await assertSuggestion(
    commands,
    { query: "div", firstPart: "", state: "class" },
    [
      {
        suggestion: ".bar",
        count: 1,
        type: "class",
      },
      {
        suggestion: ".foo",
        count: 1,
        type: "class",
      },
    ]
  );

  info("Suggestion for non-existent tag names will return no suggestion");
  await assertSuggestion(
    commands,
    { query: "", firstPart: "marquee", state: "tag" },
    []
  );

  await commands.destroy();
});

async function assertSuggestion(
  commands,
  { query, firstPart, state },
  expectedSuggestions
) {
  const suggestions = await commands.inspectorCommand.getSuggestionsForQuery(
    query,
    firstPart,
    state
  );
  is(
    suggestions.length,
    expectedSuggestions.length,
    "Got the expected number of suggestions"
  );
  for (let i = 0; i < expectedSuggestions.length; i++) {
    info(` ## Asserting suggestion #${i}:`);
    const expectedSuggestion = expectedSuggestions[i];
    const [suggestion, count, type] = suggestions[i];
    is(
      suggestion,
      expectedSuggestion.suggestion,
      "The suggested string is valid"
    );
    is(count, expectedSuggestion.count, "The number of matches is valid");
    is(type, expectedSuggestion.type, "The type of match is valid");
  }
}
