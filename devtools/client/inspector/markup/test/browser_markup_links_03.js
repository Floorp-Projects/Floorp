/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that links appear correctly in attributes created in content.

const TEST_URL = URL_ROOT + "doc_markup_links.html";

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);

  info("Adding a contextmenu attribute to the body node via the content");
  let onMutated = inspector.once("markupmutation");
  yield testActor.setAttribute("body", "contextmenu", "menu1");
  yield onMutated;

  info("Checking for links in the new attribute");
  let {editor} = yield getContainerForSelector("body", inspector);
  let linkEls = editor.attrElements.get("contextmenu")
                                   .querySelectorAll(".link");
  is(linkEls.length, 1, "There is one link in the contextmenu attribute");
  is(linkEls[0].dataset.type, "idref", "The link has the right type");
  is(linkEls[0].textContent, "menu1", "The link has the right value");

  info("Editing the contextmenu attribute on the body node");
  onMutated = inspector.once("markupmutation");
  yield testActor.setAttribute("body", "contextmenu", "menu2");
  yield onMutated;

  info("Checking for links in the updated attribute");
  ({editor} = yield getContainerForSelector("body", inspector));
  linkEls = editor.attrElements.get("contextmenu").querySelectorAll(".link");
  is(linkEls.length, 1, "There is one link in the contextmenu attribute");
  is(linkEls[0].dataset.type, "idref", "The link has the right type");
  is(linkEls[0].textContent, "menu2", "The link has the right value");
});
