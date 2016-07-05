/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that disabling JavaScript for a tab works as it should.

const TEST_URI = URL_ROOT + "browser_toolbox_options_disable_js.html";

function test() {
  gBrowser.selectedTab = gBrowser.addTab();
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    gDevTools.showToolbox(target).then(testSelectTool);
  }, true);

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI);
}

function testSelectTool(toolbox) {
  toolbox.once("options-selected", () => testToggleJS(toolbox));
  toolbox.selectTool("options");
}

let testToggleJS = Task.async(function* (toolbox) {
  ok(true, "Toolbox selected via selectTool method");

  yield testJSEnabled();
  yield testJSEnabledIframe();

  // Disable JS.
  yield toggleJS(toolbox);

  yield testJSDisabled();
  yield testJSDisabledIframe();

  // Re-enable JS.
  yield toggleJS(toolbox);

  yield testJSEnabled();
  yield testJSEnabledIframe();

  finishUp(toolbox);
});

function* testJSEnabled() {
  info("Testing that JS is enabled");

  // We use waitForTick here because switching docShell.allowJavascript to true
  // takes a while to become live.
  yield waitForTick();

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    let doc = content.document;
    let output = doc.getElementById("output");
    doc.querySelector("#logJSEnabled").click();
    is(output.textContent, "JavaScript Enabled", 'Output is "JavaScript Enabled"');
  });
}

function* testJSEnabledIframe() {
  info("Testing that JS is enabled in the iframe");

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    let doc = content.document;
    let iframe = doc.querySelector("iframe");
    let iframeDoc = iframe.contentDocument;
    let output = iframeDoc.getElementById("output");
    iframeDoc.querySelector("#logJSEnabled").click();
    is(output.textContent, "JavaScript Enabled",
                            'Output is "JavaScript Enabled" in iframe');
  });
}

function* toggleJS(toolbox) {
  let panel = toolbox.getCurrentPanel();
  let cbx = panel.panelDoc.getElementById("devtools-disable-javascript");

  if (cbx.checked) {
    info("Clearing checkbox to re-enable JS");
  } else {
    info("Checking checkbox to disable JS");
  }

  let browserLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  cbx.click();
  yield browserLoaded;
}

function* testJSDisabled() {
  info("Testing that JS is disabled");

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    let doc = content.document;
    let output = doc.getElementById("output");
    doc.querySelector("#logJSDisabled").click();

    ok(output.textContent !== "JavaScript Disabled",
       'output is not "JavaScript Disabled"');
  });
}

function* testJSDisabledIframe() {
  info("Testing that JS is disabled in the iframe");

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    let doc = content.document;
    let iframe = doc.querySelector("iframe");
    let iframeDoc = iframe.contentDocument;
    let output = iframeDoc.getElementById("output");
    iframeDoc.querySelector("#logJSDisabled").click();
    ok(output.textContent !== "JavaScript Disabled",
       'output is not "JavaScript Disabled" in iframe');
  });
}

function finishUp(toolbox) {
  toolbox.destroy().then(function () {
    gBrowser.removeCurrentTab();
    finish();
  });
}
