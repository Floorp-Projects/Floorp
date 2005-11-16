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

var PlacesPage = { 
  _controller: null,
  _tabbrowser: null,
  _topWindow: null,
  _topDocument: null,
  _populators: { },
};

PlacesPage.init = function PP_init() {
  var wm =
      Cc["@mozilla.org/appshell/window-mediator;1"].
      getService(Ci.nsIWindowMediator);
  this._topWindow = wm.getMostRecentWindow("navigator:browser");
  this._topDocument = this._topWindow.document;
  this._tabbrowser = this._topWindow.getBrowser();

  var self = this;
  function onTabSelect(event) {
    self.onTabSelect(event);
  }
  this._tabbrowser.addEventListener("select", onTabSelect, false);

  var placesList = document.getElementById("placesList");
  var placeContent = document.getElementById("placeContent");
  var placesCommands = document.getElementById("placesCommands");
  
  this.controller = 
    new PlacesController([placesList, placeContent], placesCommands);

  const NH = Ci.nsINavHistory;  
  var places = 
      Cc["@mozilla.org/browser/nav-history;1"].getService(NH);
  var query = places.getNewQuery();
  var date = new Date();
  var result = places.executeQuery(query, [NH.GROUP_BY_HOST], 
                                   1, NH.SORT_BY_NONE, false);
  result.QueryInterface(Ci.nsITreeView);
  var placeContent = document.getElementById("placeContent");
  placeContent.view = result;
  
  this._showPlacesUI();
};

PlacesPage.uninit = function PP_uninit() {
  this._hidePlacesUI();
};

PlacesPage.onTabSelect = function PP_onTabSelect(event) {
  var tabURI = this._tabbrowser.selectedBrowser.currentURI.spec;
  (tabURI == PLACES_URI) ? this._showPlacesUI() : this._hidePlacesUI();
};

PlacesPage._showPlacesUI = function PP__showPlacesUI() {
  LOG("SHOW Places UI");
  this._tabbrowser.setAttribute("places", "true");
  var statusbar = this._topDocument.getElementById("status-bar");
  this._oldStatusBarState = statusbar.hidden;
  statusbar.hidden = true;
};

PlacesPage._hidePlacesUI = function PP__hidePlacesUI() {
  LOG("HIDE Places UI");
  this._tabbrowser.removeAttribute("places");
  var statusbar = this._topDocument.getElementById("status-bar");
  statusbar.hidden = this._oldStatusBarState;
};

PlacesPage.setPopulator = 
function PP_setPopulator(uiID, providerType, populator) {
  if (!(uiID in this._populators))
    this._populators[uiID] = { };
  // There can only be one populator per provider type. 
  this._populators[uiID][providerType] = populator;
};

PlacesPage.removePopulator = 
function PP_removePopulator(uiID, providerType) {
  if (!(uiID in this._populators))
    throw Cr.NS_ERROR_INVALID_ARGUMENT;
  delete this._populators[uiID][providerType];
};

PlacesPage.loadQuery = function PP_loadQuery(uri) {
  var placeURI = uri.QueryInterface(Ci.mozIPlaceURI);
  this._updateUI(placeURI.providerType);
};

function CommandBarPopulator() {
  this.element = document.getElementById("commandBar");
  this.newFolder = document.getElementById("commandBar_newFolder");
  this.groupOptions = document.getElementById("commandBar_groupOptions");
  this.groupOption0 = document.getElementById("commandBar_groupOption0");
  this.groupOption1 = document.getElementById("commandBar_groupOption1");
  this.saveSearch = document.getElementById("commandBar_saveSearch");
  
  this.FOLDER_BUTTON = 0x01;
  this.GROUP_OPTIONS = 0x02;
  this.SAVE_SEARCH = 0x04;
    
  function Config(buttons, folderCommand, group0Command, group1Command) {
    this.buttons = buttons;
    this.folderCommand = folderCommand;
    this.group0Command = group0Command;
    this.group1Command = group1Command;
  }
  
  this.config = { 
    bookmark: new Config(self.FOLDER_BUTTON, null, null, null),
    history:  new Config(self.GROUP_OPTIONS, null, "placesCmd_groupby:site",
                         "placesCmd_groupby:page"),
    search:   new Config(self.GROUP_OPTIONS | self.SAVE_SEARCH, null, 
                         "placesCmd_groupby:site", "placesCmd_groupby:page"),
    feed:     new Config(self.GROUP_OPTIONS | self.SAVE_SEARCH | self.FOLDER_BUTTON, 
                         "placesCmd_subscribe", "placesCmd_groupby:feed", 
                         "placesCmd_groupby:post")
  };
}
CommandBarPopulator.prototype.exec = 
function CBP_exec(providerType) {
  var config = this.config[providerType];
  this.newFolder.hidden = !(config.buttons & this.FOLDER_BUTTON);
  this.groupOptions.hidden = !(config.buttons & this.GROUP_OPTIONS);
  this.saveSearch.hidden = !(config.buttons & this.SAVE_SEARCH);
  this.newFolder.command = config.folderCommand;
  this.groupOption0.command = config.group0Command;
  this.groupOption1.command = config.group1Command;
};

