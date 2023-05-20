/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Bug 1830111 - Test that searching elements while having hidden <iframe> works

const HTML = `
  <!DOCTYPE html>
  <html>
    <body>
      <!-- The nested iframe, will be a children node of the top iframe
           but won't be displayed, not considered as valid children by the inspector -->
      <iframe><iframe></iframe></iframe>
      <div>after iframe</<div>
      <script>
        document.querySelector("iframe").appendChild(document.createElement("iframe"));
      </script>
    </body>
  </html>
`;

const TEST_URI = "data:text/html;charset=utf-8," + encodeURI(HTML);

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URI);

  await focusSearchBoxUsingShortcut(inspector.panelWin);

  const onSearchProcessingDone =
    inspector.searchSuggestions.once("processing-done");
  synthesizeKeys("div", inspector.panelWin);
  info("Waiting for search query to complete");
  await onSearchProcessingDone;

  const popup = inspector.searchSuggestions.searchPopup;
  const actualSuggestions = popup.getItems().map(item => item.label);
  Assert.deepEqual(
    actualSuggestions,
    ["div"],
    "autocomplete popup displays the right suggestions"
  );

  const onSearchResult = inspector.search.once("search-result");
  EventUtils.synthesizeKey("VK_RETURN", {}, inspector.panelWin);

  info("Waiting for results");
  await onSearchResult;

  const nodeFront = await getNodeFront("div", inspector);
  is(inspector.selection.nodeFront, nodeFront, "The <div> element is selected");
});
