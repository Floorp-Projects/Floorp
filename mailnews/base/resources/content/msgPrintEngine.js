/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

/* This is where functions related to the print engine are kept */

/* globals for a particular window */
var printEngineContractID      = "@mozilla.org/messenger/msgPrintEngine;1";
var printEngineWindow;
var printEngine;
var printSettings;

/* Functions related to startup */
function OnLoadPrintEngine()
{
  PrintEngineCreateGlobals();
	InitPrintEngineWindow();
  printEngine.startPrintOperation(printSettings);
}

function OnUnloadPrintEngine()
{
}

function PrintEngineCreateGlobals()
{
	/* get the print engine instance */
	printEngine = Components.classes[printEngineContractID].createInstance();
	printEngine = printEngine.QueryInterface(Components.interfaces.nsIMsgPrintEngine);
}

function InitPrintEngineWindow()
{
  /* Tell the nsIPrintEngine object what window is rendering the email */
  printEngine.setWindow(window);

  /* hide the printEngine window.  see bug #73995 */
  printEngine.showWindow(false);

  /* See if we got arguments.
   * Window was opened via window.openDialog.  Copy argument
   * and perform compose initialization 
   */
  if ( window.arguments && window.arguments[0] != null ) {
    var numSelected = window.arguments[0];
    var uriArray = window.arguments[1];
    var statusFeedback = window.arguments[2];
    printSettings = window.arguments[3].QueryInterface(Components.interfaces.nsIPrintSettings);
    if (printSettings) {
      printSettings.isCancelled = false;
    }

    printEngine.setStatusFeedback(statusFeedback);

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
  if (window.frames["printengine"].location != "about:blank")
      window.frames["printengine"].location = "about:blank";
}

function StopUrls()
{
  printEngine.stopUrls();
}

function PrintEnginePrint()
{
  printEngineWindow = window.openDialog("chrome://messenger/content/msgPrintEngine.xul", "", "chrome,dialog=no,all,centerscreen");
}
