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
* PseudoClassDialog --------------------------------------------
*  A dialog for choosing the pseudo-classes that should be 
*  imitated on the selected element.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// global variables /////////////////////

var dialog;

//////////// global constants ////////////////////

const gCheckBoxIds = {
  cbxStateHover: 4,
  cbxStateActive: 1,
  cbxStateFocus: 2
};
/////////////////////////////////////////////////

window.addEventListener("load", PseudoClassDialog_initialize, false);

function PseudoClassDialog_initialize()
{
  dialog = new PseudoClassDialog();
  dialog.initialize();
  doSetOKCancel(gDoOKFunc);
}

////////////////////////////////////////////////////////////////////////////
//// class PseudoClassDialog

function PseudoClassDialog() 
{
  this.mOpener = window.opener.viewer;
  this.mSubject = window.arguments[0];

  this.mDOMUtils = XPCU.createInstance("@mozilla.org/inspector/dom-utils;1", "inIDOMUtils");
}

PseudoClassDialog.prototype = 
{
  
  initialize: function()
  {
    var state = this.mDOMUtils.getContentState(this.mSubject);
    
    for (var key in gCheckBoxIds) {
      if (gCheckBoxIds[key] & state) {
        var cbx = document.getElementById(key);
        cbx.setAttribute("checked", "true");
      }
    }
  },
  
  closeDialog: function()
  {
    var el = this.mSubject;
    var root = el.ownerDocument.documentElement;
    
    for (var key in gCheckBoxIds) {
      var cbx = document.getElementById(key);
      if (cbx.checked) 
        this.mDOMUtils.setContentState(el, gCheckBoxIds[key]);
      else
        this.mDOMUtils.setContentState(root, gCheckBoxIds[key]);
    }
    
    window.close();
  }
  
};

////////////////////////////////////////////////////////////////////////////
//// dialogOverlay stuff

function gDoOKFunc() {
  dialog.closeDialog();

  return true;
}
