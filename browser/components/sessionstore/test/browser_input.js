/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/browser/" +
            "browser/components/sessionstore/test/browser_input_sample.html";

function test() {
  TestRunner.run();
}

/**
 * This test ensures that modifying form input fields on a web page marks the
 * window as dirty and causes the corresponding form data of the tab that
 * changed to be re-collected.
 */

function runTests() {
  // Create a dummy window that is regarded as active. We need to do this
  // because we always collect data for tabs of active windows no matter if
  // the window is dirty or not.
  let win = OpenBrowserWindow();
  yield waitForLoad(win);

  // Create a tab with some form fields.
  let tab = gBrowser.selectedTab = gBrowser.addTab(URL);
  let browser = gBrowser.selectedBrowser;
  yield waitForLoad(browser);

  // All windows currently marked as dirty will be written to disk
  // and thus marked clean afterwards.
  yield forceWriteState();

  // Modify the checkbox field's state.
  let chk = browser.contentDocument.getElementById("chk");
  EventUtils.sendMouseEvent({type: "click"}, chk);
  yield waitForInput();

  // Check that we'll save the form data state correctly.
  let state = JSON.parse(ss.getBrowserState());
  let {formdata} = state.windows[0].tabs[1].entries[0];
  is(formdata.id.chk, true, "chk's value is correct");

  // Clear dirty state of all windows again.
  yield forceWriteState();

  // Modify the text input field's state.
  browser.contentDocument.getElementById("txt").focus();
  EventUtils.synthesizeKey("m", {});
  yield waitForInput();

  // Check that we'll save the form data state correctly.
  let state = JSON.parse(ss.getBrowserState());
  let {formdata} = state.windows[0].tabs[1].entries[0];
  is(formdata.id.chk, true, "chk's value is correct");
  is(formdata.id.txt, "m", "txt's value is correct");

  // Clear dirty state of all windows again.
  yield forceWriteState();

  // Modify the state of the checkbox contained in the iframe.
  let cdoc = browser.contentDocument.getElementById("ifr").contentDocument;
  EventUtils.sendMouseEvent({type: "click"}, cdoc.getElementById("chk"));
  yield waitForInput();

  // Modify the state of text field contained in the iframe.
  cdoc.getElementById("txt").focus();
  EventUtils.synthesizeKey("m", {});
  yield waitForInput();

  // Check that we'll save the iframe's form data correctly.
  let state = JSON.parse(ss.getBrowserState());
  let {formdata} = state.windows[0].tabs[1].entries[0].children[0];
  is(formdata.id.chk, true, "iframe chk's value is correct");
  is(formdata.id.txt, "m", "iframe txt's value is correct");

  // Clear dirty state of all windows again.
  yield forceWriteState();

  // Modify the content editable's state.
  browser.contentDocument.getElementById("ced").focus();
  EventUtils.synthesizeKey("m", {});
  yield waitForInput();

  // Check the we'll correctly save the content editable's state.
  let state = JSON.parse(ss.getBrowserState());
  let {innerHTML} = state.windows[0].tabs[1].entries[0].children[1];
  is(innerHTML, "m", "content editable's value is correct");

  // Clean up after ourselves.
  gBrowser.removeTab(tab);
  win.close();
}

function forceWriteState() {
  const PREF = "browser.sessionstore.interval";
  const TOPIC = "sessionstore-state-write";

  Services.obs.addObserver(function observe() {
    Services.obs.removeObserver(observe, TOPIC);
    Services.prefs.clearUserPref(PREF);
    executeSoon(next);
  }, TOPIC, false);

  Services.prefs.setIntPref(PREF, 0);
}

function waitForLoad(aElement) {
  aElement.addEventListener("load", function onLoad() {
    aElement.removeEventListener("load", onLoad, true);
    executeSoon(next);
  }, true);
}

function waitForInput() {
  let mm = gBrowser.selectedBrowser.messageManager;

  mm.addMessageListener("SessionStore:input", function onPageShow() {
    mm.removeMessageListener("SessionStore:input", onPageShow);
    executeSoon(next);
  });
}
