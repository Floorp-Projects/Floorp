/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tempScope = {};
let {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let TargetFactory = devtools.TargetFactory;

let DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);

function test() {
  waitForExplicitFinish();

  let doc;
  let view;
  let viewDoc;
  let inspector;

  gDevTools.testing = true;
  SimpleTest.registerCleanupFunction(() => {
    gDevTools.testing = false;
  });

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  content.location = "http://mochi.test:8888/browser/browser/devtools/fontinspector/test/browser_fontinspector.html";

  function setupTest() {
    let rng = doc.createRange();
    rng.selectNode(doc.body);
    let fonts = DOMUtils.getUsedFontFaces(rng);
    if (fonts.length != 2) {
      // Fonts are not loaded yet.
      // Let try again in a couple of milliseconds (hacky, but
      // there's not better way to do it. See bug 835247).
      setTimeout(setupTest, 500);
    } else {
      let target = TargetFactory.forTab(gBrowser.selectedTab);
      gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
        openFontInspector(toolbox.getCurrentPanel());
      });
    }
  }

  function openFontInspector(aInspector) {
    info("Inspector open");
    inspector = aInspector;

    inspector.selection.setNode(doc.body);
    inspector.sidebar.select("fontinspector");
    inspector.sidebar.once("fontinspector-ready", testBodyFonts);
  }

  function testBodyFonts() {
    info("Font Inspector ready");

    view = inspector.sidebar.getWindowForTab("fontinspector");
    viewDoc = view.document;

    ok(!!view.fontInspector, "Font inspector document is alive.");

    let s = viewDoc.querySelectorAll("#all-fonts > section");
    is(s.length, 2, "Found 2 fonts");

    is(s[0].querySelector(".font-name").textContent,
       "DeLarge Bold", "font 0: Right font name");
    ok(s[0].classList.contains("is-remote"),
       "font 0: is remote");
    is(s[0].querySelector(".font-url").value,
       "http://mochi.test:8888/browser/browser/devtools/fontinspector/test/browser_font.woff",
       "font 0: right url");
    is(s[0].querySelector(".font-format").textContent,
       "woff", "font 0: right font format");
    is(s[0].querySelector(".font-css-name").textContent,
       "bar", "font 0: right css name");

    let font1Name = s[1].querySelector(".font-name").textContent;
    let font1CssName = s[1].querySelector(".font-css-name").textContent;

    // On Linux test machines, the Arial font doesn't exist.
    // The fallback is "Liberation Sans"

    ok((font1Name == "Arial") || (font1Name == "Liberation Sans"),
       "font 1: Right font name");
    ok(s[1].classList.contains("is-local"), "font 1: is local");
    ok((font1CssName == "Arial") || (font1CssName == "Liberation Sans"),
       "Arial", "font 1: right css name");

    testDivFonts();
  }

  function testDivFonts() {
    inspector.selection.setNode(doc.querySelector("div"));
    inspector.once("inspector-updated", () => {
      let s = viewDoc.querySelectorAll("#all-fonts > section");
      is(s.length, 1, "Found 1 font on DIV");
      is(s[0].querySelector(".font-name").textContent, "DeLarge Bold",
        "The DIV font has the right name");

      testShowAllFonts();
    });
  }

  function testShowAllFonts() {
    viewDoc.querySelector("#showall").click();
    inspector.once("inspector-updated", () => {
      is(inspector.selection.node, doc.body, "Show all fonts selected the body node");
      let s = viewDoc.querySelectorAll("#all-fonts > section");
      is(s.length, 2, "And font-inspector still shows 2 fonts for body");

      finishUp();
    });
  }

  function finishUp() {
    executeSoon(function() {
      gDevTools.once("toolbox-destroyed", () => {
        doc = view = viewDoc = inspector = null;
        gBrowser.removeCurrentTab();
        finish();
      });
      inspector._toolbox.destroy();
    });
  }
}
