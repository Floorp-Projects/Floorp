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
window.addEventListener("unload", ComputedStyleViewer_destroy, false);

function ComputedStyleViewer_initialize()
{
  viewer = new ComputedStyleViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

function ComputedStyleViewer_destroy()
{
  viewer.destroy();
}

////////////////////////////////////////////////////////////////////////////
//// class ComputedStyleViewer

function ComputedStyleViewer()
{
  this.mObsMan = new ObserverManager(this);
  this.mURL = window.location;
  
  this.mOutliner = document.getElementById("olStyles");
  var bx = this.mOutliner.boxObject;
  this.mOlBox = bx.QueryInterface(Components.interfaces.nsIOutlinerBoxObject);
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
  },

  initialize: function(aPane)
  {
    this.mPane = aPane;
    aPane.notifyViewerReady(this);
  },

  destroy: function()
  {
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

ComputedStyleView.prototype = new inBaseOutlinerView();

ComputedStyleView.prototype.getCellText = 
function(aRow, aColId) 
{
  if (aColId == "olcStyleName") {
    return this.mStyleList.item(aRow);
  } else if (aColId == "olcStyleValue") {
    var prop = this.mStyleList.item(aRow);
    var cssval = this.mStyleList.getPropertyCSSValue(prop);
    return cssval ? cssval.cssText : "";
  }
  
  return "";
}