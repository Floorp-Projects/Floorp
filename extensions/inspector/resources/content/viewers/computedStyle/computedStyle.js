/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/***************************************************************
* ComputedStyleViewer --------------------------------------------
*  The viewer for the computed css styles on a DOM element.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

//////////// global constants ////////////////////

//////////////////////////////////////////////////

window.addEventListener("load", ComputedStyleViewer_initialize, false);

function ComputedStyleViewer_initialize()
{
  viewer = new ComputedStyleViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

////////////////////////////////////////////////////////////////////////////
//// class ComputedStyleViewer

function ComputedStyleViewer()
{
  this.mObsMan = new ObserverManager(this);
  this.mURL = window.location;
  
  this.mTree = document.getElementById("olStyles");
  this.mOlBox = this.mTree.treeBoxObject;
}

ComputedStyleViewer.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mSubject: null,
  mPane: null,

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  get uid() { return "computedStyle" },
  get pane() { return this.mPane },

  get subject() { return this.mSubject },
  set subject(aObject) 
  {
    this.mOlBox.view = new ComputedStyleView(aObject);
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
  },

  initialize: function(aPane)
  {
    this.mPane = aPane;
    aPane.notifyViewerReady(this);
  },

  destroy: function()
  {
    // We need to remove the view at this time or else it will attempt to 
    // re-paint while the document is being deconstructed, resulting in
    // some nasty XPConnect assertions
    this.mOlBox.view = null;
  },

  isCommandEnabled: function(aCommand)
  {
    return false;
  },
  
  getCommand: function(aCommand)
  {
    return null;
  },

  ////////////////////////////////////////////////////////////////////////////
  //// event dispatching

  addObserver: function(aEvent, aObserver) { this.mObsMan.addObserver(aEvent, aObserver); },
  removeObserver: function(aEvent, aObserver) { this.mObsMan.removeObserver(aEvent, aObserver); },

  ////////////////////////////////////////////////////////////////////////////
  //// stuff

  onItemSelected: function()
  {
  }
};

////////////////////////////////////////////////////////////////////////////
//// ComputedStyleView

function ComputedStyleView(aObject)
{
  var view = aObject.ownerDocument.defaultView;
  this.mStyleList = view.getComputedStyle(aObject, "");
  this.mRowCount = this.mStyleList.length;
}

ComputedStyleView.prototype = new inBaseTreeView();

ComputedStyleView.prototype.getCellText = 
function(aRow, aCol) 
{
  if (aCol.id == "olcStyleName") {
    return this.mStyleList.item(aRow);
  } else if (aCol.id == "olcStyleValue") {
    var prop = this.mStyleList.item(aRow);
    return this.mStyleList.getPropertyValue(prop);
  }
  
  return "";
}