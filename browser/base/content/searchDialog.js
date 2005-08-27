/* ***** BEGIN LICENSE BLOCK *****
#if 0
   - Version: MPL 1.1/GPL 2.0/LGPL 2.1
   -
   - The contents of this file are subject to the Mozilla Public License Version
   - 1.1 (the "License"); you may not use this file except in compliance with
   - the License. You may obtain a copy of the License at
   - http://www.mozilla.org/MPL/
   -
   - Software distributed under the License is distributed on an "AS IS" basis,
   - WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
   - for the specific language governing rights and limitations under the
   - License.
   -
   - The Original Code is Mozilla Search Dialog.
   -
   - The Initial Developer of the Original Code is
   - Gavin Sharp <gavin@gavinsharp.com>.
   - Portions created by the Initial Developer are Copyright (C) 2005
   - the Initial Developer. All Rights Reserved.
   -
   - Contributor(s):
   -  Pierre Chanial <p_ch@verizon.net>
   -
   - Alternatively, the contents of this file may be used under the terms of
   - either the GNU General Public License Version 2 or later (the "GPL"), or
   - the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
   - in which case the provisions of the GPL or the LGPL are applicable instead
   - of those above. If you wish to allow use of your version of this file only
   - under the terms of either the GPL or the LGPL, and not to allow others to
   - use your version of this file under the terms of the MPL, indicate your
   - decision by deleting the provisions above and replace them with the notice
   - and other provisions required by the LGPL or the GPL. If you do not delete
   - the provisions above, a recipient may use your version of this file under
   - the terms of any one of the MPL, the GPL or the LGPL.
   -
#endif
 * ***** END LICENSE BLOCK ***** */

var gDialog = {};
var gSelectedEngineIndex;

const kTabPref    = "browser.tabs.opentabfor.searchdialog";
const kEnginePref = "browser.search.selectedEngine";
const kDefEnginePref = "browser.search.defaultenginename";
const nsIPLS      = Components.interfaces.nsIPrefLocalizedString;
const kXUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

function onLoad() {
  setTimeout(onAfterLoad, 0);
}

function onAfterLoad() {

  gDialog.list = document.getElementById("searchEngineList");
  gDialog.list.addEventListener("ValueChange", onEnginesListValueChange, false);
  gDialog.input = document.getElementById("searchInput");
  gDialog.newtab = document.getElementById("searchInNewTab");
  gDialog.searchbundle = document.getElementById("searchbarBundle");
  gDialog.browserbundle = document.getElementById("browserBundle");

  var el = document.getAnonymousElementByAttribute(gDialog.input, "anonid",
                                                   "textbox-input-box");
  gDialog.contextmenu = document.getAnonymousElementByAttribute(el,
                                    "anonid", "input-box-contextmenu");

  var pref = Components.classes["@mozilla.org/preferences-service;1"]
                       .getService(Components.interfaces.nsIPrefBranch2);

  var selectedEngine;
  var tabChecked = false;
  try {
    selectedEngine = pref.getComplexValue(kEnginePref, nsIPLS).data;
  } catch (ex) { 
    selectedEngine = pref.getComplexValue(kDefEnginePref, nsIPLS).data;
  }
  try {
    tabChecked = pref.getBoolPref(kTabPref);
  } catch (ex) {  }

  gDialog.newtab.setAttribute("checked", tabChecked);

  for (var i=0; i < gDialog.list.menupopup.childNodes.length; i++) {
    var label = gDialog.list.menupopup.childNodes[i].getAttribute("label");
    if (label == selectedEngine)
      gDialog.list.selectedIndex = i;
  }

  // Make sure we have a selected item
  if (gDialog.list.selectedIndex == -1)
    gDialog.list.selectedIndex = 0;

  gSelectedEngineIndex = gDialog.list.selectedIndex;

  // Add "Clear Search History" item to the text box context menu
  var sep = document.createElementNS(kXUL_NS, "menuseparator");
  gDialog.contextmenu.appendChild(sep);

  var element = document.createElementNS(kXUL_NS, "menuitem");

  var label = gDialog.searchbundle.getString("cmd_clearHistory");
  var accesskey = gDialog.searchbundle.getString("cmd_clearHistory_accesskey");
  element.setAttribute("label", label);
  element.setAttribute("accesskey", accesskey);
  element.setAttribute("cmd", "cmd_clearhistory");

  gDialog.contextmenu.appendChild(element);
  gDialog.input.controllers.appendController(clearHistoryController);
}

function onEnginesListValueChange() {
  if (gDialog.list.value == "addengines") {
    var url = gDialog.browserbundle.getString("searchEnginesURL");
    window.opener.openNewWindowWith(url, null, false);
    gDialog.list.selectedIndex = gSelectedEngineIndex;
  } else {
    gSelectedEngineIndex = gDialog.list.selectedIndex;
  }
}

function updateAddEnginesItem(aAdd) {
  // Remove the old item if it exists
  var elem = document.getElementById("addenginemenuitem");
  var sep = document.getElementById("addenginesep");
  if (elem)
    gDialog.list.menupopup.removeChild(elem);
  if (sep)
    gDialog.list.menupopup.removeChild(sep);
  
  if (aAdd) {
    // Add "Add Engines" item to the drop down
    var sep = document.createElementNS(kXUL_NS, "menuseparator");
    sep.setAttribute("id", "addenginesep");
    gDialog.list.menupopup.appendChild(sep);
  
    var label = gDialog.searchbundle.getString("cmd_addEngine");
    var newItem = gDialog.list.appendItem(label, "addengines");
    newItem.setAttribute("id", "addenginemenuitem");
    newItem.setAttribute("class", "menuitem-iconic engine-icon");
  }
}

function onDialogAccept() {
  var searchText = gDialog.input.value;
  var engine = gDialog.list.value;
  var searchURL;

  var searchSvc =
      Components.classes["@mozilla.org/rdf/datasource;1?name=internetsearch"]
                .getService(Components.interfaces.nsIInternetSearchService);

  // XXX Bug 269994: Use dummy string if there is no user entered string
  searchURL = searchSvc.GetInternetSearchURL(engine,
                             searchText ? encodeURIComponent(searchText):"A",
                             0, 0, {value:0});

  if (searchText) {
    // Add item to form history
    var frmHistSvc = Components.classes["@mozilla.org/satchel/form-history;1"]
                               .getService(Components.interfaces.nsIFormHistory);
    frmHistSvc.addEntry("searchbar-history", searchText);
  } else {
    try {
      // Get the engine's base URL
      searchURL = makeURI(searchURL).host;
    } catch (ex) {}
  }

  setPrefs();
  gDialog.input.controllers.removeController(clearHistoryController);

  window.opener.delayedSearchLoadURL(searchURL, gDialog.newtab.checked);

  // Delay closing slightly to avoid timing bug on Linux.
  window.close();
  return false;
}

function setPrefs() {
  var pref = Components.classes["@mozilla.org/preferences-service;1"]
                       .getService(Components.interfaces.nsIPrefBranch2);

  var pls = Components.classes["@mozilla.org/pref-localizedstring;1"]
                      .createInstance(nsIPLS);

  var name = gDialog.list.selectedItem.getAttribute("label");
  pls.data = name;

  pref.setComplexValue(kEnginePref, nsIPLS, pls);
  pref.setBoolPref(kTabPref, gDialog.newtab.checked);
}

var clearHistoryController = {
  frmHistSvc: Components.classes["@mozilla.org/satchel/form-history;1"]
                        .getService(Components.interfaces.nsIFormHistory),

  autocompleteSearchParam: "searchbar-history",

  supportsCommand: function (aCommand) {
    return aCommand == "cmd_clearhistory";
  },

  isCommandEnabled: function (aCommand) {
    return this.frmHistSvc.nameExists(this.autocompleteSearchParam);
  },
  
  doCommand: function (aCommand) {
    this.frmHistSvc.removeEntriesForName(this.autocompleteSearchParam);
    gDialog.input.value = "";
  }
}
