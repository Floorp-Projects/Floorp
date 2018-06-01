/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that attributes are linkified correctly when attributes are updated
// and created.

const TEST_URL = URL_ROOT + "doc_markup_links.html";

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Adding a contextmenu attribute to the body node");
  await addNewAttributes("body", "contextmenu=\"menu1\"", inspector);

  info("Checking for links in the new attribute");
  let {editor} = await getContainerForSelector("body", inspector);
  let linkEls = editor.attrElements.get("contextmenu")
                                   .querySelectorAll(".link");
  is(linkEls.length, 1, "There is one link in the contextmenu attribute");
  is(linkEls[0].dataset.type, "idref", "The link has the right type");
  is(linkEls[0].textContent, "menu1", "The link has the right value");

  info("Editing the contextmenu attribute on the body node");
  const nodeMutated = inspector.once("markupmutation");
  const attr = editor.attrElements.get("contextmenu").querySelector(".editable");
  setEditableFieldValue(attr, "contextmenu=\"menu2\"", inspector);
  await nodeMutated;

  info("Checking for links in the updated attribute");
  ({editor} = await getContainerForSelector("body", inspector));
  linkEls = editor.attrElements.get("contextmenu").querySelectorAll(".link");
  is(linkEls.length, 1, "There is one link in the contextmenu attribute");
  is(linkEls[0].dataset.type, "idref", "The link has the right type");
  is(linkEls[0].textContent, "menu2", "The link has the right value");
});
