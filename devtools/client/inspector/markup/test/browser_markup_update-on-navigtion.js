/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that markup view handles page navigation correctly.

const URL_1 = URL_ROOT + "doc_markup_update-on-navigtion_1.html";
const URL_2 = URL_ROOT + "doc_markup_update-on-navigtion_2.html";

add_task(async function() {
  const { inspector } = await openInspectorForURL(URL_1);

  assertMarkupViewIsLoaded();
  await selectNode("#one", inspector);

  const willNavigate = inspector.currentTarget.once("will-navigate");

  // We should not await on navigateTo here, because the test will assert the
  // various phases of the inspector during the navigation.
  const onNavigated = navigateTo(URL_2);

  info("Waiting for will-navigate");
  await willNavigate;

  info("Navigation to page 2 has started, the inspector should be empty");
  assertMarkupViewIsEmpty();

  info("Waiting for new-root");
  await inspector.once("new-root");

  info("Navigation to page 2 was done, the inspector should be back up");
  assertMarkupViewIsLoaded();

  await onNavigated;
  await selectNode("#two", inspector);

  function assertMarkupViewIsLoaded() {
    const markupViewBox = inspector.panelDoc.getElementById("markup-box");
    is(markupViewBox.childNodes.length, 1, "The markup-view is loaded");
  }

  function assertMarkupViewIsEmpty() {
    const markupViewFrame = inspector._markupFrame.contentDocument.getElementById(
      "root"
    );
    is(markupViewFrame.childNodes.length, 0, "The markup-view is unloaded");
  }
});
