/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests to make sure that URLs are clickable in the rule view

let doc;
let computedView;
let inspector;

const BASE_URL = "http://example.com/browser/browser/" +
                 "devtools/styleinspector/test/";
const TEST_URI = BASE_URL +
                 "browser_styleinspector_bug_677930_urls_clickable.html";
const TEST_IMAGE = BASE_URL + "test-image.png";
const BASE_64_URL = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";

function setNode(node, inspector)
{
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNode(node);
  return updated;
}

function selectNodes(aInspector, aRuleView)
{
  inspector = aInspector;
  let sidebar = inspector.sidebar;
  let contentDoc = aRuleView.doc;

  let relative1 = doc.querySelector(".relative1");
  let relative2 = doc.querySelector(".relative2");
  let absolute = doc.querySelector(".absolute");
  let inline = doc.querySelector(".inline");
  let base64 = doc.querySelector(".base64");
  let noimage = doc.querySelector(".noimage");
  let inlineresolved = doc.querySelector(".inline-resolved");

  ok(relative1, "captain, we have the relative1 div");
  ok(relative2, "captain, we have the relative2 div");
  ok(absolute, "captain, we have the absolute div");
  ok(inline, "captain, we have the inline div");
  ok(base64, "captain, we have the base64 div");
  ok(noimage, "captain, we have the noimage div");
  ok(inlineresolved, "captain, we have the inlineresolved div");


  Task.spawn(function*() {

    yield setNode(relative1, inspector);
    is(inspector.selection.node, relative1, "selection matches the relative1 element");
    let relativeLink = contentDoc.querySelector(".ruleview-propertycontainer a");
    ok (relativeLink, "Link exists for relative1 node");
    is (relativeLink.getAttribute("href"), TEST_IMAGE, "href matches");

    yield setNode(relative2, inspector);
    is(inspector.selection.node, relative2, "selection matches the relative2 element");
    let relativeLink = contentDoc.querySelector(".ruleview-propertycontainer a");
    ok (relativeLink, "Link exists for relative2 node");
    is (relativeLink.getAttribute("href"), TEST_IMAGE, "href matches");

    yield setNode(absolute, inspector);
    is(inspector.selection.node, absolute, "selection matches the absolute element");
    let absoluteLink = contentDoc.querySelector(".ruleview-propertycontainer a");
    ok (absoluteLink, "Link exists for absolute node");
    is (absoluteLink.getAttribute("href"), TEST_IMAGE, "href matches");

    yield setNode(inline, inspector);
    is(inspector.selection.node, inline, "selection matches the inline element");
    let inlineLink = contentDoc.querySelector(".ruleview-propertycontainer a");
    ok (inlineLink, "Link exists for inline node");
    is (inlineLink.getAttribute("href"), TEST_IMAGE, "href matches");

    yield setNode(base64, inspector);
    is(inspector.selection.node, base64, "selection matches the base64 element");
    let base64Link = contentDoc.querySelector(".ruleview-propertycontainer a");
    ok (base64Link, "Link exists for base64 node");
    is (base64Link.getAttribute("href"), BASE_64_URL, "href matches");

    yield setNode(inlineresolved, inspector);
    is(inspector.selection.node, inlineresolved, "selection matches the style tag element");
    let inlineResolvedLink = contentDoc.querySelector(".ruleview-propertycontainer a");
    ok (inlineResolvedLink, "Link exists for style tag node");
    is (inlineResolvedLink.getAttribute("href"), TEST_IMAGE, "href matches");

    yield setNode(noimage, inspector);
    is(inspector.selection.node, noimage, "selection matches the inline element");
    let noimageLink = contentDoc.querySelector(".ruleview-propertycontainer a");
    ok (!noimageLink, "There is no link for the node with no background image");

    finishUp();
  });
}

function finishUp()
{
  doc = computedView = inspector = null;
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    doc = content.document;
    waitForFocus(() => openRuleView(selectNodes), content);
  }, true);

  content.location = TEST_URI;
}
