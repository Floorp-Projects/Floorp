/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that disabling JavaScript for a tab works as it should.

const TEST_URI = URL_ROOT + "browser_toolbox_options_disable_js.html";

var doc;
var toolbox;

function test() {
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

  // We use executeSoon here because switching docSehll.allowJavascript to true
  // takes a while to become live.
  executeSoon(function() {
    let output = doc.getElementById("output");
    doc.querySelector("#logJSEnabled").click();
    is(output.textContent, "JavaScript Enabled", 'Output is "JavaScript Enabled"');
    testJSEnabledIframe(secondPass);
  });
}

function testJSEnabledIframe(secondPass) {
  info("Testing that JS is enabled in the iframe");

  let iframe = doc.querySelector("iframe");
  let iframeDoc = iframe.contentDocument;
  let output = iframeDoc.getElementById("output");
  iframeDoc.querySelector("#logJSEnabled").click();
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

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    doc = content.document;
    deferred.resolve();
  }, true);

  cbx.click();

  return deferred.promise;
}

function testJSDisabled() {
  info("Testing that JS is disabled");

  let output = doc.getElementById("output");
  doc.querySelector("#logJSDisabled").click();

  ok(output.textContent !== "JavaScript Disabled",
     'output is not "JavaScript Disabled"');
  testJSDisabledIframe();
}

function testJSDisabledIframe() {
  info("Testing that JS is disabled in the iframe");

  let iframe = doc.querySelector("iframe");
  let iframeDoc = iframe.contentDocument;
  let output = iframeDoc.getElementById("output");
  iframeDoc.querySelector("#logJSDisabled").click();
  ok(output.textContent !== "JavaScript Disabled",
     'output is not "JavaScript Disabled" in iframe');
  toggleJS().then(function() {
    testJSEnabled(null, null, true);
  });
}

function finishUp() {
  toolbox.destroy().then(function() {
    gBrowser.removeCurrentTab();
    toolbox = doc = null;
    finish();
  });
}
