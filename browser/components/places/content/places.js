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

var gPlaces = {

  controller: null,

  init: function() {
    var wm =
        Cc["@mozilla.org/appshell/window-mediator;1"].
        getService(Ci.nsIWindowMediator);
    var topWindow = wm.getMostRecentWindow("navigator:browser");
    var tabbrowser = topWindow.getBrowser();
    tabbrowser.setAttribute("places", "true");
    
    var statusbar = topWindow.document.getElementById("status-bar");
    statusbar.hidden = true;
    
    var placesList = document.getElementById("placesList");
    var placeContent = document.getElementById("placeContent");
    var placesCommands = document.getElementById("placesCommands");
    
    this.controller = 
      new PlacesController([placesList, placeContent], placesCommands);
  },
  
  get _selectedElement() {
    LOG(document.popupNode.localName);
  },
  
  makeContextMenu: function(popup) {
    var selection = document.popupNode.selection;
    
    
    LOG(document.popupNode.localName);
    LOG(document.commandDispatcher.focusedElement.localName);
    return true;
  },

}
