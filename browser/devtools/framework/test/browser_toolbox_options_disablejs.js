/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that disabling JavaScript for a tab works as it should.

const TEST_URI = "http://example.com/browser/browser/devtools/framework/" +
                 "test/browser_toolbox_options_disablejs.html";

let doc;
let toolbox;

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    doc = content.document;
    gDevTools.showToolbox(target).then(testSelectTool);
  }, true);

  content.location = TEST_URI;
}

function testSelectTool(aToolbox) {
  toolbox = aToolbox;
  toolbox.once("options-selected", testJSEnabled);
  toolbox.selectTool("options");
}

function testJSEnabled(event, tool, secondPass) {
  ok(true, "Toolbox selected via selectTool method");
  info("Testing that JS is enabled");

  let logJSEnabled = doc.getElementById("logJSEnabled");
  let output = doc.getElementById("output");

  // We use executeSoon here because switching docSehll.allowJavascript to true
  // takes a while to become live.
  executeSoon(function() {
    EventUtils.synthesizeMouseAtCenter(logJSEnabled, {}, doc.defaultView);
    is(output.textContent, "JavaScript Enabled", 'Output is "JavaScript Enabled"');
    testJSEnabledIframe(secondPass);
  });
}

function testJSEnabledIframe(secondPass) {
  info("Testing that JS is enabled in the iframe");

  let iframe = doc.querySelector("iframe");
  let iframeDoc = iframe.contentDocument;
  let logJSEnabled = iframeDoc.getElementById("logJSEnabled");
  let output = iframeDoc.getElementById("output");

  EventUtils.synthesizeMouseAtCenter(logJSEnabled, {}, iframe.contentWindow);
  is(output.textContent, "JavaScript Enabled",
                         'Output is "JavaScript Enabled" in iframe');
  if (secondPass) {
    finishUp();
  } else {
    toggleJS().then(testJSDisabled);
  }
}

function toggleJS() {
  let deferred = promise.defer();
  let panel = toolbox.getCurrentPanel();
  let cbx = panel.panelDoc.getElementById("devtools-disable-javascript");

  cbx.scrollIntoView();

  if (cbx.checked) {
    info("Clearing checkbox to re-enable JS");
  } else {
    info("Checking checkbox to disable JS");
  }

  // After uising scrollIntoView() we need to use executeSoon() to wait for the
  // browser to scroll.
  executeSoon(function() {
    gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
      gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
      doc = content.document;

      deferred.resolve();
    }, true);

    EventUtils.synthesizeMouseAtCenter(cbx, {}, panel.panelWin);
  });

  return deferred.promise;
}

function testJSDisabled() {
  info("Testing that JS is disabled");

  let logJSDisabled = doc.getElementById("logJSDisabled");
  let output = doc.getElementById("output");

  EventUtils.synthesizeMouseAtCenter(logJSDisabled, {}, doc.defaultView);
  ok(output.textContent !== "JavaScript Disabled",
     'output is not "JavaScript Disabled"');

  testJSDisabledIframe();
}

function testJSDisabledIframe() {
  info("Testing that JS is disabled in the iframe");

  let iframe = doc.querySelector("iframe");
  let iframeDoc = iframe.contentDocument;
  let logJSDisabled = iframeDoc.getElementById("logJSDisabled");
  let output = iframeDoc.getElementById("output");

  EventUtils.synthesizeMouseAtCenter(logJSDisabled, {}, iframe.contentWindow);
  ok(output.textContent !== "JavaScript Disabled",
     'output is not "JavaScript Disabled" in iframe');
  toggleJS().then(function() {
    testJSEnabled(null, null, true);
  });
}

function finishUp() {
  doc = toolbox = null;
  finish();
}
