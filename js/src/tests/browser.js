/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: If you're adding new test harness functionality to this file -- first,
//       should you at all?  Most stuff is better in specific tests, or in
//       nested shell.js/browser.js.  Second, can you instead add it to
//       shell.js?  Our goal is to unify these two files for readability, and
//       the plan is to empty out this file into that one over time.  Third,
//       supposing you must add to this file, please add it to this IIFE for
//       better modularity/resilience against tests that must do particularly
//       bizarre things that might break the harness.

(function(global) {
  /**********************************************************************
   * CACHED PRIMORDIAL FUNCTIONALITY (before a test might overwrite it) *
   **********************************************************************/

  var ReflectApply = global.Reflect.apply;

  // BEWARE: ObjectGetOwnPropertyDescriptor is only safe to use if its result
  //         is inspected using own-property-examining functionality.  Directly
  //         accessing properties on a returned descriptor without first
  //         verifying the property's existence can invoke user-modifiable
  //         behavior.
  var ObjectGetOwnPropertyDescriptor = global.Object.getOwnPropertyDescriptor;

  var document = global.document;
  var documentBody = global.document.body;
  var documentDocumentElement = global.document.documentElement;
  var DocumentCreateElement = global.document.createElement;
  var ElementInnerHTMLSetter =
    ObjectGetOwnPropertyDescriptor(global.Element.prototype, "innerHTML").set;
  var HTMLIFramePrototypeContentWindowGetter =
    ObjectGetOwnPropertyDescriptor(global.HTMLIFrameElement.prototype, "contentWindow").get;
  var HTMLIFramePrototypeRemove = global.HTMLIFrameElement.prototype.remove;
  var NodePrototypeAppendChild = global.Node.prototype.appendChild;
  var NodePrototypeTextContentSetter =
    ObjectGetOwnPropertyDescriptor(global.Node.prototype, "textContent").set;

  // Cached DOM nodes used by the test harness itself.  (We assume the test
  // doesn't misbehave in a way that actively interferes with what the test
  // harness runner observes, e.g. navigating the page to a different location.
  // Short of running every test in a worker -- which has its own problems --
  // there's no way to isolate a test from the page to that extent.)
  var printOutputContainer =
    global.document.getElementById("jsreftest-print-output-container");

  /****************************
   * GENERAL HELPER FUNCTIONS *
   ****************************/

  function AppendChild(elt, kid) {
    ReflectApply(NodePrototypeAppendChild, elt, [kid]);
  }

  function CreateElement(name) {
    return ReflectApply(DocumentCreateElement, document, [name]);
  }

  function HTMLSetAttribute(element, name, value) {
    ReflectApply(HTMLElementPrototypeSetAttribute, element, [name, value]);
  }

  function SetTextContent(element, text) {
    ReflectApply(NodePrototypeTextContentSetter, element, [text]);
  }

  /****************************
   * UTILITY FUNCTION EXPORTS *
   ****************************/

  var newGlobal = global.newGlobal;
  if (typeof newGlobal !== "function") {
    newGlobal = function newGlobal() {
      var iframe = CreateElement("iframe");
      AppendChild(documentDocumentElement, iframe);
      var win =
        ReflectApply(HTMLIFramePrototypeContentWindowGetter, iframe, []);
      ReflectApply(HTMLIFramePrototypeRemove, iframe, []);

      // Shim in "evaluate"
      win.evaluate = win.eval;
      return win;
    };
    global.newGlobal = newGlobal;
  }

  // This function is *only* used by shell.js's for-browsers |print()| function!
  // It's only defined/exported here because it needs CreateElement and friends,
  // only defined here, and we're not yet ready to move them to shell.js.
  function AddPrintOutput(s) {
    var msgDiv = CreateElement("div");
    SetTextContent(msgDiv, s);
    AppendChild(printOutputContainer, msgDiv);
  }
  global.AddPrintOutput = AddPrintOutput;

  /*************************************************************************
   * HARNESS-CENTRIC EXPORTS (we should generally work to eliminate these) *
   *************************************************************************/

  // This overwrites shell.js's version that merely prints the given string.
  function writeHeaderToLog(string) {
    string = String(string);

    // First dump to the console.
    dump(string + "\n");

    // Then output to the page.
    var h2 = CreateElement("h2");
    SetTextContent(h2, string);
    AppendChild(printOutputContainer, h2);
  }
  global.writeHeaderToLog = writeHeaderToLog;

  // XXX This function overwrites one in shell.js.  We should define the
  //     separate versions in a single location.  Also the dependence on
  //     |global.{PASSED,FAILED}| is very silly.
  function writeFormattedResult(expect, actual, string, passed) {
    // XXX remove this?  it's unneeded in the shell version
    string = String(string);

    dump(string + "\n");

    var font = CreateElement("font");
    if (passed) {
      HTMLSetAttribute(font, "color", "#009900");
      SetTextContent(font, " \u00A0" + global.PASSED);
    } else {
      HTMLSetAttribute(font, "color", "#aa0000");
      SetTextContent(font, "\u00A0" + global.FAILED + expect);
    }

    var b = CreateElement("b");
    AppendChild(b, font);

    var tt = CreateElement("tt");
    SetTextContent(tt, string);
    AppendChild(tt, b);

    AppendChild(printOutputContainer, tt);
    AppendChild(printOutputContainer, CreateElement("br"));
  }
  global.writeFormattedResult = writeFormattedResult;
})(this);


var gPageCompleted;
var GLOBAL = this + '';

// Variables local to jstests harness.
var jstestsTestPassesUnlessItThrows = false;
var jstestsRestoreFunction;
var jstestsOptions;

/*
 * Signals to this script that the current test case should be considered to
 * have passed if it doesn't throw an exception.
 *
 * Overrides the same-named function in shell.js.
 */
function testPassesUnlessItThrows() {
  jstestsTestPassesUnlessItThrows = true;
}

/*
 * Sets a restore function which restores the standard built-in ECMAScript
 * properties after a destructive test case, and which will be called after
 * the test case terminates.
 */
function setRestoreFunction(restore) {
  jstestsRestoreFunction = restore;
}

window.onerror = function (msg, page, line)
{
  jstestsTestPassesUnlessItThrows = false;

  // Restore options in case a test case used this common variable name.
  options = jstestsOptions;

  // Restore the ECMAScript environment after potentially destructive tests.
  if (typeof jstestsRestoreFunction === "function") {
    jstestsRestoreFunction();
  }

  optionsPush();

  if (typeof DESCRIPTION == 'undefined')
  {
    DESCRIPTION = 'Unknown';
  }
  if (typeof EXPECTED == 'undefined')
  {
    EXPECTED = 'Unknown';
  }

  var testcase = new TestCase("unknown-test-name", DESCRIPTION, EXPECTED, "error");

  if (document.location.href.indexOf('-n.js') != -1)
  {
    // negative test
    testcase.passed = true;
  }

  testcase.reason = page + ':' + line + ': ' + msg;

  reportFailure(msg);

  optionsReset();
};

function gc()
{
  try
  {
    SpecialPowers.forceGC();
  }
  catch(ex)
  {
    print('gc: ' + ex);
  }
}

function options(aOptionName)
{
  // return value of options() is a comma delimited list
  // of the previously set values

  var value = '';
  for (var optionName in options.currvalues)
  {
    value += optionName + ',';
  }
  if (value)
  {
    value = value.substring(0, value.length-1);
  }

  if (aOptionName) {
    if (!(aOptionName in SpecialPowers.Cu)) {
      // This test is trying to flip an unsupported option, so it's
      // likely no longer testing what it was supposed to.  Fail it
      // hard.
      throw "Unsupported JSContext option '"+ aOptionName +"'";
    }

    if (options.currvalues.hasOwnProperty(aOptionName))
      // option is set, toggle it to unset
      delete options.currvalues[aOptionName];
    else
      // option is not set, toggle it to set
      options.currvalues[aOptionName] = true;

    SpecialPowers.Cu[aOptionName] =
      options.currvalues.hasOwnProperty(aOptionName);
  }

  return value;
}

// Keep a reference to options around so that we can restore it after running
// a test case, which may have used this common name for one of its own
// variables.
jstestsOptions = options;

function optionsInit() {

  // hash containing the set options.
  options.currvalues = {
    strict:     true,
    werror:     true,
    strict_mode: true
  };

  // record initial values to support resetting
  // options to their initial values
  options.initvalues = {};

  // record values in a stack to support pushing
  // and popping options
  options.stackvalues = [];

  for (var optionName in options.currvalues)
  {
    var propName = optionName;

    if (!(propName in SpecialPowers.Cu))
    {
      throw "options.currvalues is out of sync with Components.utils";
    }
    if (!SpecialPowers.Cu[propName])
    {
      delete options.currvalues[optionName];
    }
    else
    {
      options.initvalues[optionName] = true;
    }
  }
}

function jsTestDriverBrowserInit()
{

  if (typeof dump != 'function')
  {
    dump = print;
  }

  optionsInit();
  optionsClear();

  if (document.location.search.indexOf('?') != 0)
  {
    // not called with a query string
    return;
  }

  var properties = {};
  var fields = document.location.search.slice(1).split(';');
  for (var ifield = 0; ifield < fields.length; ifield++)
  {
    var propertycaptures = /^([^=]+)=(.*)$/.exec(fields[ifield]);
    if (!propertycaptures)
    {
      properties[fields[ifield]] = true;
    }
    else
    {
      properties[propertycaptures[1]] = decodeURIComponent(propertycaptures[2]);
      if (propertycaptures[1] == 'language')
      {
        // language=(type|language);mimetype
        properties.mimetype = fields[ifield+1];
      }
    }
  }

  if (properties.language != 'type')
  {
    try
    {
      properties.version = /javascript([.0-9]+)/.exec(properties.mimetype)[1];
    }
    catch(ex)
    {
    }
  }

  if (!properties.version && navigator.userAgent.indexOf('Gecko/') != -1)
  {
    // If the version is not specified, and the browser is Gecko,
    // use the default version corresponding to the shell's version(0).
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=522760#c11
    // Otherwise adjust the version to match the suite version for 1.6,
    // and later due to the use of for-each, let, yield, etc.
    //
    // The logic to upgrade the JS version in the shell lives in the
    // corresponding shell.js.
    //
    // Note that js1_8, js1_8_1, and js1_8_5 are treated identically in
    // the browser.
    if (properties.test.match(/^js1_6/))
    {
      properties.version = '1.6';
    }
    else if (properties.test.match(/^js1_7/))
    {
      properties.version = '1.7';
    }
    else if (properties.test.match(/^js1_8/))
    {
      properties.version = '1.8';
    }
    else if (properties.test.match(/^ecma_6\/LexicalEnvironment/))
    {
      properties.version = '1.8';
    }
    else if (properties.test.match(/^ecma_6\/Class/))
    {
      properties.version = '1.8';
    }
    else if (properties.test.match(/^ecma_6\/extensions/))
    {
      properties.version = '1.8';
    }
  }

  // default to language=type;text/javascript. required for
  // reftest style manifests.
  if (!properties.language)
  {
    properties.language = 'type';
    properties.mimetype = 'text/javascript';
  }

  gTestPath = properties.test;

  if (properties.gczeal)
  {
    gczeal(Number(properties.gczeal));
  }

  var testpathparts = properties.test.split(/\//);

  if (testpathparts.length < 2)
  {
    // must have at least suitepath/testcase.js
    return;
  }

  document.write('<title>' + properties.test + '<\/title>');

  // XXX bc - the first document.written script is ignored if the protocol
  // is file:. insert an empty script tag, to work around it.
  document.write('<script></script>');

  // Output script tags for shell.js, then browser.js, at each level of the
  // test path hierarchy.
  var prepath = "";
  var i = 0;
  for (end = testpathparts.length - 1; i < end; i++) {
    prepath += testpathparts[i] + "/";
    outputscripttag(prepath + "shell.js", properties);
    outputscripttag(prepath + "browser.js", properties);
  }

  // Output the test script itself.
  outputscripttag(prepath + testpathparts[i], properties);

  // Finally output the driver-end script to advance to the next test.
  outputscripttag('js-test-driver-end.js', properties);
  return;
}

function outputscripttag(src, properties)
{
  if (!src)
  {
    return;
  }

  var s = '<script src="' +  src + '" charset="utf-8" ';

  if (properties.language != 'type')
  {
    s += 'language="javascript';
    if (properties.version)
    {
      s += properties.version;
    }
  }
  else
  {
    s += 'type="' + properties.mimetype;
    if (properties.version)
    {
      s += ';version=' + properties.version;
    }
  }
  s += '"><\/script>';

  document.write(s);
}

function jsTestDriverEnd()
{
  // gDelayTestDriverEnd is used to
  // delay collection of the test result and
  // signal to Spider so that tests can continue
  // to run after page load has fired. They are
  // responsible for setting gDelayTestDriverEnd = true
  // then when completed, setting gDelayTestDriverEnd = false
  // then calling jsTestDriverEnd()

  if (gDelayTestDriverEnd)
  {
    return;
  }

  window.onerror = null;

  // Restore options in case a test case used this common variable name.
  options = jstestsOptions;

  // Restore the ECMAScript environment after potentially destructive tests.
  if (typeof jstestsRestoreFunction === "function") {
    jstestsRestoreFunction();
  }

  if (jstestsTestPassesUnlessItThrows) {
    var testcase = new TestCase("unknown-test-name", "", true, true);
    print(PASSED);
    jstestsTestPassesUnlessItThrows = false;
  }

  try
  {
    optionsReset();
  }
  catch(ex)
  {
    dump('jsTestDriverEnd ' + ex);
  }

  if (window.opener && window.opener.runNextTest)
  {
    if (window.opener.reportCallBack)
    {
      window.opener.reportCallBack(window.opener.gWindow);
    }
    setTimeout('window.opener.runNextTest()', 250);
  }
  else
  {
    for (var i = 0; i < gTestcases.length; i++)
    {
      gTestcases[i].dump();
    }

    // tell reftest the test is complete.
    document.documentElement.className = '';
    // tell Spider page is complete
    gPageCompleted = true;
  }
}

//var dlog = (function (s) { print('debug: ' + s); });
var dlog = (function (s) {});

// dialog closer from http://bclary.com/projects/spider/spider/chrome/content/spider/dialog-closer.js

var gDialogCloser;
var gDialogCloserObserver;

function registerDialogCloser()
{
  gDialogCloser = SpecialPowers.
    Cc['@mozilla.org/embedcomp/window-watcher;1'].
    getService(SpecialPowers.Ci.nsIWindowWatcher);

  gDialogCloserObserver = {observe: dialogCloser_observe};

  gDialogCloser.registerNotification(gDialogCloserObserver);
}

function unregisterDialogCloser()
{
  gczeal(0);

  if (!gDialogCloserObserver || !gDialogCloser)
  {
    return;
  }

  gDialogCloser.unregisterNotification(gDialogCloserObserver);

  gDialogCloserObserver = null;
  gDialogCloser = null;
}

// use an array to handle the case where multiple dialogs
// appear at one time
var gDialogCloserSubjects = [];

function dialogCloser_observe(subject, topic, data)
{
  if (subject instanceof ChromeWindow && topic == 'domwindowopened' )
  {
    gDialogCloserSubjects.push(subject);
    // timeout of 0 needed when running under reftest framework.
    subject.setTimeout(closeDialog, 0);
  }
}

function closeDialog()
{
  var subject;

  while ( (subject = gDialogCloserSubjects.pop()) != null)
  {
    if (subject.document instanceof XULDocument &&
        subject.document.documentURI == 'chrome://global/content/commonDialog.xul')
    {
      subject.close();
    }
    else
    {
      // alerts inside of reftest framework are not XULDocument dialogs.
      subject.close();
    }
  }
}

registerDialogCloser();
window.addEventListener('unload', unregisterDialogCloser, true);

jsTestDriverBrowserInit();
