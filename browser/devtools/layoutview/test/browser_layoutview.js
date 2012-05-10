/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("devtools.layoutview.enabled", true);
  Services.prefs.setBoolPref("devtools.inspector.sidebarOpen", true);

  let doc;
  let node;
  let view;

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

    Services.obs.addObserver(openLayoutView,
      InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
    InspectorUI.toggleInspectorUI();
  }

  function openLayoutView() {
    Services.obs.removeObserver(openLayoutView,
      InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);

    info("Inspector open");

    let highlighter = InspectorUI.highlighter;
    highlighter.highlight(node);
    highlighter.lock();

    window.addEventListener("message", viewReady, true);
  }

  function viewReady(e) {
    if (e.data != "layoutview-ready") return;

    window.removeEventListener("message", viewReady, true);

    info("Layout view ready");

    view = InspectorUI._sidebar._layoutview;

    ok(!!view, "LayoutView document is alive.");

    view.open();

    ok(view.iframe.getAttribute("open"), "true", "View is open.");

    test1();
  }

  function test1() {
    let viewdoc = view.iframe.contentDocument;

    for (let i = 0; i < res1.length; i++) {
      let elt = viewdoc.querySelector(res1[i].selector);
      is(elt.textContent, res1[i].value, res1[i].selector + " has the right value.");
    }

    InspectorUI.selection.style.height = "150px";
    InspectorUI.selection.style.paddingRight = "50px";

    setTimeout(test2, 200); // Should be enough to get a MozAfterPaint event
  }

  function test2() {
    let viewdoc = view.iframe.contentDocument;

    for (let i = 0; i < res2.length; i++) {
      let elt = viewdoc.querySelector(res2[i].selector);
      is(elt.textContent, res2[i].value, res2[i].selector + " has the right value after style update.");
    }

    executeSoon(function() {
      Services.obs.addObserver(finishUp,
        InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);
      InspectorUI.closeInspectorUI();
    });
  }

  function finishUp() {
    Services.prefs.clearUserPref("devtools.layoutview.enabled");
    Services.prefs.clearUserPref("devtools.inspector.sidebarOpen");
    Services.obs.removeObserver(finishUp, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);
    gBrowser.removeCurrentTab();
    finish();
  }
}
