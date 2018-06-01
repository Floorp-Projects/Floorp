/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the class panel shows a message when invalid nodes are selected.
// text nodes, pseudo-elements, DOCTYPE, comment nodes.

add_task(async function() {
  await addTab(`data:text/html;charset=utf-8,
    <body>
    <style>div::after {content: "test";}</style>
    <!-- comment -->
    Some text
    <div></div>
    </body>`);

  info("Open the class panel");
  const {inspector, view} = await openRuleView();
  view.showClassPanel();

  info("Selecting the DOCTYPE node");
  const {nodes} = await inspector.walker.children(inspector.walker.rootNode);
  await selectNode(nodes[0], inspector);
  checkMessageIsDisplayed(view);

  info("Selecting the comment node");
  const styleNode = await getNodeFront("style", inspector);
  const commentNode = await inspector.walker.nextSibling(styleNode);
  await selectNode(commentNode, inspector);
  checkMessageIsDisplayed(view);

  info("Selecting the text node");
  const textNode = await inspector.walker.nextSibling(commentNode);
  await selectNode(textNode, inspector);
  checkMessageIsDisplayed(view);

  info("Selecting the ::after pseudo-element");
  const divNode = await getNodeFront("div", inspector);
  const pseudoElement = (await inspector.walker.children(divNode)).nodes[0];
  await selectNode(pseudoElement, inspector);
  checkMessageIsDisplayed(view);
});

function checkMessageIsDisplayed(view) {
  ok(view.classListPreviewer.classesEl.querySelector(".no-classes"),
     "The message is displayed");
  checkClassPanelContent(view, []);
}
