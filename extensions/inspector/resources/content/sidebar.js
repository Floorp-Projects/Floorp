/*
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

/***************************************************************
* InspectorSidebar -------------------------------------------------
*  The primary object that controls the Inspector sidebar.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// global variables /////////////////////

var inspector;

//////////// global constants ////////////////////

const kObserverServiceIID  = "@mozilla.org/observer-service;1";

const gNavigator = window._content;

//////////////////////////////////////////////////

window.addEventListener("load", InspectorSidebar_initialize, false);

function InspectorSidebar_initialize()
{
  inspector = new InspectorSidebar();
  inspector.initialize();
}

////////////////////////////////////////////////////////////////////////////
//// class InspectorSidebar

function InspectorSidebar()
{
}

InspectorSidebar.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization

  get document() { return this.mDocPanel.viewer.subject; },

  initialize: function()
  {
    this.installNavObserver();

    this.mPanelSet = document.getElementById("bxPanelSet");
    this.mPanelSet.initialize();
  },
  
  destroy: function()
  {
  },

  getViewer: function(aUID)
  {
    return this.mPanelSet.registry.getViewerByUID(aUID);
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// Navigation
  
  setTargetWindow: function(aWindow)
  {
    this.setTargetDocument(aWindow.document);
  },

  setTargetDocument: function(aDoc)
  {
    this.mPanelSet.getPanel(0).subject = aDoc;
  },

  installNavObserver: function()
  {
		var observerService = XPCU.getService(kObserverServiceIID, "nsIObserverService");
    observerService.AddObserver(NavLoadObserver, "EndDocumentLoad");
  }
};

var NavLoadObserver = {
  Observe: function(aWindow)
  {
    inspector.setTargetWindow(aWindow);
  }
};
