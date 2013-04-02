/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
 * Requests to load the given JavaScript file before the file containing the
 * test case.
 */
function include(file) {
  outputscripttag(file, {language: "type", mimetype: "text/javascript"});
}

/*
 * Sets a restore function which restores the standard built-in ECMAScript
 * properties after a destructive test case, and which will be called after
 * the test case terminates.
 */
function setRestoreFunction(restore) {
  jstestsRestoreFunction = restore;
}

function htmlesc(str) {
  if (str == '<')
    return '&lt;';
  if (str == '>')
    return '&gt;';
  if (str == '&')
    return '&amp;';
  return str;
}

function DocumentWrite(s)
{
  try
  {
    var msgDiv = document.createElement('div');
    msgDiv.innerHTML = s;
    document.body.appendChild(msgDiv);
    msgDiv = null;
  }
  catch(excp)
  {
    document.write(s + '<br>\n');
  }
}

function print() {
  var s = '';
  var a;
  for (var i = 0; i < arguments.length; i++)
  {
    a = arguments[i];
    s += String(a) + ' ';
  }

  if (typeof dump == 'function')
  {
    dump( s + '\n');
  }

  s = s.replace(/[<>&]/g, htmlesc);

  DocumentWrite(s);
}

function writeHeaderToLog( string ) {
  string = String(string);

  if (typeof dump == 'function')
  {
    dump( string + '\n');
  }

  string = string.replace(/[<>&]/g, htmlesc);

  DocumentWrite( "<h2>" + string + "</h2>" );
}

function writeFormattedResult( expect, actual, string, passed ) {
  string = String(string);

  if (typeof dump == 'function')
  {
    dump( string + '\n');
  }

  string = string.replace(/[<>&]/g, htmlesc);

  var s = "<tt>"+ string ;
  s += "<b>" ;
  s += ( passed ) ? "<font color=#009900> &nbsp;" + PASSED
    : "<font color=#aa0000>&nbsp;" +  FAILED + expect + "</tt>";

  DocumentWrite( s + "</font></b></tt><br>" );
  return passed;
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
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
    Components.utils.forceGC();
  }
  catch(ex)
  {
    print('gc: ' + ex);
  }
}

function jsdgc()
{
  try
  {
    // Thanks to dveditz
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
    var jsdIDebuggerService = Components.interfaces.jsdIDebuggerService;
    var service = Components.classes['@mozilla.org/js/jsd/debugger-service;1'].
      getService(jsdIDebuggerService);
    service.GC();
  }
  catch(ex)
  {
    print('jsdgc: ' + ex);
  }
}

function quit()
{
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
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
    if (!(aOptionName in Components.utils)) {
//    if (!(aOptionName in SpecialPowers.wrap(Components).utils)) {
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

//    SpecialPowers.wrap(Components).utils[aOptionName] = options.currvalues.hasOwnProperty(aOptionName);
    Components.utils[aOptionName] =
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
    methodjit:  true,
    methodjit_always: true,
    strict_mode: true
  };

  // record initial values to support resetting
  // options to their initial values
  options.initvalues = {};

  // record values in a stack to support pushing
  // and popping options
  options.stackvalues = [];

  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  for (var optionName in options.currvalues)
  {
    var propName = optionName;

//    if (!(propName in SpecialPowers.wrap(Components).utils))
    if (!(propName in Components.utils))
    {
      throw "options.currvalues is out of sync with Components.utils";
    }
//    if (!SpecialPowers.wrap(Components).utils[propName])
    if (!Components.utils[propName])
    {
      delete options.currvalues[optionName];
    }
    else
    {
      options.initvalues[optionName] = true;
    }
  }
}

function gczeal(z)
{
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  Components.utils.setGCZeal(z);
}

function jit(on)
{
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

  /*
   * since the default setting of jit changed from false to true
   * in http://hg.mozilla.org/tracemonkey/rev/685e00e68be9
   * bisections which depend upon jit settings can be thrown off.
   * default jit(false) when not running jsreftests to make bisections
   * depending upon jit settings consistent over time. This is not needed
   * in shell tests as the default jit setting has not changed there.
   */

  if (properties.jit  || !document.location.href.match(/jsreftest.html/))
    jit(properties.jit);

  var testpathparts = properties.test.split(/\//);

  if (testpathparts.length < 3)
  {
    // must have at least suitepath/subsuite/testcase.js
    return;
  }
  var suitepath = testpathparts.slice(0,testpathparts.length-2).join('/');
  var subsuite = testpathparts[testpathparts.length - 2];
  var test     = testpathparts[testpathparts.length - 1];

  document.write('<title>' + suitepath + '/' + subsuite + '/' + test + '<\/title>');

  // XXX bc - the first document.written script is ignored if the protocol
  // is file:. insert an empty script tag, to work around it.
  document.write('<script></script>');

  // Enable a test suite that has more than two levels of directories to
  // provide browser.js and shell.js in its base directory.
  // This assumes that suitepath is a relative path, as is the case in the
  // try server environment. Absolute paths are not allowed.
  if (suitepath.indexOf('/') !== -1) {
    var base = suitepath.slice(0, suitepath.indexOf('/'));
    outputscripttag(base + '/shell.js', properties);
    outputscripttag(base + '/browser.js', properties);
  }

  outputscripttag(suitepath + '/shell.js', properties);
  outputscripttag(suitepath + '/browser.js', properties);
  outputscripttag(suitepath + '/' + subsuite + '/shell.js', properties);
  outputscripttag(suitepath + '/' + subsuite + '/browser.js', properties);
  outputscripttag(suitepath + '/' + subsuite + '/' + test, properties);
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
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
//  gDialogCloser = SpecialPowers.wrap(Components).
  gDialogCloser = Components.
    classes['@mozilla.org/embedcomp/window-watcher;1'].
    getService(Components.interfaces.nsIWindowWatcher);

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
