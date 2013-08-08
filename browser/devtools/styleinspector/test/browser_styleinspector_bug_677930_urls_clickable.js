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

function selectNode(aInspector, aRuleView)
{
  inspector = aInspector;
  let sidebar = inspector.sidebar;
  let contentDoc = aRuleView.doc;

  let relative = doc.querySelector(".relative");
  let absolute = doc.querySelector(".absolute");
  let inline = doc.querySelector(".inline");
  let base64 = doc.querySelector(".base64");
  let noimage = doc.querySelector(".noimage");

  ok(relative, "captain, we have the relative div");
  ok(absolute, "captain, we have the absolute div");
  ok(inline, "captain, we have the inline div");
  ok(base64, "captain, we have the base64 div");
  ok(noimage, "captain, we have the noimage div");

  inspector.selection.setNode(relative);
  inspector.once("inspector-updated", () => {
    is(inspector.selection.node, relative, "selection matches the relative element");
    let relativeLink = contentDoc.querySelector(".ruleview-propertycontainer a");
    ok (relativeLink, "Link exists for relative node");
    ok (relativeLink.getAttribute("href"), TEST_IMAGE);

    inspector.selection.setNode(absolute);
    inspector.once("inspector-updated", () => {
      is(inspector.selection.node, absolute, "selection matches the absolute element");
      let absoluteLink = contentDoc.querySelector(".ruleview-propertycontainer a");
      ok (absoluteLink, "Link exists for absolute node");
      ok (absoluteLink.getAttribute("href"), TEST_IMAGE);

      inspector.selection.setNode(inline);
      inspector.once("inspector-updated", () => {
        is(inspector.selection.node, inline, "selection matches the inline element");
        let inlineLink = contentDoc.querySelector(".ruleview-propertycontainer a");
        ok (inlineLink, "Link exists for inline node");
        ok (inlineLink.getAttribute("href"), TEST_IMAGE);

        inspector.selection.setNode(base64);
        inspector.once("inspector-updated", () => {
          is(inspector.selection.node, base64, "selection matches the base64 element");
          let base64Link = contentDoc.querySelector(".ruleview-propertycontainer a");
          ok (base64Link, "Link exists for base64 node");
          ok (base64Link.getAttribute("href"), BASE_64_URL);

          inspector.selection.setNode(noimage);
          inspector.once("inspector-updated", () => {
            is(inspector.selection.node, noimage, "selection matches the inline element");
            let noimageLink = contentDoc.querySelector(".ruleview-propertycontainer a");
            ok (!noimageLink, "There is no link for the node with no background image");
            finishUp();
          });
        });
      });
    });
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
    waitForFocus(() => openRuleView(selectNode), content);
  }, true);

  content.location = TEST_URI;
}
