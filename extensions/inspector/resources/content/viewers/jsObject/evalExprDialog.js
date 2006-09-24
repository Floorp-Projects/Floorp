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
 *   Jason Barnabe <jason_barnabe@fastmail.fm>
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
* EvalExprDialog --------------------------------------------
*  A dialog for entering javascript expression to evaluate and
* view in the JS Object Viewer.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

var gViewer = window.arguments[0];
var gTarget = window.arguments[1];

/**
 * Executes the JavaScript expression entered by the user.
 */
function execute()
{
  var txf = document.getElementById("txfExprInput");
  var rad = document.getElementById("inspect-new-window");
  try {
    gViewer.doEvalExpr(txf.value, gTarget, rad.selected);
  } catch (ex) {
    // alert the user of an error in their expression, and don't close
    Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
              .getService(Components.interfaces.nsIPromptService)
                .alert(window, document.getElementById("strings")
                               .getString("jsObjectExpressionError.title"), ex);
    return false;
  }
  return true;
}
