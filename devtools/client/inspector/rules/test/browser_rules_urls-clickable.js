/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests to make sure that URLs are clickable in the rule view

const TEST_URI = URL_ROOT_SSL + "doc_urls_clickable.html";
const TEST_IMAGE = URL_ROOT_SSL + "doc_test_image.png";
const BASE_64_URL =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAA" +
  "FCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAA" +
  "BJRU5ErkJggg==";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();
  await selectNodes(inspector, view);
});

async function selectNodes(inspector, ruleView) {
  const relative1 = ".relative1";
  const relative2 = ".relative2";
  const absolute = ".absolute";
  const inline = ".inline";
  const base64 = ".base64";
  const noimage = ".noimage";
  const inlineresolved = ".inline-resolved";

  await selectNode(relative1, inspector);
  let relativeLink = ruleView.styleDocument.querySelector(
    ".ruleview-propertyvaluecontainer a"
  );
  ok(relativeLink, "Link exists for relative1 node");
  is(relativeLink.getAttribute("href"), TEST_IMAGE, "href matches");

  await selectNode(relative2, inspector);
  relativeLink = ruleView.styleDocument.querySelector(
    ".ruleview-propertyvaluecontainer a"
  );
  ok(relativeLink, "Link exists for relative2 node");
  is(relativeLink.getAttribute("href"), TEST_IMAGE, "href matches");

  await selectNode(absolute, inspector);
  const absoluteLink = ruleView.styleDocument.querySelector(
    ".ruleview-propertyvaluecontainer a"
  );
  ok(absoluteLink, "Link exists for absolute node");
  is(absoluteLink.getAttribute("href"), TEST_IMAGE, "href matches");

  await selectNode(inline, inspector);
  const inlineLink = ruleView.styleDocument.querySelector(
    ".ruleview-propertyvaluecontainer a"
  );
  ok(inlineLink, "Link exists for inline node");
  is(inlineLink.getAttribute("href"), TEST_IMAGE, "href matches");

  await selectNode(base64, inspector);
  const base64Link = ruleView.styleDocument.querySelector(
    ".ruleview-propertyvaluecontainer a"
  );
  ok(base64Link, "Link exists for base64 node");
  is(base64Link.getAttribute("href"), BASE_64_URL, "href matches");

  await selectNode(inlineresolved, inspector);
  const inlineResolvedLink = ruleView.styleDocument.querySelector(
    ".ruleview-propertyvaluecontainer a"
  );
  ok(inlineResolvedLink, "Link exists for style tag node");
  is(inlineResolvedLink.getAttribute("href"), TEST_IMAGE, "href matches");

  await selectNode(noimage, inspector);
  const noimageLink = ruleView.styleDocument.querySelector(
    ".ruleview-propertyvaluecontainer a"
  );
  ok(!noimageLink, "There is no link for the node with no background image");
}
