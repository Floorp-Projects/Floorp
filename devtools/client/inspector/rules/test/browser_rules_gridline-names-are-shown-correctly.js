/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the text editor correctly calculates the grid line names shown in the
// autocomplete popup. We generally want to show all the grid line names for a grid
// container, except for implicit line names created by an implicitly named area.

const TEST_URL = URL_ROOT + "doc_grid_area_gridline_names.html";

add_task(async function () {
  await addTab(TEST_URL);
  const { inspector, view } = await openRuleView();

  info(
    "Test that implicit grid line names from explicit grid areas are shown."
  );
  await testExplicitNamedAreas(inspector, view);

  info(
    "Test that explicit grid line names creating implicit grid areas are shown."
  );
  await testImplicitNamedAreasWithExplicitGridLineNames(inspector, view);

  info(
    "Test that implicit grid line names creating implicit grid areas are not shown."
  );
  await testImplicitAreasWithImplicitGridLineNames(inspector, view);
  await testImplicitNamedAreasWithReversedGridLineNames(inspector, view);
});

async function testExplicitNamedAreas(inspector, view) {
  await selectNode(".header", inspector);

  const gridColLines = ["header-start", "header-end", "main-start", "main-end"];

  const propertyName = view.styleDocument.querySelectorAll(
    ".ruleview-propertyvalue"
  )[0];
  const gridLineNamesUpdated = inspector.once("grid-line-names-updated");
  const editor = await focusEditableField(view, propertyName);
  const onPopupShown = once(editor.popup, "popup-opened");
  await gridLineNamesUpdated;

  EventUtils.synthesizeKey("VK_DOWN", { shiftKey: true }, view.styleWindow);

  await onPopupShown;

  info(
    "Check that the expected grid line column names are shown in the editor popup."
  );
  for (const lineName of gridColLines) {
    Assert.greater(
      editor.gridLineNames.cols.indexOf(lineName),
      -1,
      `${lineName} is a suggested implicit grid line`
    );
  }
}

async function testImplicitNamedAreasWithExplicitGridLineNames(
  inspector,
  view
) {
  await selectNode(".contentArea", inspector);

  const gridRowLines = [
    "main-start",
    "main-end",
    "content-start",
    "content-end",
  ];

  const propertyName = view.styleDocument.querySelectorAll(
    ".ruleview-propertyvalue"
  )[1];
  const gridLineNamesUpdated = inspector.once("grid-line-names-updated");
  const editor = await focusEditableField(view, propertyName);
  const onPopupShown = once(editor.popup, "popup-opened");
  await gridLineNamesUpdated;

  EventUtils.synthesizeKey("VK_DOWN", { shiftKey: true }, view.styleWindow);

  await onPopupShown;

  info(
    "Check that the expected grid line row names are shown in the editor popup."
  );
  for (const lineName of gridRowLines) {
    Assert.greater(
      editor.gridLineNames.rows.indexOf(lineName),
      -1,
      `${lineName} is a suggested explicit grid line`
    );
  }
}

async function testImplicitAreasWithImplicitGridLineNames(inspector, view) {
  await selectNode(".a", inspector);

  const propertyName = view.styleDocument.querySelectorAll(
    ".ruleview-propertyvalue"
  )[0];
  const gridLineNamesUpdated = inspector.once("grid-line-names-updated");
  const editor = await focusEditableField(view, propertyName);
  const onPopupShown = once(editor.popup, "popup-opened");
  await gridLineNamesUpdated;

  EventUtils.synthesizeKey("VK_DOWN", { shiftKey: true }, view.styleWindow);

  await onPopupShown;

  info(
    "Check that the implicit grid lines created by implicit areas are not shown."
  );
  ok(
    !(editor.gridLineNames.cols.indexOf("a-end") > -1),
    "a-end is not shown because it is created by an implicit named area."
  );
}

async function testImplicitNamedAreasWithReversedGridLineNames(
  inspector,
  view
) {
  await selectNode(".b", inspector);

  const propertyName = view.styleDocument.querySelectorAll(
    ".ruleview-propertyvalue"
  )[0];
  const gridLineNamesUpdated = inspector.once("grid-line-names-updated");
  const editor = await focusEditableField(view, propertyName);
  const onPopupShown = once(editor.popup, "popup-opened");
  await gridLineNamesUpdated;

  EventUtils.synthesizeKey("VK_DOWN", { shiftKey: true }, view.styleWindow);

  await onPopupShown;

  info(
    "Test that reversed implicit grid line names from implicit areas are not shown"
  );
  ok(
    !(editor.gridLineNames.cols.indexOf("b-start") > -1),
    "b-start is not shown because it is created by an implicit named area."
  );
}
