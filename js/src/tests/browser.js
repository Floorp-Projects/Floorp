/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var gPageCompleted;
var GLOBAL = this + '';

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

  if (aOptionName === 'moar_xml')
    aOptionName = 'xml';

  if (aOptionName && aOptionName !== 'allow_xml') {
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
    if (!(aOptionName in Components.utils))
    {
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

    Components.utils[aOptionName] =
      options.currvalues.hasOwnProperty(aOptionName);
  }  

  return value;
}

function optionsInit() {

  // hash containing the set options.
  options.currvalues = {
    strict:     true,
    werror:     true,
    atline:     true,
    moar_xml:   true,
    relimit:    true,
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
    if (optionName === "moar_xml")
      propName = "xml";

    if (!(propName in Components.utils))
    {
      throw "options.currvalues is out of sync with Components.utils";
    }
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
    // Otherwise adjust the version to match the suite version for 1.7,
    // and later due to the use of let, yield, etc.
    //
    // Note that js1_8, js1_8_1, and js1_8_5 are treated identically in
    // the browser.
    if (properties.test.match(/^js1_7/))
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

  outputscripttag(suitepath + '/shell.js', properties);
  outputscripttag(suitepath + '/browser.js', properties);
  outputscripttag(suitepath + '/' + subsuite + '/shell.js', properties);
  outputscripttag(suitepath + '/' + subsuite + '/browser.js', properties);
  outputscripttag(suitepath + '/' + subsuite + '/' + test, properties,
  	properties.e4x || /e4x\//.test(properties.test));
  outputscripttag('js-test-driver-end.js', properties);
  return;
}

function outputscripttag(src, properties, e4x)
{
  if (!src)
  {
    return;
  }

  if (e4x)
  {
    // e4x requires type=mimetype;e4x=1
    properties.language = 'type';
  }

  var s = '<script src="' +  src + '" ';

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
    if (e4x)
    {
      s += ';e4x=1';
    }
  }
  s += '"><\/script>';

  document.write(s);
}

var JSTest = {
  waitForExplicitFinish: function () {
    gDelayTestDriverEnd = true;
  },

  testFinished: function () {
    gDelayTestDriverEnd = false;
    jsTestDriverEnd();
  }
};

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
  dlog('registerDialogCloser: start');
  try
  {
    netscape.security.PrivilegeManager.
      enablePrivilege('UniversalXPConnect');
  }
  catch(excp)
  {
    print('registerDialogCloser: ' + excp);
    return;
  }

  gDialogCloser = Components.
    classes['@mozilla.org/embedcomp/window-watcher;1'].
    getService(Components.interfaces.nsIWindowWatcher);

  gDialogCloserObserver = {observe: dialogCloser_observe};

  gDialogCloser.registerNotification(gDialogCloserObserver);

  dlog('registerDialogCloser: complete');
}

function unregisterDialogCloser()
{
  dlog('unregisterDialogCloser: start');

  gczeal(0);

  if (!gDialogCloserObserver || !gDialogCloser)
  {
    return;
  }
  try
  {
    netscape.security.PrivilegeManager.
      enablePrivilege('UniversalXPConnect');
  }
  catch(excp)
  {
    print('unregisterDialogCloser: ' + excp);
    return;
  }

  gDialogCloser.unregisterNotification(gDialogCloserObserver);

  gDialogCloserObserver = null;
  gDialogCloser = null;

  dlog('unregisterDialogCloser: stop');
}

// use an array to handle the case where multiple dialogs
// appear at one time
var gDialogCloserSubjects = [];

function dialogCloser_observe(subject, topic, data)
{
  try
  {
    netscape.security.PrivilegeManager.
      enablePrivilege('UniversalXPConnect');

    dlog('dialogCloser_observe: ' +
         'subject: ' + subject + 
         ', topic=' + topic + 
         ', data=' + data + 
         ', subject.document.documentURI=' + subject.document.documentURI +
         ', subjects pending=' + gDialogCloserSubjects.length);
  }
  catch(excp)
  {
    print('dialogCloser_observe: ' + excp);
    return;
  }

  if (subject instanceof ChromeWindow && topic == 'domwindowopened' )
  {
    gDialogCloserSubjects.push(subject);
    // timeout of 0 needed when running under reftest framework.
    subject.setTimeout(closeDialog, 0);
  }
  dlog('dialogCloser_observe: subjects pending: ' + gDialogCloserSubjects.length);
}

function closeDialog()
{
  var subject;
  dlog('closeDialog: subjects pending: ' + gDialogCloserSubjects.length);

  while ( (subject = gDialogCloserSubjects.pop()) != null)
  {
    dlog('closeDialog: subject=' + subject);

    dlog('closeDialog: subject.document instanceof XULDocument: ' + (subject.document instanceof XULDocument));
    dlog('closeDialog: subject.document.documentURI: ' + subject.document.documentURI);

    if (subject.document instanceof XULDocument && 
        subject.document.documentURI == 'chrome://global/content/commonDialog.xul')
    {
      dlog('closeDialog: close XULDocument dialog?');
      subject.close();
    }
    else
    {
      // alerts inside of reftest framework are not XULDocument dialogs.
      dlog('closeDialog: close chrome dialog?');
      subject.close();
    }
  }
}

registerDialogCloser();
window.addEventListener('unload', unregisterDialogCloser, true);

jsTestDriverBrowserInit();
