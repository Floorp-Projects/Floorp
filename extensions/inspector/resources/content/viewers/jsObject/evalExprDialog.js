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
* EvalExprDialog --------------------------------------------
*  A dialog for entering javascript expression to evaluate and
* view in the JS Object Viewer.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// global variables /////////////////////

var dialog;

//////////////////////////////////////////////////

window.addEventListener("load", EvalExprDialog_initialize, false);

function EvalExprDialog_initialize()
{
  dialog = new EvalExprDialog();
  dialog.initialize();
}

////////////////////////////////////////////////////////////////////////////
//// class EvalExprDialog

function EvalExprDialog()
{
  this.mViewer = window.arguments[0];
  this.mTarget = window.arguments[1];
}

EvalExprDialog.prototype = 
{
  initialize: function()
  {
    var txf = document.getElementById("txfExprInput");
    txf.focus();
  },

  doExec: function()
  {
    var txf = document.getElementById("txfExprInput");
    var cbx = document.getElementById("cbxNewView");
    this.mViewer.doEvalExpr(txf.value, this.mTarget, cbx.checked);
    
    window.close();
  },
  
  doCancel: function()
  {
    window.close();
  }

};
