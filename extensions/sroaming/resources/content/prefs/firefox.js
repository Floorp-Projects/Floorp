/*-*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
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
 * The Original Code is Mozilla Roaming code.
 *
 * The Initial Developer of the Original Code is 
 *   Ben Bucksch <http://www.bucksch.org> of
 *   Beonex <http://www.beonex.com>
 * Portions created by the Initial Developer are Copyright (C) 2002-2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger
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

// For compatibility with Communicator prefs window

//var hPrefWindow = new nsPrefWindow();

function nsPrefWindow()
{
  this.okHandlers     = new Array();
  this.cancelHandlers = new Array();
}
nsPrefWindow.prototype =
{
  onOK : function()
  {
    for(var i = 0; i < hPrefWindow.okHandlers.length; i++)
      try
      {
        hPrefWindow.okHandlers[i]();
      } catch (e) {}
  },
  onCancel : function()
  {
    for(var i = 0; i < hPrefWindow.cancelHandlers.length; i++)
      try
      {
        hPrefWindow.cancelHandlers[i]();
      } catch (e) {}
  },
  registerOKCallbackFunc : function(aFunction)
  {
    this.okHandlers.push(aFunction);
  },
  registerCancelCallbackFunc : function(aFunction)
  {
    this.cancelHandlers.push(aFunction);
  }
};



// dummy
function initPanel(aPrefTag)
{
} 
