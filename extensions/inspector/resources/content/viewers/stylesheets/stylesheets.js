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

