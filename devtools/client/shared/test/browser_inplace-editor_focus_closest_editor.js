/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_inplace_editor.js */

"use strict";

loadHelperScript("helper_inplace_editor.js");

// Test the inplace-editor behavior with focusEditableFieldAfterApply
// and focusEditableFieldContainerSelector options

add_task(async function () {
  await addTab(
    "data:text/html;charset=utf-8,inline editor focusEditableFieldAfterApply"
  );
  const { host, doc } = await createHost();

  testFocusNavigationWithMultipleEditor(doc);
  testFocusNavigationWithNonMatchingFocusEditableFieldContainerSelector(doc);
  testMissingFocusEditableFieldContainerSelector(doc);

  host.destroy();
  gBrowser.removeCurrentTab();
});

function testFocusNavigationWithMultipleEditor(doc) {
  // For some reason <button> or <input> are not rendered, so let's use divs with
  // tabindex attribute to make them focusable.
  doc.body.innerHTML = `
      <main>
        <header>
          <div role=button tabindex=0 id="header-button">HEADER</div>
        </header>
        <section>
          <span id="span-1" tabindex=0>SPAN 1</span>
          <div role=button tabindex=0 id="section-button-1">BUTTON 1</div>
          <p>
            <span id="span-2" tabindex=0>SPAN 2</span>
            <div role=button tabindex=0 id="section-button-2">BUTTON 2</div>
          </p>
          <span id="span-3" tabindex=0>SPAN 3</span>
          <div role=button tabindex=0 id="section-button-3">BUTTON 3</div>
        </section>
        <sidebar>
          <div role=button tabindex=0 id="sidebar-button">SIDEBAR</div>
        </sidebar>
      <main>`;

  const span1 = doc.getElementById("span-1");
  const span2 = doc.getElementById("span-2");
  const span3 = doc.getElementById("span-3");

  info("Create 3 editable fields for the 3 spans inside the main element");
  const options = {
    focusEditableFieldAfterApply: true,
    focusEditableFieldContainerSelector: "main",
  };
  editableField({
    element: span1,
    ...options,
  });
  editableField({
    element: span2,
    ...options,
  });
  editableField({
    element: span3,
    ...options,
  });

  span1.click();

  is(
    doc.activeElement.inplaceEditor.elt.id,
    "span-1",
    "Visible editable field is the one associated with span-1"
  );
  assertFocusedElementInplaceEditorInput(doc);

  EventUtils.sendKey("Tab");

  is(
    doc.activeElement.inplaceEditor.elt.id,
    "span-2",
    "Using Tab moved focus to span-2 editable field"
  );
  assertFocusedElementInplaceEditorInput(doc);

  EventUtils.sendKey("Tab");

  is(
    doc.activeElement.inplaceEditor.elt.id,
    "span-3",
    "Using Tab moved focus to span-3 editable field"
  );
  assertFocusedElementInplaceEditorInput(doc);

  EventUtils.sendKey("Tab");

  is(
    doc.activeElement.id,
    "sidebar-button",
    "Using Tab moved focus outside of <main>"
  );
}

function testFocusNavigationWithNonMatchingFocusEditableFieldContainerSelector(
  doc
) {
  // For some reason <button> or <input> are not rendered, so let's use divs with
  // tabindex attribute to make them focusable.
  doc.body.innerHTML = `
      <main>
        <span id="span-1" tabindex=0>SPAN 1</span>
        <div role=button tabindex=0 id="section-button-1">BUTTON 1</div>
        <span id="span-2" tabindex=0>SPAN 2</span>
        <div role=button tabindex=0 id="section-button-2">BUTTON 2</div>
      <main>`;

  const span1 = doc.getElementById("span-1");
  const span2 = doc.getElementById("span-2");

  info(
    "Create editable fields for the 2 spans, but pass a focusEditableFieldContainerSelector that does not match any element"
  );
  const options = {
    focusEditableFieldAfterApply: true,
    focusEditableFieldContainerSelector: ".does-not-exist",
  };
  editableField({
    element: span1,
    ...options,
  });
  editableField({
    element: span2,
    ...options,
  });

  span1.click();

  is(
    doc.activeElement.inplaceEditor.elt.id,
    "span-1",
    "Visible editable field is the one associated with span-1"
  );
  assertFocusedElementInplaceEditorInput(doc);

  EventUtils.sendKey("Tab");

  is(
    doc.activeElement.id,
    "section-button-1",
    "Using Tab moved focus to section-button-1, non-editable field"
  );
  ok(!doc.querySelector("input"), "There's no editable input displayed");
}

function testMissingFocusEditableFieldContainerSelector(doc) {
  doc.body.innerHTML = "";
  const element = createSpan(doc);
  editableField({
    element,
    focusEditableFieldAfterApply: true,
  });

  element.click();
  is(
    element.style.display,
    "inline-block",
    "The original <span> was not hidden"
  );
  ok(!doc.querySelector("input"), "No input was created");
}

function assertFocusedElementInplaceEditorInput(doc) {
  ok(
    doc.activeElement.matches("input.styleinspector-propertyeditor"),
    "inplace editor input is focused"
  );
}
