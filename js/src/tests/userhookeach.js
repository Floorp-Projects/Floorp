/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Bob Clary
 */

/*
 * Spider hook function to check if an individual browser based JS test
 * has completed executing.
 */

var gCheckInterval = 1000;
var gCurrentTestId;
var gReport;
var gCurrentTestStart;
var gCurrentTestStop;
var gCurrentTestValid;
var gPageStart;
var gPageStop;

function userOnStart()
{
  try
  {
    dlog('userOnStart');
    registerDialogCloser();
  }
  catch(ex)
  {
    cdump('Spider: FATAL ERROR: userOnStart: ' + ex);
  }
}

function userOnBeforePage()
{
  try
  {
    dlog('userOnBeforePage');
    gPageStart = new Date();

    gCurrentTestId = /test=(.*);language/.exec(gSpider.mCurrentUrl.mUrl)[1];
    gCurrentTestValid = true;
    gCurrentTestStart = new Date();
  }
  catch(ex)
  {
    cdump('Spider: WARNING ERROR: userOnBeforePage: ' + ex);
    gCurrentTestValid = false;
    gPageCompleted = true;
  }
}

function userOnAfterPage()
{
  try
  {
    dlog('userOnAfterPage');
    gPageStop = new Date();

    checkTestCompleted();
  }
  catch(ex)
  {
    cdump('Spider: WARNING ERROR: userOnAfterPage: ' + ex);
    gCurrentTestValid = false;
    gPageCompleted = true;
  }
}

function userOnStop()
{
  try
  {
    // close any pending dialogs
    closeDialog();
    unregisterDialogCloser();
  }
  catch(ex)
  {
    cdump('Spider: WARNING ERROR: userOnStop: ' + ex);
  }
}

function userOnPageTimeout()
{
  gPageStop = new Date();
  if (typeof gSpider.mDocument != 'undefined')
  {
    try
    {
      var win = gSpider.mDocument.defaultView;
      if (win.wrappedJSObject)
      {
        win = win.wrappedJSObject;
      }
      gPageCompleted = win.gPageCompleted = true;
      checkTestCompleted();
    }
    catch(ex)
    {
      cdump('Spider: WARNING ERROR: userOnPageTimeout: ' + ex);
    }
  }
}

function checkTestCompleted()
{
  try
  {
    dlog('checkTestCompleted()');

    var win = gSpider.mDocument.defaultView;
    if (win.wrappedJSObject)
    {
      win = win.wrappedJSObject;
    }

    if (!gCurrentTestValid)
    {
      gPageCompleted = true;
    }
    else if (win.gPageCompleted)
    {
      gCurrentTestStop = new Date();
      // gc to flush out issues quickly
      collectGarbage();

      dlog('Page Completed');

      var gTestcases = win.gTestcases;
      if (typeof gTestcases == 'undefined')
      {
        cdump('JavaScriptTest: ' + gCurrentTestId +
              ' gTestcases array not defined. Possible test failure.');
        throw 'gTestcases array not defined. Possible test failure.';
      }
      else if (gTestcases.length == 0)
      {
        cdump('JavaScriptTest: ' + gCurrentTestId +
              ' gTestcases array is empty. Tests not run.');
        new win.TestCase(win.gTestFile, win.summary, 'Unknown', 'gTestcases array is empty. Tests not run..');
      }
      else
      {
      }
      cdump('JavaScriptTest: ' + gCurrentTestId + ' Elapsed time ' + ((gCurrentTestStop - gCurrentTestStart)/1000).toFixed(2) + ' seconds');

      gPageCompleted = true;
    }
    else
    {
      dlog('page not completed, recheck');
      setTimeout(checkTestCompleted, gCheckInterval);
    }
  }
  catch(ex)
  {
    cdump('Spider: WARNING ERROR: checkTestCompleted: ' + ex);
    gPageCompleted = true;
  } 
}

gConsoleListener.onConsoleMessage =
  function userOnConsoleMessage(s)
{
  dump(s);
};
