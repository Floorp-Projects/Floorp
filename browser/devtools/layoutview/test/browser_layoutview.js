/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let TargetFactory = devtools.TargetFactory;

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("devtools.layoutview.enabled", true);
  Services.prefs.setBoolPref("devtools.inspector.sidebarOpen", true);

  let doc;
  let node;
  let view;
  let inspector;

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

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  let style = "div { position: absolute; top: 42px; left: 42px; height: 100px; width: 100px; border: 10px solid black; padding: 20px; margin: 30px auto;}";
  let html = "<style>" + style + "</style><div></div>"
  content.location = "data:text/html," + encodeURIComponent(html);

  function setupTest() {
    node = doc.querySelector("div");
    ok(node, "node found");

    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
      openLayoutView(toolbox.getCurrentPanel());
    });
  }

  function openLayoutView(aInspector) {
    inspector = aInspector;

    info("Inspector open");

    inspector.sidebar.select("layoutview");
    inspector.sidebar.once("layoutview-ready", () => {
      inspector.selection.setNode(node);
      inspector.once("inspector-updated", viewReady);
    });
  }


  function viewReady() {
    info("Layout view ready");

    view = inspector.sidebar.getWindowForTab("layoutview");

    ok(!!view.layoutview, "LayoutView document is alive.");

    test1();
  }

  function test1() {
    let viewdoc = view.document;

    for (let i = 0; i < res1.length; i++) {
      let elt = viewdoc.querySelector(res1[i].selector);
      is(elt.textContent, res1[i].value, res1[i].selector + " has the right value.");
    }

    inspector.once("layoutview-updated", test2);

    inspector.selection.node.style.height = "150px";
    inspector.selection.node.style.paddingRight = "50px";
  }

  function test2() {
    let viewdoc = view.document;

    for (let i = 0; i < res2.length; i++) {
      let elt = viewdoc.querySelector(res2[i].selector);
      is(elt.textContent, res2[i].value, res2[i].selector + " has the right value after style update.");
    }

    executeSoon(function() {
      gDevTools.once("toolbox-destroyed", finishUp);
      inspector._toolbox.destroy();
    });
  }

  function finishUp() {
    Services.prefs.clearUserPref("devtools.layoutview.enabled");
    Services.prefs.clearUserPref("devtools.inspector.sidebarOpen");
    gBrowser.removeCurrentTab();
    finish();
  }
}
