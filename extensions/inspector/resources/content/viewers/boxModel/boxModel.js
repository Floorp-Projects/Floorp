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
* BoxModelViewer --------------------------------------------
*  The viewer for the boxModel and visual appearance of an element.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

//////////// global constants ////////////////////

const kIMPORT_RULE = Components.interfaces.nsIDOMCSSRule.IMPORT_RULE;

//////////////////////////////////////////////////

window.addEventListener("load", BoxModelViewer_initialize, false);

function BoxModelViewer_initialize()
{
  viewer = new BoxModelViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

////////////////////////////////////////////////////////////////////////////
//// class BoxModelViewer

function BoxModelViewer()
{
  this.mURL = window.location;
  this.mObsMan = new ObserverManager(this);
}

BoxModelViewer.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mSubject: null,
  mPane: null,
  
  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  get uid() { return "boxModel" },
  get pane() { return this.mPane },

  get subject() { return this.mSubject },
  set subject(aObject) 
  {
    this.mSubject = aObject;
    this.updateStatGroup();
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
  },

  initialize: function(aPane)
  {
    this.mPane = aPane;
    aPane.notifyViewerReady(this);
  },

  destroy: function()
  {
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
  //// statistical updates
  
  updateStatGroup: function()
  {
    var ml = document.getElementById("mlStats");
    this.showStatGroup(ml.value);
  },
  
  showStatGroup: function(aGroup)
  {
    if (aGroup == "position") {
      this.showPositionStats();
    } else if (aGroup == "dimension") {
      this.showDimensionStats();
    } else if (aGroup == "margin") {
      this.showMarginStats();
    } else if (aGroup == "border") {
      this.showBorderStats();
    } else if (aGroup == "padding") {
      this.showPaddingStats();
    }    
  },
  
  showStatistic: function(aCol, aRow, aSide, aSize)
  {
    var label = document.getElementById("txR"+aRow+"C"+aCol+"Label");
    var val = document.getElementById("txR"+aRow+"C"+aCol+"Value");
    label.setAttribute("value", aSide && aSide.length ? aSide + ":" : "");
    val.setAttribute("value", aSize);
  },
  
  showPositionStats: function()
  {
    if ("boxObject" in this.mSubject) { // xul elements
      var bx = this.mSubject.boxObject;
      this.showStatistic(1, 1, "x", bx.x);
      this.showStatistic(1, 2, "y", bx.y);
      this.showStatistic(2, 1, "screen x", bx.screenX);
      this.showStatistic(2, 2, "screen y", bx.screenY);
    } else { // html elements
      this.showStatistic(1, 1, "x", this.mSubject.offsetLeft);
      this.showStatistic(1, 2, "y", this.mSubject.offsetTop);
      this.showStatistic(2, 1, "", "");
      this.showStatistic(2, 2, "", "");
    }
  },
  
  showDimensionStats: function()
  {
    if ("boxObject" in this.mSubject) { // xul elements
      var bx = this.mSubject.boxObject;
      this.showStatistic(1, 1, "box width", bx.width);
      this.showStatistic(1, 2, "box height", bx.height);
      this.showStatistic(2, 1, "content width", "");
      this.showStatistic(2, 2, "content height", "");
      this.showStatistic(3, 1, "", "");
      this.showStatistic(3, 2, "", "");
    } else { // html elements
      this.showStatistic(1, 1, "box width", this.mSubject.offsetWidth);
      this.showStatistic(1, 2, "box height", this.mSubject.offsetHeight);
      this.showStatistic(2, 1, "content width", "");
      this.showStatistic(2, 2, "content height", "");
      this.showStatistic(3, 1, "", "");
      this.showStatistic(3, 2, "", "");
    }
  },

  getSubjectComputedStyle: function()
  {
    var view = this.mSubject.ownerDocument.defaultView;
    return view.getComputedStyle(this.mSubject, "");
  },

  showMarginStats: function()
  {
    var style = this.getSubjectComputedStyle();
    var data = [this.readMarginStyle(style, "top"), this.readMarginStyle(style, "right"), 
                this.readMarginStyle(style, "bottom"), this.readMarginStyle(style, "left")];
    this.showSideStats("margin", data);                
  },

  showBorderStats: function()
  {
    var style = this.getSubjectComputedStyle();
    var data = [this.readBorderStyle(style, "top"), this.readBorderStyle(style, "right"), 
                this.readBorderStyle(style, "bottom"), this.readBorderStyle(style, "left")];
    this.showSideStats("border", data);                
  },

  showPaddingStats: function()
  {
    var style = this.getSubjectComputedStyle();
    var data = [this.readPaddingStyle(style, "top"), this.readPaddingStyle(style, "right"), 
                this.readPaddingStyle(style, "bottom"), this.readPaddingStyle(style, "left")];
    this.showSideStats("padding", data);
  },

  showSideStats: function(aName, aData)
  {
    this.showStatistic(1, 1, aName+"-top", aData[0]);
    this.showStatistic(2, 1, aName+"-right", aData[1]);
    this.showStatistic(1, 2, aName+"-bottom", aData[2]);
    this.showStatistic(2, 2, aName+"-left", aData[3]);
    this.showStatistic(3, 1, "", "");
    this.showStatistic(3, 2, "", "");
  },
  
  readMarginStyle: function(aStyle, aSide)
  {
    return aStyle.getPropertyCSSValue("margin-"+aSide).cssText;
  },
  
  readPaddingStyle: function(aStyle, aSide)
  {
    return aStyle.getPropertyCSSValue("padding-"+aSide).cssText;
  },
  
  readBorderStyle: function(aStyle, aSide)
  {
    var style = aStyle.getPropertyCSSValue("border-"+aSide+"-style").cssText;
    if (!style || !style.length) {
      return "none";
    } else {
      return aStyle.getPropertyCSSValue("border-"+aSide+"-width").cssText + " " + 
             style + " " +
             aStyle.getPropertyCSSValue("border-"+aSide+"-color").cssText;
    }
  }
};
