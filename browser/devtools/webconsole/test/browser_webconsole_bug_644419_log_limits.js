/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the Web Console limits the number of lines displayed according to
// the limit set for each category.

const TEST_URI = "http://example.com/browser/browser/devtools/" +
                 "webconsole/test/test-bug-644419-log-limits.html";

var gOldPref;

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 644419: Console should " +
         "have user-settable log limits for each message category");
  browser.addEventListener("load", onLoad, true);
}

function onLoad(aEvent) {
  browser.removeEventListener(aEvent.type, onLoad, true);

  openConsole(null, function(aHud) {
    aHud.jsterm.clearOutput();
    hud = aHud;
    outputNode = aHud.outputNode;

    browser.addEventListener("load", testWebDevLimits, true);
    expectUncaughtException();
    content.location = TEST_URI;
  });
}

function testWebDevLimits(aEvent) {
  browser.removeEventListener(aEvent.type, testWebDevLimits, true);
  gOldPref = Services.prefs.getIntPref("devtools.hud.loglimit.console");
  Services.prefs.setIntPref("devtools.hud.loglimit.console", 10);

  // Find the sentinel entry.
  waitForSuccess({
    name: "bar is not defined",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("bar is not defined") > -1;
    },
    successFn: testWebDevLimits2,
    failureFn: testWebDevLimits2,
  });
}

function testWebDevLimits2() {
  // Fill the log with Web Developer errors.
  for (let i = 0; i < 11; i++) {
    content.console.log("test message " + i);
  }

  waitForSuccess({
    name: "10 console.log messages displayed and one pruned",
    validatorFn: function()
    {
      let message0 = outputNode.textContent.indexOf("test message 0");
      let message10 = outputNode.textContent.indexOf("test message 10");
      return message0 == -1 && message10 > -1;
    },
    successFn: function()
    {
      findLogEntry("test message 1");
      // Check if the sentinel entry is still there.
      findLogEntry("bar is not defined");

      Services.prefs.setIntPref("devtools.hud.loglimit.console", gOldPref);
      testJsLimits();
    },
    failureFn: testJsLimits,
  });
}

function testJsLimits() {
  gOldPref = Services.prefs.getIntPref("devtools.hud.loglimit.exception");
  Services.prefs.setIntPref("devtools.hud.loglimit.exception", 10);

  hud.jsterm.clearOutput();
  content.console.log("testing JS limits");

  // Find the sentinel entry.
  waitForSuccess({
    name: "console.log 'testing JS limits'",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("testing JS limits") > -1;
    },
    successFn: testJsLimits2,
    failureFn: testNetLimits,
  });
}

function testJsLimits2() {
  // Fill the log with JS errors.
  let head = content.document.getElementsByTagName("head")[0];
  for (let i = 0; i < 11; i++) {
    var script = content.document.createElement("script");
    script.text = "fubar" + i + ".bogus(6);";
    expectUncaughtException();
    head.insertBefore(script, head.firstChild);
  }

  waitForSuccess({
    name: "10 JS errors shown",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("fubar10 is not defined") > -1;
    },
    successFn: function()
    {
      testLogEntry(outputNode, "fubar0 is not defined", "first message is pruned", false, true);
      findLogEntry("fubar1 is not defined");
      // Check if the sentinel entry is still there.
      findLogEntry("testing JS limits");

      Services.prefs.setIntPref("devtools.hud.loglimit.exception", gOldPref);
      testNetLimits();
    },
    failureFn: testNetLimits,
  });
}

var gCounter, gImage;

function testNetLimits() {
  gOldPref = Services.prefs.getIntPref("devtools.hud.loglimit.network");
  Services.prefs.setIntPref("devtools.hud.loglimit.network", 10);

  hud.jsterm.clearOutput();
  content.console.log("testing Net limits");

  // Find the sentinel entry.
  waitForSuccess({
    name: "console.log 'testing Net limits'",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("testing Net limits") > -1;
    },
    successFn: function()
    {
      // Fill the log with network messages.
      gCounter = 0;
      loadImage();
    },
    failureFn: testCssLimits,
  });
}

function loadImage() {
  if (gCounter < 11) {
    let body = content.document.getElementsByTagName("body")[0];
    gImage && gImage.removeEventListener("load", loadImage, true);
    gImage = content.document.createElement("img");
    gImage.src = "test-image.png?_fubar=" + gCounter;
    body.insertBefore(gImage, body.firstChild);
    gImage.addEventListener("load", loadImage, true);
    gCounter++;
    return;
  }

  is(gCounter, 11, "loaded 11 files");

  waitForSuccess({
    name: "loaded 11 files, one message pruned",
    validatorFn: function()
    {
      let message0 = outputNode.querySelector('*[value*="test-image.png?_fubar=0"]');
      let message10 = outputNode.querySelector('*[value*="test-image.png?_fubar=10"]');
      return !message0 && message10;
    },
    successFn: function()
    {
      findLogEntry("test-image.png?_fubar=1");
      // Check if the sentinel entry is still there.
      findLogEntry("testing Net limits");

      Services.prefs.setIntPref("devtools.hud.loglimit.network", gOldPref);
      testCssLimits();
    },
    failureFn: testCssLimits,
  });
}

function testCssLimits() {
  gOldPref = Services.prefs.getIntPref("devtools.hud.loglimit.cssparser");
  Services.prefs.setIntPref("devtools.hud.loglimit.cssparser", 10);

  hud.jsterm.clearOutput();
  content.console.log("testing CSS limits");

  // Find the sentinel entry.
  waitForSuccess({
    name: "console.log 'testing CSS limits'",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("testing CSS limits") > -1;
    },
    successFn: testCssLimits2,
    failureFn: finishTest,
  });
}

function testCssLimits2() {
  // Fill the log with CSS errors.
  let body = content.document.getElementsByTagName("body")[0];
  for (let i = 0; i < 11; i++) {
    var div = content.document.createElement("div");
    div.setAttribute("style", "-moz-foobar" + i + ": 42;");
    body.insertBefore(div, body.firstChild);
  }

  waitForSuccess({
    name: "10 CSS errors shown",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("-moz-foobar10") > -1;
    },
    successFn: function()
    {
      testLogEntry(outputNode, "Unknown property '-moz-foobar0'",
                   "first message is pruned", false, true);
      findLogEntry("Unknown property '-moz-foobar1'");
      // Check if the sentinel entry is still there.
      findLogEntry("testing CSS limits");

      Services.prefs.setIntPref("devtools.hud.loglimit.cssparser", gOldPref);
      finishTest();
    },
    failureFn: finishTest,
  });
}
