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
* ObjectApp -------------------------------------------------
*  The primary object that controls the Inspector compact app.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// global variables /////////////////////

var inspector;

//////////// global constants ////////////////////

//////////////////////////////////////////////////

window.addEventListener("load", ObjectApp_initialize, false);
window.addEventListener("unload", ObjectApp_destroy, false);

function ObjectApp_initialize()
{
  inspector = new ObjectApp();
  inspector.initialize();
}

function ObjectApp_destroy()
{
  inspector.destroy();
}

////////////////////////////////////////////////////////////////////////////
//// class ObjectApp

function ObjectApp()
{
}

ObjectApp.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization

  initialize: function()
  {
    this.mInitTarget = window.arguments && window.arguments.length > 0 ? window.arguments[0] : null;

    this.mPanelSet = document.getElementById("bxPanelSet");
    this.mPanelSet.addObserver("panelsetready", this);
    this.mPanelSet.initialize();
  },
  
  destroy: function()
  {
  },
  
  getViewer: function(aUID)
  {
    return this.mPanelSet.registry.getViewerByUID(aUID);
  },
  
  onEvent: function(aEvent)
  {
    switch (aEvent.type) {
      case "panelsetready":
        this.initViewerPanels();
    }
  },

  initViewerPanels: function()
  {
    if (this.mInitTarget)
      this.target = this.mInitTarget;
  },
  
  set target(aObj)
  {
    this.mPanelSet.getPanel(0).subject = aObj;
  }
};

////////////////////////////////////////////////////////////////////////////
//// event listeners

