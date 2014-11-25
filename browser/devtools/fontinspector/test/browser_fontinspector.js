/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tempScope = {};
let {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let TargetFactory = devtools.TargetFactory;

let TEST_URI = "http://mochi.test:8888/browser/browser/devtools/fontinspector/test/browser_fontinspector.html";

let view, viewDoc;

let test = asyncTest(function*() {
  yield loadTab(TEST_URI);
  let {toolbox, inspector} = yield openInspector();

  info("Selecting the test node");
  yield selectNode("body", inspector);

  let updated = inspector.once("fontinspector-updated");
  inspector.sidebar.select("fontinspector");
  yield updated;

  info("Font Inspector ready");

  view = inspector.sidebar.getWindowForTab("fontinspector");
  viewDoc = view.document;

  ok(!!view.fontInspector, "Font inspector document is alive.");

  yield testBodyFonts(inspector);

  yield testDivFonts(inspector);

  yield testShowAllFonts(inspector);

  view = viewDoc = null;
});

function* testBodyFonts(inspector) {
  let s = viewDoc.querySelectorAll("#all-fonts > section");
  is(s.length, 2, "Found 2 fonts");

  // test first web font
  is(s[0].querySelector(".font-name").textContent,
     "Ostrich Sans Medium", "font 0: Right font name");
  ok(s[0].classList.contains("is-remote"),
     "font 0: is remote");
  is(s[0].querySelector(".font-url").value,
     "http://mochi.test:8888/browser/browser/devtools/fontinspector/test/ostrich-regular.woff",
     "font 0: right url");
  is(s[0].querySelector(".font-format").textContent,
     "woff", "font 0: right font format");
  is(s[0].querySelector(".font-css-name").textContent,
     "bar", "font 0: right css name");

  // test system font
  let font2Name = s[1].querySelector(".font-name").textContent;
  let font2CssName = s[1].querySelector(".font-css-name").textContent;

  // On Linux test machines, the Arial font doesn't exist.
  // The fallback is "Liberation Sans"
  ok((font2Name == "Arial") || (font2Name == "Liberation Sans"),
     "font 1: Right font name");
  ok(s[1].classList.contains("is-local"), "font 2: is local");
  ok((font2CssName == "Arial") || (font2CssName == "Liberation Sans"),
     "Arial", "font 2: right css name");
}

function* testDivFonts(inspector) {
  let updated = inspector.once("fontinspector-updated");
  yield selectNode("div", inspector);
  yield updated;

  let sections1 = viewDoc.querySelectorAll("#all-fonts > section");
  is(sections1.length, 1, "Found 1 font on DIV");
  is(sections1[0].querySelector(".font-name").textContent, "Ostrich Sans Medium",
    "The DIV font has the right name");
}

function* testShowAllFonts(inspector) {
  info("testing showing all fonts");

  let updated = inspector.once("fontinspector-updated");
  viewDoc.querySelector("#showall").click();
  yield updated;

  is(inspector.selection.nodeFront.nodeName, "BODY", "Show all fonts selected the body node");
  let sections = viewDoc.querySelectorAll("#all-fonts > section");
  is(sections.length, 2, "And font-inspector still shows 2 fonts for body");
}
