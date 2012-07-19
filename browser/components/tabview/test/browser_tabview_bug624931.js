/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that there is a transform being applied to the tabs as they zoom in
// and out.

let tab, frontChanged, transformChanged;

function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  let contentWindow = document.getElementById("tab-view").contentWindow;
  tab = contentWindow.UI.getActiveTab();
  ok(tab, "We have an active tab");

  frontChanged = transformChanged = false;
  tab.$container[0].addEventListener("DOMAttrModified", checkForFrontAddition,
                                     false);
  tab.$canvas[0].addEventListener("DOMAttrModified", checkForTransformAddition,
                                  false);

  window.addEventListener("tabviewhidden", onTabViewHidden, false);
  TabView.toggle();
}

function checkForFrontAddition(aEvent) {
  if (aEvent.attrName == "class" &&
      aEvent.target.classList.contains("front")) {
    frontChanged = true;
  }
}

function checkForTransformAddition(aEvent) {
  if (aEvent.attrName == "style" && aEvent.target.style.transform) {
    transformChanged = true;
  }
}

function onTabViewHidden() {
  window.removeEventListener("tabviewhidden", onTabViewHidden, false);

  ok(frontChanged, "the CSS class 'front' was added while zooming in");
  ok(transformChanged, "the CSS class 'transform' was modified while " +
     "zooming in");

  frontChanged = transformChanged = false;
  tab.$container[0].removeEventListener("DOMAttrModified",
                                        checkForFrontAddition, false);
  tab.$container[0].addEventListener("DOMAttrModified", checkForFrontRemoval,
                                     false);

  window.addEventListener("tabviewshown", onTabViewShownAgain, false);
  TabView.toggle();
}

function checkForFrontRemoval(aEvent) {
  if (aEvent.attrName == "class" &&
      !aEvent.target.classList.contains("front")) {
    frontChanged = true;
  }
}

function onTabViewShownAgain() {
  window.removeEventListener("tabviewshown", onTabViewShownAgain, false);

  ok(frontChanged, "the CSS class 'front' was removed while zooming out");
  ok(transformChanged, "the CSS class 'transform' was removed while zooming " +
     "out");

  tab.$container[0].removeEventListener("DOMAttrModified",
                                        checkForFrontRemoval, false);
  tab.$canvas[0].removeEventListener("DOMAttrModified",
                                     checkForTransformAddition, false);

  window.addEventListener("tabviewhidden", onTabViewHiddenAgain, false);
  TabView.toggle();
}

function onTabViewHiddenAgain() {
  window.removeEventListener("tabviewhidden", onTabViewHiddenAgain, false);

  finish();
}

