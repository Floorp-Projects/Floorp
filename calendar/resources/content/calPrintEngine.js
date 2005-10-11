/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Garth Smedley <garths@oeone.com>
 *                 Mike Potter <mikep@oeone.com>
 *                 Colin Phillips <colinp@oeone.com> 
 *                 Chris Charabaruk <ccharabaruk@meldstar.com>
 *                 ArentJan Banck <ajbanck@planet.nl>
 *                 Chris Allen
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
 *                 Joey Minta <jminta@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


/*-----------------------------------------------------------------
 *   W I N D O W      V A R I A B L E S
 */

var gMyTitle;
var gContent;
var gShowprivate ;
var gCalendarWindow ;
var gHtmlDocument;
var gTempFile = null;
var gTempUri;

var gPrintSettingsAreGlobal = true;
var gSavePrintSettings = true;
var gPrintSettings = null;
var gWebProgress = null;

/*-----------------------------------------------------------------
 *   W I N D O W      F U N C T I O N S
 */

function getWebNavigation()
{
  try {
    return gContent.webNavigation;
  } catch (e) {
    return null;
  }
}

var gPrintPreviewObs = {
    observe: function(aSubject, aTopic, aData)
    {
      setTimeout(FinishPrintPreview, 0);
    },

    QueryInterface : function(iid)
    {
     if (iid.equals(Components.interfaces.nsIObserver) || iid.equals(Components.interfaces.nsISupportsWeakReference))
      return this;
     
     throw Components.results.NS_NOINTERFACE;
    }
};

function BrowserPrintPreview()
{
  var ifreq;
  var webBrowserPrint;  
  try {
    ifreq = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    webBrowserPrint = ifreq.getInterface(Components.interfaces.nsIWebBrowserPrint);     
  } catch (e) {
    // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
    // causing an exception to be thrown which we catch here.
    // Unfortunately this will also consume helpful failures, so add a
    // dump(e); // if you need to debug
  }
  try {
    gPrintSettings = GetPrintSettings();
  } catch(e) {
    gPrintSettings = PrintUtils.getPrintSettings();
  }


  gWebProgress = new Object();

  var printPreviewParams    = new Object();
  var notifyOnOpen          = new Object();
  var printingPromptService = Components.classes["@mozilla.org/embedcomp/printingprompt-service;1"]
                                  .getService(Components.interfaces.nsIPrintingPromptService);
  if (printingPromptService) {
    // just in case we are already printing,
    // an error code could be returned if the Prgress Dialog is already displayed
    try {
      printingPromptService.showProgress(this, webBrowserPrint, gPrintSettings, gPrintPreviewObs, false, gWebProgress,
                                         printPreviewParams, notifyOnOpen);
      if (printPreviewParams.value) {
        var webNav = getWebNavigation();
        printPreviewParams.value.docTitle = window.arguments[0].title;
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
  try {
    var ifreq = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    var webBrowserPrint = ifreq.getInterface(Components.interfaces.nsIWebBrowserPrint);     
    if (webBrowserPrint) {
      try {
        gPrintSettings = GetPrintSettings();
      } catch(e) {
        gPrintSettings = PrintUtils.getPrintSettings();
      }
      // Don't print the fake title and url
      gPrintSettings.docURL=" ";
      gPrintSettings.title=window.arguments[0].title;
      
      webBrowserPrint.printPreview(gPrintSettings, null, gWebProgress.value);
    }
    showPrintPreviewToolbar();

    content.focus();
  } catch (e) {
    // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
    // causing an exception to be thrown which we catch here.
    // Unfortunately this will also consume helpful failures, so add a
    // dump(e); // if you need to debug
  }
}

/*-----------------------------------------------------------------
 *   Called when the dialog is loaded.
 */

function OnLoadPrintEngine(){
  var args = window.arguments[0];
  gContent = document.getElementById("content");

  var htmlexporter = Components.classes["@mozilla.org/calendar/export;1?type=html"].createInstance(Components.interfaces.calIExporter);

  // Fail-safe check to not init twice, to prevent leaking files
  if (!gTempFile) {
    const nsIFile = Components.interfaces.nsIFile;
    var dirService = Components.classes["@mozilla.org/file/directory_service;1"]
                               .getService(Components.interfaces.nsIProperties);
    gTempFile = dirService.get("TmpD", nsIFile);
    gTempFile.append("calendarPrint.html");
    gTempFile.createUnique(nsIFile.NORMAL_FILE_TYPE, 0600);
    dump("tf: "+gTempFile.path+"\n");
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
    var gTempUri = ioService.newFileURI(gTempFile); 
  }
  const nsIFileOutputStream = Components.interfaces.nsIFileOutputStream;
  var outputStream;
  var stream = Components.classes["@mozilla.org/network/file-output-stream;1"]
                         .createInstance(Components.interfaces.nsIFileOutputStream);
  try {
    stream.init(gTempFile, 0x2A, 0600, 0);
    htmlexporter.exportToStream(stream, args.eventList.length, args.eventList);
    stream.close();
  }
  catch(ex) {
    dump('print failed to export:'+ ex);
  }

  gHtmlDocument = window.content.document;
  gHtmlDocument.location.href = gTempUri.spec;
  BrowserPrintPreview();
}

function OnUnloadPrintEngine()
{
  gTempFile.remove(false);
}

function showPrintPreviewToolbar()
{
  const kXULNS = 
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

  var printPreviewTB = document.createElementNS(kXULNS, "toolbar");
  printPreviewTB.setAttribute("printpreview", true);
  printPreviewTB.setAttribute("id", "print-preview-toolbar");

  gContent.parentNode.insertBefore(printPreviewTB, gContent);
}

function BrowserExitPrintPreview()
{
  window.close();
}

// hack around the toolkit problems in bug 270235
function getBrowser()
{
  BrowserExitPrintPreview();
}
