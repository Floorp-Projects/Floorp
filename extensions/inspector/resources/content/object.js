/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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

