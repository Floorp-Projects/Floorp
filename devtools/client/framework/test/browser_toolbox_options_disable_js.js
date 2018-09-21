/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that disabling JavaScript for a tab works as it should.

const TEST_URI = URL_ROOT + "browser_toolbox_options_disable_js.html";

function test() {
  addTab(TEST_URI).then(tab => {
    const target = TargetFactory.forTab(tab);
    gDevTools.showToolbox(target).then(testSelectTool);
  });
}

function testSelectTool(toolbox) {
  toolbox.once("options-selected", () => testToggleJS(toolbox));
  toolbox.selectTool("options");
}

const testToggleJS = async function(toolbox) {
  ok(true, "Toolbox selected via selectTool method");

  await testJSEnabled();
  await testJSEnabledIframe();

  // Disable JS.
  await toggleJS(toolbox);

  await testJSDisabled();
  await testJSDisabledIframe();

  // Re-enable JS.
  await toggleJS(toolbox);

  await testJSEnabled();
  await testJSEnabledIframe();

  finishUp(toolbox);
};

async function testJSEnabled() {
  info("Testing that JS is enabled");

  // We use waitForTick here because switching docShell.allowJavascript to true
  // takes a while to become live.
  await waitForTick();

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    const doc = content.document;
    const output = doc.getElementById("output");
    doc.querySelector("#logJSEnabled").click();
    is(output.textContent, "JavaScript Enabled", 'Output is "JavaScript Enabled"');
  });
}

async function testJSEnabledIframe() {
  info("Testing that JS is enabled in the iframe");

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    const doc = content.document;
    const iframe = doc.querySelector("iframe");
    const iframeDoc = iframe.contentDocument;
    const output = iframeDoc.getElementById("output");
    iframeDoc.querySelector("#logJSEnabled").click();
    is(output.textContent, "JavaScript Enabled",
                            'Output is "JavaScript Enabled" in iframe');
  });
}

async function toggleJS(toolbox) {
  const panel = toolbox.getCurrentPanel();
  const cbx = panel.panelDoc.getElementById("devtools-disable-javascript");

  if (cbx.checked) {
    info("Clearing checkbox to re-enable JS");
  } else {
    info("Checking checkbox to disable JS");
  }

  const browserLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  cbx.click();
  await browserLoaded;
}

async function testJSDisabled() {
  info("Testing that JS is disabled");

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    const doc = content.document;
    const output = doc.getElementById("output");
    doc.querySelector("#logJSDisabled").click();

    ok(output.textContent !== "JavaScript Disabled",
       'output is not "JavaScript Disabled"');
  });
}

async function testJSDisabledIframe() {
  info("Testing that JS is disabled in the iframe");

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    const doc = content.document;
    const iframe = doc.querySelector("iframe");
    const iframeDoc = iframe.contentDocument;
    const output = iframeDoc.getElementById("output");
    iframeDoc.querySelector("#logJSDisabled").click();
    ok(output.textContent !== "JavaScript Disabled",
       'output is not "JavaScript Disabled" in iframe');
  });
}

function finishUp(toolbox) {
  toolbox.destroy().then(function() {
    gBrowser.removeCurrentTab();
    finish();
  });
}
