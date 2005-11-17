//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla History System
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
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

// Abstract View Interface:
/*

  get selection() {
  
  },

*/

const PLACES_URI = "chrome://browser/content/places/places.xul";

function LOG(str) {
  dump("*** " + str + "\n");
}

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

/**
 * Implements nsIController, nsICommandController... 
 * @param   elements
 *          A list of elements that this controller applies to
 * @param   commandSet
 *          A XUL <commandset> that contains the commands supported by this 
 *          controller
 * @constructor
 */
function PlacesController(elements, commandSet) {
  if (!commandSet || !elements)
    throw Cr.NS_ERROR_NULL_POINTER;
    
  for (var i = 0; i < elements.length; ++i)
    elements[i].controllers.appendController(this);
}

PlacesController.prototype.isCommandEnabled =
function PC_isCommandEnabled(command) {
  LOG("isCommandEnabled: " + command);
  
};

PlacesController.prototype.supportsCommand = 
function PC_supportsCommand(command) {
  //LOG("supportsCommand: " + command);
  return document.getElementById(command) != null;
};

PlacesController.prototype.doCommand = 
function PC_doCommand(command) {
  LOG("doCommand: " + command);
  
};

PlacesController.prototype.doCommandWithParams = 
function PC_doCommandWithParams(command) {
  LOG("doCommandWithParams: " + command);

};

PlacesController.prototype.getCommandStateWithParams =
function PC_getCommandStateWithParams(command, params) {
  LOG("getCommandStateWithParams: " + command);

};

PlacesController.prototype.onEvent = 
function PC_onEvent(eventName) {
  LOG("onEvent: " + eventName);

};

PlacesController.prototype.QueryInterface = 
function PC_QueryInterface(iid) {
  if (!iid.equals(Ci.nsIController) &&
      !iid.equals(Ci.nsICommandController))
    throw Cr.NS_ERROR_NO_INTERFACE;
  return this;
};

