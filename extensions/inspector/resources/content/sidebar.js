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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
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

function InspectorSidebar_initialize()
{
  inspector = new InspectorSidebar();
  inspector.initialize();
}

window.addEventListener("load", InspectorSidebar_initialize, false);

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
    this.mPanelSet.addObserver("panelsetready", this, false);
    this.mPanelSet.initialize();
  },
  
  destroy: function()
  {
  },

  doViewerCommand: function(aCommand)
  {
    this.mPanelSet.execCommand(aCommand);
  },
  
  getViewer: function(aUID)
  {
    return this.mPanelSet.registry.getViewerByUID(aUID);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Viewer Panels
  
  initViewerPanels: function()
  {
    this.mDocPanel = this.mPanelSet.getPanel(0);
    this.mDocPanel.addObserver("subjectChange", this, false);
    this.mObjectPanel = this.mPanelSet.getPanel(1);
  },

  onEvent: function(aEvent)
  {
    if (aEvent.type == "panelsetready") {
      this.initViewerPanels();
    }
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
    observerService.addObserver(NavLoadObserver, "EndDocumentLoad", false);
  }
};

var NavLoadObserver = {
  observe: function(aWindow)
  {
    inspector.setTargetWindow(aWindow);
  }
};
