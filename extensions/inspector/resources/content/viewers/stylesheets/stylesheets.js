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
* StylesheetsViewer --------------------------------------------
*  The viewer for the stylesheets loaded by a document.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

//////////// global constants ////////////////////

const kIMPORT_RULE = Components.interfaces.nsIDOMCSSRule.IMPORT_RULE;

//////////////////////////////////////////////////

window.addEventListener("load", StylesheetsViewer_initialize, false);
window.addEventListener("load", StylesheetsViewer_destroy, false);

function StylesheetsViewer_initialize()
{
  viewer = new StylesheetsViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

function StylesheetsViewer_destroy()
{
  viewer.destroy();
}

////////////////////////////////////////////////////////////////////////////
//// class StylesheetsViewer

function StylesheetsViewer()
{
  this.mURL = window.location;
  this.mObsMan = new ObserverManager(this);
}

StylesheetsViewer.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mSubject: null,
  mPane: null,

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  get uid() { return "stylesheets" },
  get pane() { return this.mPane },

  get subject() { return this.mSubject },
  set subject(aObject) 
  {
    var ss = aObject.styleSheets;
    for (var i = 0; i < ss.length; ++i) {
      var rules = ss[i].cssRules;
      for (var k = 0; k < rules.length; ++k) {
        if (rules[k].type == kIMPORT_RULE) {
          var text = document.createElement("text");
          text.setAttribute("value", rules[k].cssText);
          document.documentElement.appendChild(text);
        }
      }
    }
      
  
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

  ////////////////////////////////////////////////////////////////////////////
  //// event dispatching

  addObserver: function(aEvent, aObserver) { this.mObsMan.addObserver(aEvent, aObserver); },
  removeObserver: function(aEvent, aObserver) { this.mObsMan.removeObserver(aEvent, aObserver); },

  ////////////////////////////////////////////////////////////////////////////
  //// stuff
  
  stuff: function()
  {
  }

};

