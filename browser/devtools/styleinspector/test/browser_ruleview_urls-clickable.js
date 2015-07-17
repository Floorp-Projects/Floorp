/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests to make sure that URLs are clickable in the rule view

const TEST_URI = TEST_URL_ROOT + "doc_urls_clickable.html";
const TEST_IMAGE = TEST_URL_ROOT + "doc_test_image.png";
const BASE_64_URL = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";

add_task(function*() {
  yield addTab(TEST_URI);
  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNodes(inspector, view);
});

function* selectNodes(inspector, ruleView) {
  let relative1 = ".relative1";
  let relative2 = ".relative2";
  let absolute = ".absolute";
  let inline = ".inline";
  let base64 = ".base64";
  let noimage = ".noimage";
  let inlineresolved = ".inline-resolved";

  yield selectNode(relative1, inspector);
  let relativeLink = ruleView.styleDocument.querySelector(".ruleview-propertyvaluecontainer a");
  ok(relativeLink, "Link exists for relative1 node");
  is(relativeLink.getAttribute("href"), TEST_IMAGE, "href matches");

  yield selectNode(relative2, inspector);
  relativeLink = ruleView.styleDocument.querySelector(".ruleview-propertyvaluecontainer a");
  ok(relativeLink, "Link exists for relative2 node");
  is(relativeLink.getAttribute("href"), TEST_IMAGE, "href matches");

  yield selectNode(absolute, inspector);
  let absoluteLink = ruleView.styleDocument.querySelector(".ruleview-propertyvaluecontainer a");
  ok(absoluteLink, "Link exists for absolute node");
  is(absoluteLink.getAttribute("href"), TEST_IMAGE, "href matches");

  yield selectNode(inline, inspector);
  let inlineLink = ruleView.styleDocument.querySelector(".ruleview-propertyvaluecontainer a");
  ok(inlineLink, "Link exists for inline node");
  is(inlineLink.getAttribute("href"), TEST_IMAGE, "href matches");

  yield selectNode(base64, inspector);
  let base64Link = ruleView.styleDocument.querySelector(".ruleview-propertyvaluecontainer a");
  ok(base64Link, "Link exists for base64 node");
  is(base64Link.getAttribute("href"), BASE_64_URL, "href matches");

  yield selectNode(inlineresolved, inspector);
  let inlineResolvedLink = ruleView.styleDocument.querySelector(".ruleview-propertyvaluecontainer a");
  ok(inlineResolvedLink, "Link exists for style tag node");
  is(inlineResolvedLink.getAttribute("href"), TEST_IMAGE, "href matches");

  yield selectNode(noimage, inspector);
  let noimageLink = ruleView.styleDocument.querySelector(".ruleview-propertyvaluecontainer a");
  ok(!noimageLink, "There is no link for the node with no background image");
}
