/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* This is where functions related to the print engine are kept */

/* globals for a particular window */
var printEngineContractID      = "@mozilla.org/messenger/msgPrintEngine;1";
var printEngineWindow;
var printEngine;
var printSettings = null;
var doingPrintPreview = false;
var gWebProgress;

const kMsgBundle = "chrome://messenger/locale/messenger.properties";

/* Functions related to startup */
function OnLoadPrintEngine()
{
  PrintEngineCreateGlobals();
	InitPrintEngineWindow();
  printEngine.startPrintOperation(printSettings);
}

function OnUnloadPrintEngine()
{
  if (printEngine.doPrintPreview) {
    var webBrowserPrint = printEngine.webBrowserPrint;
    webBrowserPrint.exitPrintPreview(); 
  }
}

function PrintEngineCreateGlobals()
{
	/* get the print engine instance */
	printEngine = Components.classes[printEngineContractID].createInstance();
	printEngine = printEngine.QueryInterface(Components.interfaces.nsIMsgPrintEngine);
}

function getWebNavigation()
{
  try {
    return document.getElementById("content").webNavigation;
  } catch (e) {
    return null;
  }
}

function showPrintPreviewToolbar()
{
  const kXULNS = 
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

  var printPreviewTB = document.createElementNS(kXULNS, "toolbar");
  printPreviewTB.setAttribute("printpreview", true);
  printPreviewTB.setAttribute("id", "print-preview-toolbar");

  var navToolbox = document.getElementById("content");
  navToolbox.parentNode.insertBefore(printPreviewTB, navToolbox);
  
}

function BrowserExitPrintPreview()
{
  window.close();
}

// This observer is called once the progress dialog has been "opened"
var gPrintPreviewObs = {
  observe: function(aSubject, aTopic, aData)
  {
    setTimeout(FinishPrintPreview, 0);
  },

  QueryInterface : function(iid)
  {
    if (iid.equals(Components.interfaces.nsIObserver) ||
        iid.equals(Components.interfaces.nsISupportsWeakReference) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_NOINTERFACE;
  }
};

function getBundle(aURI)
{
  if (!aURI)
    return null;

  var bundle = null;
  try
  {
    var strBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"].
      getService(Components.interfaces.nsIStringBundleService);
    bundle = strBundleService.createBundle(aURI);
  }
  catch (ex)
  {
    bundle = null;
    debug("Exception getting bundle " + aURI + ": " + ex);
  }

  return bundle;
}

function setPPTitle(aTitle)
{
  var title = aTitle;
  try {
  var gBrandBundle = document.getElementById("bundle_brand");
  if (gBrandBundle) {
    var msgBundle = this.getBundle(kMsgBundle);
    if (msgBundle) {
        var brandStr = gBrandBundle.getString("brandShortName")
        var array = [title, brandStr];
        title = msgBundle.formatStringFromName("PreviewTitle", array, array.length);
      }
    }
  } catch (e) {}
  document.title = title;
}

function PrintPreview()
{
  var webBrowserPrint = printEngine.webBrowserPrint;

  // Here we get the PrintingPromptService tso we can display the PP Progress from script
  // For the browser implemented via XUL with the PP toolbar we cannot let it be
  // automatically opened from the print engine because the XUL scrollbars in the PP window
  // will layout before the content window and a crash will occur.
  //
  // Doing it all from script, means it lays out before hand and we can let printing do it's own thing
  gWebProgress = new Object();

  var printPreviewParams    = new Object();
  var notifyOnOpen          = new Object();
  var printingPromptService = Components.classes["@mozilla.org/embedcomp/printingprompt-service;1"]
                                  .getService(Components.interfaces.nsIPrintingPromptService);
  if (printingPromptService) {
    // just in case we are already printing, 
    // an error code could be returned if the Prgress Dialog is already displayed
    try {
      printingPromptService.showProgress(this, webBrowserPrint, printSettings, gPrintPreviewObs, false, gWebProgress, 
                                         printPreviewParams, notifyOnOpen);
      if (printPreviewParams.value) {
        var webNav = getWebNavigation();
        printPreviewParams.value.docTitle = webNav.document.title;
        printPreviewParams.value.docURL   = webNav.currentURI.spec;
      }

      // this tells us whether we should continue on with PP or 
      // wait for the callback via the observer
      if (!notifyOnOpen.value.valueOf() || gWebProgress.value == null) {
        FinishPrintPreview();
      }
    } catch (e) {
      FinishPrintPreview();
    }
  }
}

function FinishPrintPreview()
{
  var webBrowserPrint = printEngine.webBrowserPrint;
  try {
    if (webBrowserPrint) {
      webBrowserPrint.printPreview(printSettings, null, gWebProgress.value);
    }
 
    // show the toolbar after we go into print preview mode so
    // that we can initialize the toolbar with total num pages
    showPrintPreviewToolbar();
    setPPTitle(getWebNavigation().document.title);

    content.focus();
  } catch (e) {
    // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
    // causing an exception to be thrown which we catch here.
    // Unfortunately this will also consume helpful failures, so add a
    //dump(e+"\n");
  }
  printEngine.showWindow(true);
}


// Pref listener constants
const gStartupPPObserver =
{
  printengine:null,
  observe: function(subject, topic, prefName)
  {
    this.printengine.PrintPreview();
  }
};

function InitPrintEngineWindow()
{
  /* Tell the nsIPrintEngine object what window is rendering the email */
  printEngine.setWindow(window);

  /* hide the printEngine window.  see bug #73995 */

  /* See if we got arguments.
   * Window was opened via window.openDialog.  Copy argument
   * and perform compose initialization 
   */
  if ( window.arguments && window.arguments[0] != null ) {
    var numSelected = window.arguments[0];
    var uriArray = window.arguments[1];
    var statusFeedback = window.arguments[2];
    if (window.arguments[3]) {
      printSettings = window.arguments[3].QueryInterface(Components.interfaces.nsIPrintSettings);
      if (printSettings) {
        printSettings.isCancelled = false;
      }
    }

    if (window.arguments[4]) {
      doingPrintPreview = window.arguments[4];
      //printEngine.showWindow(doingPrintPreview);
      printEngine.doPrintPreview = doingPrintPreview;
    } else {
      printEngine.doPrintPreview = false;
    }
    printEngine.showWindow(false);

    if (window.arguments.length > 5) {
      printEngine.setMsgType(window.arguments[5]);
    } else {
      printEngine.setMsgType(Components.interfaces.nsIMsgPrintEngine.MNAB_START);
    }

    if (window.arguments.length > 6) {
      printEngine.setParentWindow(window.arguments[6]);
    } else {
      printEngine.setParentWindow(null);
    }

    gStartupPPObserver.printengine = this;
    printEngine.setStatusFeedback(statusFeedback);
    printEngine.setStartupPPObserver(gStartupPPObserver);

    if (numSelected > 0) {
      printEngine.setPrintURICount(numSelected);
      for (var i = 0; i < numSelected; i++) {
        printEngine.addPrintURI(uriArray[i]);      
        //dump(uriArray[i] + "\n");
      }	    
    }
  }
}

function ClearPrintEnginePane()
{
  if (window.frames["content"].location != "about:blank")
      window.frames["content"].location = "about:blank";
}

function StopUrls()
{
  printEngine.stopUrls();
}

function PrintEnginePrint()
{
  printEngineWindow = window.openDialog("chrome://messenger/content/msgPrintEngine.xul", "", "chrome,dialog=no,all,centerscreen", false);
}

function PrintEnginePrintPreview()
{
  printEngineWindow = window.openDialog("chrome://messenger/content/msgPrintEngine.xul", "", "chrome,dialog=no,all,centerscreen", true);
}
