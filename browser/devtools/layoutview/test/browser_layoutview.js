/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Expected values:
let res1 = [
      {selector: "#element-size",              value: "160x160"},
      {selector: ".size > span",               value: "100x100"},
      {selector: ".margin.top > span",         value: 30},
      {selector: ".margin.left > span",        value: "auto"},
      {selector: ".margin.bottom > span",      value: 30},
      {selector: ".margin.right > span",       value: "auto"},
      {selector: ".padding.top > span",        value: 20},
      {selector: ".padding.left > span",       value: 20},
      {selector: ".padding.bottom > span",     value: 20},
      {selector: ".padding.right > span",      value: 20},
      {selector: ".border.top > span",         value: 10},
      {selector: ".border.left > span",        value: 10},
      {selector: ".border.bottom > span",      value: 10},
      {selector: ".border.right > span",       value: 10},
];

let res2 = [
      {selector: "#element-size",              value: "190x210"},
      {selector: ".size > span",               value: "100x150"},
      {selector: ".margin.top > span",         value: 30},
      {selector: ".margin.left > span",        value: "auto"},
      {selector: ".margin.bottom > span",      value: 30},
      {selector: ".margin.right > span",       value: "auto"},
      {selector: ".padding.top > span",        value: 20},
      {selector: ".padding.left > span",       value: 20},
      {selector: ".padding.bottom > span",     value: 20},
      {selector: ".padding.right > span",      value: 50},
      {selector: ".border.top > span",         value: 10},
      {selector: ".border.left > span",        value: 10},
      {selector: ".border.bottom > span",      value: 10},
      {selector: ".border.right > span",       value: 10},
];

let inspector;
let view;

let test = asyncTest(function*() {
  let style = "div { position: absolute; top: 42px; left: 42px; height: 100px; width: 100px; border: 10px solid black; padding: 20px; margin: 30px auto;}";
  let html = "<style>" + style + "</style><div></div>"

  let content = yield loadTab("data:text/html," + encodeURIComponent(html));
  let node = content.document.querySelector("div");
  ok(node, "node found");

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "inspector");
  inspector = toolbox.getCurrentPanel();

  info("Inspector open");

  inspector.sidebar.select("layoutview");
  yield inspector.sidebar.once("layoutview-ready");

  inspector.selection.setNode(node);
  yield inspector.once("inspector-updated");

  info("Layout view ready");

  view = inspector.sidebar.getWindowForTab("layoutview");
  ok(!!view.layoutview, "LayoutView document is alive.");

  yield runTests();

  executeSoon(function() {
    inspector._toolbox.destroy();
  });

  yield gDevTools.once("toolbox-destroyed");
});

addTest("Test that the initial values of the box model are correct",
function*() {
  let viewdoc = view.document;

  for (let i = 0; i < res1.length; i++) {
    let elt = viewdoc.querySelector(res1[i].selector);
    is(elt.textContent, res1[i].value, res1[i].selector + " has the right value.");
  }
});

addTest("Test that changing the document updates the box model",
function*() {
  let viewdoc = view.document;

  inspector.selection.node.style.height = "150px";
  inspector.selection.node.style.paddingRight = "50px";

  yield waitForUpdate();

  for (let i = 0; i < res2.length; i++) {
    let elt = viewdoc.querySelector(res2[i].selector);
    is(elt.textContent, res2[i].value, res2[i].selector + " has the right value after style update.");
  }
});
