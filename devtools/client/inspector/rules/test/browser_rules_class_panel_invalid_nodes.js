/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the class panel shows a message when invalid nodes are selected.
// text nodes, pseudo-elements, DOCTYPE, comment nodes.

add_task(function* () {
  yield addTab(`data:text/html;charset=utf-8,
    <body>
    <style>div::after {content: "test";}</style>
    <!-- comment -->
    Some text
    <div></div>
    </body>`);

  info("Open the class panel");
  let {inspector, view} = yield openRuleView();
  view.showClassPanel();

  info("Selecting the DOCTYPE node");
  let {nodes} = yield inspector.walker.children(inspector.walker.rootNode);
  yield selectNode(nodes[0], inspector);
  checkMessageIsDisplayed(view);

  info("Selecting the comment node");
  let styleNode = yield getNodeFront("style", inspector);
  let commentNode = yield inspector.walker.nextSibling(styleNode);
  yield selectNode(commentNode, inspector);
  checkMessageIsDisplayed(view);

  info("Selecting the text node");
  let textNode = yield inspector.walker.nextSibling(commentNode);
  yield selectNode(textNode, inspector);
  checkMessageIsDisplayed(view);

  info("Selecting the ::after pseudo-element");
  let divNode = yield getNodeFront("div", inspector);
  let pseudoElement = (yield inspector.walker.children(divNode)).nodes[0];
  yield selectNode(pseudoElement, inspector);
  checkMessageIsDisplayed(view);
});

function checkMessageIsDisplayed(view) {
  ok(view.classListPreviewer.classesEl.querySelector(".no-classes"),
     "The message is displayed");
  checkClassPanelContent(view, []);
}
