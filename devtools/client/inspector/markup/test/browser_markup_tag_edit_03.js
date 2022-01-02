/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a node's tagname can be edited in the markup-view

const TEST_URL = `data:text/html;charset=utf-8,
                  <div id='retag-me'><div id='retag-me-2'></div></div>`;

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  await inspector.markup.expandAll();

  info("Selecting the test node");
  await focusNode("#retag-me", inspector);

  info("Getting the markup-container for the test node");
  let container = await getContainerForSelector("#retag-me", inspector);
  ok(container.expanded, "The container is expanded");

  is(
    (await getContentPageElementProperty("#retag-me", "tagName")).toLowerCase(),
    "div",
    "We've got #retag-me element, it's a DIV"
  );
  is(
    await getContentPageElementProperty("#retag-me", "childElementCount"),
    1,
    "#retag-me has one child"
  );

  is(
    await getContentPageElementProperty("#retag-me > *", "id"),
    "retag-me-2",
    "#retag-me's only child is #retag-me-2"
  );

  info("Changing #retag-me's tagname in the markup-view");
  const mutated = inspector.once("markupmutation");
  const tagEditor = container.editor.tag;
  setEditableFieldValue(tagEditor, "p", inspector);
  await mutated;

  info("Checking that the markup-container exists and is correct");
  container = await getContainerForSelector("#retag-me", inspector);
  ok(container.expanded, "The container is still expanded");
  ok(container.selected, "The container is still selected");

  info("Checking that the tagname change was done");

  is(
    (await getContentPageElementProperty("#retag-me", "tagName")).toLowerCase(),
    "p",
    "The #retag-me element is now a P"
  );
  is(
    await getContentPageElementProperty("#retag-me", "childElementCount"),
    1,
    "#retag-me still has one child"
  );
  is(
    await getContentPageElementProperty("#retag-me > *", "id"),
    "retag-me-2",
    "#retag-me's only child is #retag-me-2"
  );
});
