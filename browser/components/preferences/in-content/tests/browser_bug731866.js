/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

function test() {
  waitForExplicitFinish();
  let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab = gBrowser.addTab("about:preferences"));
  newTabBrowser.addEventListener("load", function () {
    newTabBrowser.removeEventListener("load", arguments.callee, true);
    runTest(gBrowser.contentWindow);
  }, true);
}

function runTest(win) {
  is(gBrowser.currentURI.spec, "about:preferences", "about:preferences loaded");

  let tab = win.document;
  let elements = tab.getElementById("mainPrefPane").children;

  //Test if general pane is opened correctly
  win.gotoPref("paneGeneral");
  for (let element of elements) {
    let attributeValue = element.getAttribute("data-category");
    if (attributeValue == "paneGeneral") {
      is_element_visible(element, "General elements should be visible");
    } else {
      is_element_hidden(element, "Non-General elements should be hidden");
    }
  }

  //Test if tabs pane is opened correctly
  win.gotoPref("paneTabs");
  for (let element of elements) {
    let attributeValue = element.getAttribute("data-category");
    if (attributeValue == "paneTabs") {
      is_element_visible(element, "Tab elements should be visible");
    } else {
      is_element_hidden(element, "Non-Tab elements should be hidden");
    }
  }

  //Test if content pane is opened correctly
  win.gotoPref("paneContent");
  for (let element of elements) {
    let attributeValue = element.getAttribute("data-category");
    if (attributeValue == "paneContent") {
      is_element_visible(element, "Content elements should be visible");
    } else {
      is_element_hidden(element, "Non-Content elements should be hidden");
    }
  }

  //Test if applications pane is opened correctly
  win.gotoPref("paneApplications");
  for (let element of elements) {
    let attributeValue = element.getAttribute("data-category");
    if (attributeValue == "paneApplications") {
      is_element_visible(element, "Application elements should be visible");
    } else {
      is_element_hidden(element, "Non-Application elements should be hidden");
    }
  }

  //Test if privacy pane is opened correctly
  win.gotoPref("panePrivacy");
  for (let element of elements) {
    let attributeValue = element.getAttribute("data-category");
    if (attributeValue == "panePrivacy") {
      is_element_visible(element, "Privacy elements should be visible");
    } else {
      is_element_hidden(element, "Non-Privacy elements should be hidden");
    }
  }

  //Test if security pane is opened correctly
  win.gotoPref("paneSecurity");
  for (let element of elements) {
    let attributeValue = element.getAttribute("data-category");
    if (attributeValue == "paneSecurity") {
      is_element_visible(element, "Security elements should be visible");
    } else {
      is_element_hidden(element, "Non-Security elements should be hidden");
    }
  }

  //Test if sync pane is opened correctly
  win.gotoPref("paneSync");
  for (let element of elements) {
    let attributeValue = element.getAttribute("data-category");
    if (attributeValue == "paneSync") {
      is_element_visible(element, "Sync elements should be visible");
    } else {
      is_element_hidden(element, "Non-Sync elements should be hidden");
    }
  }

  //Test if advanced pane is opened correctly
  win.gotoPref("paneAdvanced");
  for (let element of elements) {
    let attributeValue = element.getAttribute("data-category");
    if (attributeValue == "paneAdvanced") {
      is_element_visible(element, "Advanced elements should be visible");
    } else {
      is_element_hidden(element, "Non-Advanced elements should be hidden");
    }
  }

  gBrowser.removeCurrentTab();
  win.close();
  finish();
}
