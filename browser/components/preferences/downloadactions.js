# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Download Actions Manager.
#
# The Initial Developer of the Original Code is
# Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

const kPluginHandlerContractID = "@mozilla.org/content/plugin/document-loader-factory;1";
const kDisabledPluginTypesPref = "plugin.disable_full_page_plugin_for_types";
const kShowPluginsInList = "browser.download.show_plugins_in_list";
const kHideTypesWithoutExtensions = "browser.download.hide_plugins_without_extensions";
const kRootTypePrefix = "urn:mimetype:";

///////////////////////////////////////////////////////////////////////////////
// MIME Types Datasource RDF Utils
function NC_URI(aProperty)
{
  return "http://home.netscape.com/NC-rdf#" + aProperty;
}

function MIME_URI(aType)
{
  return "urn:mimetype:" + aType;
}

function HANDLER_URI(aHandler)
{
  return "urn:mimetype:handler:" + aHandler;
}

function APP_URI(aType)
{
  return "urn:mimetype:externalApplication:" + aType;
}

var gDownloadActionsWindow = {  
  _tree         : null,
  _editButton   : null,
  _removeButton : null,
  _actions      : [],
  _plugins      : {},
  _bundle       : null,
  _pref         : Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefBranch),
  _mimeSvc      : Components.classes["@mozilla.org/uriloader/external-helper-app-service;1"]
                            .getService(Components.interfaces.nsIMIMEService),
  _excludingPlugins           : false,
  _excludingMissingExtensions : false,
  
  init: function ()
  {
    (this._editButton = document.getElementById("editFileHandler")).disabled = true;
    (this._removeButton = document.getElementById("removeFileHandler")).disabled = true;    

    if (this._pref instanceof Components.interfaces.nsIPrefBranchInternal) {
      this._pref.addObserver(kShowPluginsInList, this, false);
      this._pref.addObserver(kHideTypesWithoutExtensions, this, false);
    }
    
    // Initialize the File Type list
    this._bundle = document.getElementById("bundlePreferences");
    this._tree = document.getElementById("fileHandlersList");
    this._loadView();
    // Determine any exclusions being applied - e.g. don't show types for which
    // only a plugin handler exists, don't show types lacking extensions, etc. 
    this._view._rowCount = this._updateExclusions();    
    this._tree.treeBoxObject.view = this._view;  

    var indexToSelect = parseInt(this._tree.getAttribute("lastSelected"));
    if (indexToSelect < this._tree.view.rowCount)
      this._tree.view.selection.select(indexToSelect);
    this._tree.focus();    
  },
  
  _loadView: function ()
  {
    // Reset ALL the collections and state flags, because we can call this after
    // the window has initially displayed by resetting the filter. 
    this._actions = [];
    this._plugins = {};
    this._view._filtered = false;
    this._view._filterSet = [];
    this._view._usingExclusionSet = false;
    this._view._exclusionSet = [];
    this._view._filterValue = "";

    this._loadPluginData();
    this._loadMIMERegistryData();
  },
  
  _updateRowCount: function (aNewRowCount)
  {
    var oldCount = this._view._rowCount;
    this._view._rowCount = 0;
    this._tree.treeBoxObject.rowCountChanged(0, -oldCount);
    this._view._rowCount = aNewRowCount;
    this._tree.treeBoxObject.rowCountChanged(0, aNewRowCount);
  },
  
  uninit: function ()
  {
    if (this._pref instanceof Components.interfaces.nsIPrefBranchInternal) {
      this._pref.removeObserver(kShowPluginsInList, this);
      this._pref.removeObserver(kHideTypesWithoutExtensions, this);
    }
  },
  
  observe: function (aSubject, aTopic, aData)
  {
    if (aTopic == "nsPref:changed" &&
        (aData == kShowPluginsInList || aData == kHideTypesWithoutExtensions))
      this._updateRowCount(this._updateExclusions());
  },
  
  _updateExclusions: function ()
  {
    this._excludingPlugins = !this._pref.getBoolPref(kShowPluginsInList);
    this._excludingMissingExtensions = this._pref.getBoolPref(kHideTypesWithoutExtensions);    
    this._view._exclusionSet = [].concat(this._actions);
    if (this._excludingMissingExtensions) {
      this._view._usingExclusionSet = true;
      for (var i = 0; i < this._view._exclusionSet.length;) {
        if (!this._view._exclusionSet[i].hasExtension)
          this._view._exclusionSet.splice(i, 1);
        else
          ++i;
      }
    }
    if (this._excludingPlugins) {
      this._view._usingExclusionSet = true;
      for (i = 0; i < this._view._exclusionSet.length;) {
        if (this._view._exclusionSet[i].handledOnlyByPlugin)
          this._view._exclusionSet.splice(i, 1);
        else
          ++i        
      }      
    }

    return this._view._usingExclusionSet ? this._view._exclusionSet.length 
                                         : this._view._filtered ? this._view._filterSet.length 
                                                                : this._actions.length;
  },
  
  _loadPluginData: function ()
  {
    // Read enabled plugin type information from the category manager
    var disabled = "";
    if (this._pref.prefHasUserValue(kDisabledPluginTypesPref)) 
      disabled = this._pref.getCharPref(kDisabledPluginTypesPref);
    
    for (var i = 0; i < navigator.plugins.length; ++i) {
      var plugin = navigator.plugins[i];
      for (var j = 0; j < plugin.length; ++j) {
        var actionName = this._bundle.getFormattedString("openWith", [plugin.name])
        var type = plugin[j].type;
        this._createAction(type, actionName, true, FILEACTION_OPEN_PLUGIN, 
                           null, true, disabled.indexOf(type) == -1, true);
      }
    }
  },

  _createAction: function (aMIMEType, aActionName, 
                           aIsEditable, aHandleMode, aCustomHandler,
                           aPluginAvailable, aPluginEnabled, 
                           aHandledOnlyByPlugin)
  {
    var newAction = !(aMIMEType in this._plugins);
    var action = newAction ? new FileAction() : this._plugins[aMIMEType];
    action.type = aMIMEType;
    var info = this._mimeSvc.getFromTypeAndExtension(action.type, null);
    
    // File Extension
    try {
      action.extension = info.primaryExtension;
    }
    catch (e) {
      action.extension = this._bundle.getString("extensionNone");
      action.hasExtension = false;
    }
    
    // Large and Small Icon
    try {
      action.smallIcon = "moz-icon://goat." + info.primaryExtension + "?size=16";
      action.bigIcon = "moz-icon://goat." + info.primaryExtension + "?size=32";
    }
    catch (e) {
      action.smallIcon = "moz-icon://goat?size=16&contentType=" + info.MIMEType;
      action.bigIcon = "moz-icon://goat?contentType=" + info.MIMEType + "&size=32";
    }

    // Pretty Type Name
    if (info.description == "") {
      try {
        action.typeName = this._bundle.getFormattedString("fileEnding", [info.primaryExtension.toUpperCase()]);
      }
      catch (e) { 
        // Wow, this sucks, just show the MIME type as a last ditch effort to display
        // the type of file that this is. 
        action.typeName = info.MIMEType;
      }
    }
    else
      action.typeName = info.description;

    // Pretty Action Name
    if (aActionName)
      action.action         = aActionName;
    action.pluginAvailable  = aPluginAvailable;
    action.pluginEnabled    = aPluginEnabled;
    action.editable         = aIsEditable;
    action.handleMode       = aHandleMode;
    action.customHandler    = aCustomHandler;
    action.mimeInfo         = info;
    action.handledOnlyByPlugin  = aHandledOnlyByPlugin
    
    if (newAction && !(action.handledOnlyByPlugin && !action.pluginEnabled)) {
      this._actions.push(action);
      this._plugins[action.type] = action;
    }      
    return action;
  },
  
  _loadMIMEDS: function ()
  {
    var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"]
                                .getService(Components.interfaces.nsIProperties);
    
    var file = fileLocator.get("UMimTyp", Components.interfaces.nsIFile);

    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
    var fileHandler = ioService.getProtocolHandler("file")
                               .QueryInterface(Components.interfaces.nsIFileProtocolHandler);
    this._mimeDS = this._rdf.GetDataSourceBlocking(fileHandler.getURLSpecFromFile(file));
  },
  
  _getLiteralValue: function (aResource, aProperty)
  {
    var property = this._rdf.GetResource(NC_URI(aProperty));
    var value = this._mimeDS.GetTarget(aResource, property, true);
    if (value)
      return value.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    return "";
  },
  
  _getChildResource: function (aResource, aProperty)
  {
    var property = this._rdf.GetResource(NC_URI(aProperty));
    return this._mimeDS.GetTarget(aResource, property, true);
  },
  
  _getDisplayNameForFile: function (aFile)
  {
#ifdef XP_WIN
    if (aFile instanceof Components.interfaces.nsILocalFileWin)
      return aFile.getVersionInfoField("FileDescription"); 
#else
    // XXXben - Read the bundle name on OS X.
    var ios = Components.classes["@mozilla.org/network/io-service;1"]
                        .getService(Components.interfaces.nsIIOService);
    var url = ios.newFileURI(aFile).QueryInterface(Components.interfaces.nsIURL);
    return url.fileName;
#endif
  },  
  
  _loadMIMERegistryData: function ()
  {
    this._rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                          .getService(Components.interfaces.nsIRDFService);
    this._loadMIMEDS();                          
                          
    var root = this._rdf.GetResource("urn:mimetypes:root");
    var container = Components.classes["@mozilla.org/rdf/container;1"]
                              .createInstance(Components.interfaces.nsIRDFContainer);
    container.Init(this._mimeDS, root);
    
    var elements = container.GetElements();
    while (elements.hasMoreElements()) {
      var type = elements.getNext();
      if (!(type instanceof Components.interfaces.nsIRDFResource))
        break;
      var editable = this._getLiteralValue(type, "editable") == "true";
      if (!editable)
        continue;
      
      var handler = this._getChildResource(type, "handlerProp");
      var alwaysAsk = this._getLiteralValue(handler, "alwaysAsk") == "true";
      if (alwaysAsk)
        continue;
      var saveToDisk        = this._getLiteralValue(handler, "saveToDisk") == "true";
      var useSystemDefault  = this._getLiteralValue(handler, "useSystemDefault") == "true";
      var editable          = this._getLiteralValue(type, "editable") == "true";
      var handledInternally = this._getLiteralValue(handler, "handleInternal") == "true";
      var externalApp       = this._getChildResource(handler, "externalApplication");
      var externalAppPath   = this._getLiteralValue(externalApp, "path");
      try {
        var customHandler = Components.classes["@mozilla.org/file/local;1"]
                                      .createInstance(Components.interfaces.nsILocalFile);
        customHandler.initWithPath(externalAppPath);
      }
      catch (e) {
        customHandler = null;
      }      
      var mimeType = this._getLiteralValue(type, "value");
      var typeInfo = this._mimeSvc.getFromTypeAndExtension(mimeType, null);

      // Determine the pretty name of the associated action.
      var actionName = "";
      var handleMode = 0;
      if (saveToDisk) {
        // Save the file to disk
        actionName = this._bundle.getString("saveToDisk");
        handleMode = FILEACTION_SAVE_TO_DISK;
      }
      else if (useSystemDefault) {
        // Use the System Default handler
        actionName = this._bundle.getFormattedString("openWith", 
                                                     [typeInfo.defaultDescription]);
        handleMode = FILEACTION_OPEN_DEFAULT;
      }
      else {
        // Custom Handler
        if (customHandler) {
          actionName = this._bundle.getFormattedString("openWith", 
                                                       [this._getDisplayNameForFile(customHandler)]);
          handleMode = FILEACTION_OPEN_CUSTOM;
        }
        else {
          // Corrupt datasource, invalid custom handler path. Revert to default.
          actionName = this._bundle.getFormattedString("openWith", 
                                                       [typeInfo.defaultDescription]);
          handleMode = FILEACTION_OPEN_DEFAULT;
        }
      }

      if (handledInternally)
        handleMode = FILEACTION_OPEN_INTERNALLY;
      
      var pluginAvailable = mimeType in this._plugins && this._plugins[mimeType].pluginAvailable;
      var pluginEnabled = pluginAvailable && this._plugins[mimeType].pluginEnabled;
      if (pluginEnabled) {
        handleMode = FILEACTION_OPEN_PLUGIN;
        actionName = null;
      }
      var action = this._createAction(mimeType, actionName, editable, handleMode, 
                                      customHandler, pluginAvailable, pluginEnabled,
                                      false);
    }
  },
  
  _view: {
    _filtered           : false,
    _filterSet          : [],
    _usingExclusionSet  : false,
    _exclusionSet       : [],
    _filterValue        : "",

    _rowCount: 0,
    get rowCount() 
    { 
      return this._rowCount; 
    },
    
    get activeCollection ()
    {
      return this._filtered ? this._filterSet 
                            : this._usingExclusionSet ? this._exclusionSet 
                                                      : gDownloadActionsWindow._actions;
    },

    getItemAtIndex: function (aIndex)
    {
      return this.activeCollection[aIndex];
    },
    
    getCellText: function (aIndex, aColumn)
    {
      switch (aColumn.id) {
      case "fileExtension":
        return this.getItemAtIndex(aIndex).extension.toUpperCase();
      case "fileType":
        return this.getItemAtIndex(aIndex).typeName;
      case "fileMIMEType":
        return this.getItemAtIndex(aIndex).type;
      case "fileHandler":
        return this.getItemAtIndex(aIndex).action;
      }
      return "";
    },
    getImageSrc: function (aIndex, aColumn) 
    {
      if (aColumn.id == "fileExtension") 
        return this.getItemAtIndex(aIndex).smallIcon;
      return "";
    },
    _selection: null, 
    get selection () { return this._selection; },
    set selection (val) { this._selection = val; return val; },
    getRowProperties: function (aIndex, aProperties) {},
    getCellProperties: function (aIndex, aColumn, aProperties) {},
    getColumnProperties: function (aColumn, aProperties) {},
    isContainer: function (aIndex) { return false; },
    isContainerOpen: function (aIndex) { return false; },
    isContainerEmpty: function (aIndex) { return false; },
    isSeparator: function (aIndex) { return false; },
    isSorted: function (aIndex) { return false; },
    canDrop: function (aIndex, aOrientation) { return false; },
    drop: function (aIndex, aOrientation) {},
    getParentIndex: function (aIndex) { return -1; },
    hasNextSibling: function (aParentIndex, aIndex) { return false; },
    getLevel: function (aIndex) { return 0; },
    getProgressMode: function (aIndex, aColumn) {},    
    getCellValue: function (aIndex, aColumn) {},
    setTree: function (aTree) {},    
    toggleOpenState: function (aIndex) { },
    cycleHeader: function (aColumn) {},    
    selectionChanged: function () {},    
    cycleCell: function (aIndex, aColumn) {},    
    isEditable: function (aIndex, aColumn) { return false; },
    setCellValue: function (aIndex, aColumn, aValue) {},    
    setCellText: function (aIndex, aColumn, aValue) {},    
    performAction: function (aAction) {},  
    performActionOnRow: function (aAction, aIndex) {},    
    performActionOnCell: function (aAction, aindex, aColumn) {}
  },

  removeFileHandler: function ()
  {
    var selection = this._tree.view.selection; 
    if (selection.count < 1)
      return;
      
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                  .getService(Components.interfaces.nsIPromptService);
    var flags = promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0;
    flags += promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1;

    var title = this._bundle.getString("removeTitle" + (selection.count > 1 ? "Multiple" : "Single"));
    var message = this._bundle.getString("removeMessage" + (selection.count > 1 ? "Multiple" : "Single"));
    var button = this._bundle.getString("removeButton" + (selection.count > 1 ? "Multiple" : "Single"));
    var rv = promptService.confirmEx(window, title, message, flags, button, 
                                     null, null, null, { value: 0 });
    if (rv != 0)
      return;     

    var rangeCount = selection.getRangeCount();
    var lastSelected = 0;
    var mimeDSDirty = false;
    for (var i = 0; i < rangeCount; ++i) {
      var min = { }; var max = { };
      selection.getRangeAt(i, min, max);
      for (var j = min.value; j <= max.value; ++j) {
        var item = this._view.getItemAtIndex(j);
        if (!item.handledOnlyByPlugin) {
          // There is data for this type in the MIME registry, so make sure we
          // remove it from the MIME registry. We don't disable the plugin here because
          // if we do there's currently no way through the UI to re-enable it. We may
          // come up with some sort of solution for that at a later date. 
          var typeRes = this._rdf.GetResource(MIME_URI(item.type));
          var handlerRes = this._getChildResource(typeRes, "handlerProp");
          var extAppRes = this._getChildResource(handlerRes, "externalApplication");
          this._cleanResource(extAppRes);
          this._cleanResource(handlerRes);
          this._cleanResource(typeRes); 
          mimeDSDirty = true;         
        }
        lastSelected = (j + 1) >= this._view.rowCount ? j-1 : j;
      }
    }
    if (mimeDSDirty && 
        this._mimeDS instanceof Components.interfaces.nsIRDFRemoteDataSource)
      this._mimeDS.Flush();
    
    // Just reload the list to make sure deletions are respected
    this._loadView();
    this._updateRowCount(this._updateExclusions());

    selection.select(lastSelected);
  },
  
  _cleanResource: function (aResource)
  {
    var labels = this._mimeDS.ArcLabelsOut(aResource);
    while (labels.hasMoreElements()) {
      var arc = labels.getNext();
      if (!(arc instanceof Components.interfaces.nsIRDFResource))
        break;
      var target = this._mimeDS.GetTarget(aResource, arc, true);
      this._mimeDS.Unassert(aResource, arc, target);
    }
  },
  
  _disablePluginForItem: function (aItem)
  {
    if (aItem.pluginAvailable) {
      // Since we're disabling the full page plugin for this content type, 
      // we must add it to the disabled list if it's not in there already.
      var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefBranch);
      var disabled = aItem.type;
      if (prefs.prefHasUserValue(kDisabledPluginTypesPref)) {
        disabled = prefs.getCharPref(kDisabledPluginTypesPref);
        if (disabled.indexOf(aItem.type) == -1) 
          disabled += "," + aItem.type;
      }
      prefs.setCharPref(kDisabledPluginTypesPref, disabled);   
      
      // Also, we update the category manager so that existing browser windows
      // update.
      var catman = Components.classes["@mozilla.org/categorymanager;1"]
                             .getService(Components.interfaces.nsICategoryManager);
      catman.deleteCategoryEntry("Gecko-Content-Viewers", aItem.type, false);     
    }    
  },
  
  _enablePluginForItem: function (aItem)
  {
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch);
    // Since we're enabling the full page plugin for this content type, we must
    // look at the disabled types list and ensure that this type isn't in it.
    if (prefs.prefHasUserValue(kDisabledPluginTypesPref)) {
      var disabledList = prefs.getCharPref(kDisabledPluginTypesPref);
      if (disabledList == aItem.type)
        prefs.clearUserPref(kDisabledPluginTypesPref);
      else {
        var disabledTypes = disabledList.split(",");
        var disabled = "";
        for (var i = 0; i < disabledTypes.length; ++i) {
          if (aItem.type != disabledTypes[i])
            disabled += disabledTypes[i] + (i == disabledTypes.length - 1 ? "" : ",");
        }
        prefs.setCharPref(kDisabledPluginTypesPref, disabled);
      }
    }

    // Also, we update the category manager so that existing browser windows
    // update.
    var catman = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(Components.interfaces.nsICategoryManager);
    catman.addCategoryEntry("Gecko-Content-Viewers", aItem.type,
                            kPluginHandlerContractID, false, true);
  },
  
  _ensureMIMERegistryEntry: function (aItem)
  {
    var root = this._rdf.GetResource("urn:mimetypes:root");
    var container = Components.classes["@mozilla.org/rdf/container;1"]
                              .createInstance(Components.interfaces.nsIRDFContainer);
    container.Init(this._mimeDS, root);
    
    var itemResource = this._rdf.GetResource(MIME_URI(aItem.type));
    var handlerResource = null;
    if (container.IndexOf(itemResource) == -1) {
      container.AppendElement(itemResource);
      this._setLiteralValue(itemResource, "editable", "true");
      this._setLiteralValue(itemResource, "value", aItem.type);
      
      handlerResource = this._rdf.GetResource(HANDLER_URI(aItem.type));
      this._setLiteralValue(handlerResource, "alwaysAsk", "false");
      var handlerProp = this._rdf.GetResource(NC_URI("handlerProp"));
      this._mimeDS.Assert(itemResource, handlerProp, handlerResource, true);
      
      var extAppResource = this._rdf.GetResource(APP_URI(aItem.type));
      this._setLiteralValue(extAppResource, "path", "");
      var extAppProp = this._rdf.GetResource(NC_URI("externalApplication"));
      this._mimeDS.Assert(handlerResource, extAppProp, extAppResource, true);
    }
    else
      handlerResource = this._getChildResource(itemResource, "handlerProp");
        
    return handlerResource;
  },
  
  _setLiteralValue: function (aResource, aProperty, aValue)
  {
    var property = this._rdf.GetResource(NC_URI(aProperty));
    var newValue = this._rdf.GetLiteral(aValue);
    var oldValue = this._mimeDS.GetTarget(aResource, property, true);
    if (oldValue)
      this._mimeDS.Change(aResource, property, oldValue, newValue);
    else
      this._mimeDS.Assert(aResource, property, newValue, true);
  },
  
  editFileHandler: function ()
  {
    var selection = this._tree.view.selection; 
    if (selection.count != 1)
      return;

    var item = this._view.getItemAtIndex(selection.currentIndex);
    openDialog("chrome://browser/content/preferences/changeaction.xul", 
               "_blank", "modal,centerscreen", item);
    
    // Update the database
    switch (item.handleMode) {
    case FILEACTION_OPEN_PLUGIN:
      this._enablePluginForItem(item);
      // We don't need to adjust the database because plugin settings always
      // supercede whatever is in the db, leaving it untouched allows the last
      // user setting(s) to be preserved if they ever revert.
      break;
    case FILEACTION_OPEN_DEFAULT:
      this._disablePluginForItem(item);
      var handlerRes = this._ensureMIMERegistryEntry(item);
      this._setLiteralValue(handlerRes, "useSystemDefault", "true");
      this._setLiteralValue(handlerRes, "saveToDisk", "false");
      break;
    case FILEACTION_OPEN_CUSTOM:
      this._disablePluginForItem(item);
      var handlerRes = this._ensureMIMERegistryEntry(item);
      this._setLiteralValue(handlerRes, "useSystemDefault", "false");
      this._setLiteralValue(handlerRes, "saveToDisk", "false");
      var extAppRes = this._getChildResource(handlerRes, "externalApplication");
      this._setLiteralValue(extAppRes, "path", item.customHandler.path);
      break;
    case FILEACTION_SAVE_TO_DISK:
      this._disablePluginForItem(item);
      var handlerRes = this._ensureMIMERegistryEntry(item);
      this._setLiteralValue(handlerRes, "useSystemDefault", "false");
      this._setLiteralValue(handlerRes, "saveToDisk", "true");
      break;
    }
    
    if (this._mimeDS instanceof Components.interfaces.nsIRDFRemoteDataSource)
      this._mimeDS.Flush();
    
    // Update the view
    this._tree.treeBoxObject.invalidateRow(selection.currentIndex);    
  },
  
  onSelectionChanged: function ()
  {
    if (this._tree.view.rowCount == 0) {
      this._removeButton.disabled = true;
      this._editButton.disabled = true;
      return;
    }
      
    var selection = this._tree.view.selection; 
    var selected = selection.count;
    this._removeButton.disabled = selected == 0;
    this._editButton.disabled = selected != 1;
    var stringKey = selected > 1 ? "removeButtonMultiple" : "removeButtonSingle";
    this._removeButton.label = this._bundle.getString(stringKey);
    
    var canRemove = true;
    var canEdit = true;
    
    var rangeCount = selection.getRangeCount();
    var min = { }, max = { };
    var setLastSelected = false;
    for (var i = 0; i < rangeCount; ++i) {
      selection.getRangeAt(i, min, max);
      
      for (var j = min.value; j <= max.value; ++j) {
        if (!setLastSelected) {
          // Set the last selected index to the first item in the selection
          this._tree.setAttribute("lastSelected", j);
          setLastSelected = true;
        }

        var item = this._view.getItemAtIndex(j);
        if (item && 
            (!item.editable || item.handleMode == FILEACTION_OPEN_INTERNALLY))
          canEdit = false;
        
        if (item && 
            (!item.editable || item.handleMode == FILEACTION_OPEN_INTERNALLY ||
             item.handledOnlyByPlugin))
          canRemove = false;
      }
    }
    
    if (!canRemove)
      this._removeButton.disabled = true;
    if (!canEdit)
      this._editButton.disabled = true;
  },
  
  _lastSortProperty : "",
  _lastSortAscending: false,
  sort: function (aProperty) 
  {
    var ascending = (aProperty == this._lastSortProperty) ? !this._lastSortAscending : true;
    function sortByProperty(a, b) 
    {
      return a[aProperty].toLowerCase().localeCompare(b[aProperty].toLowerCase());
    }
    function sortByExtension(a, b)
    {
      if (!a.hasExtension && b.hasExtension)
        return 1;
      if (!b.hasExtension && a.hasExtension)
        return -1;
      return a.extension.toLowerCase().localeCompare(b.extension.toLowerCase());
    }
    // Sort the Filtered List, if in Filtered mode
    if (!this._view._filtered) { 
      this._view.activeCollection.sort(aProperty == "extension" ? sortByExtension : sortByProperty);
      if (!ascending)
        this._view.activeCollection.reverse();
    }

    this._view.selection.clearSelection();
    this._view.selection.select(0);
    this._tree.treeBoxObject.invalidate();
    this._tree.treeBoxObject.ensureRowIsVisible(0);

    this._lastSortAscending = ascending;
    this._lastSortProperty = aProperty;
  },
  
  clearFilter: function ()
  {    
    // Clear the Filter and the Tree Display
    document.getElementById("filter").value = "";
    this._view._filtered = false;
    this._view._filterSet = [];

    // Just reload the list to make sure deletions are respected
    this._loadView();
    this._updateRowCount(this._updateExclusions());

    // Restore selection
    this._view.selection.clearSelection();
    for (var i = 0; i < this._lastSelectedRanges.length; ++i) {
      var range = this._lastSelectedRanges[i];
      this._view.selection.rangedSelect(range.min, range.max, true);
    }
    this._lastSelectedRanges = [];

    document.getElementById("actionsIntro").value = this._bundle.getString("actionsAll");
    document.getElementById("clearFilter").disabled = true;
  },
  
  _actionMatchesFilter: function (aAction)
  {
    return aAction.extension.toLowerCase().indexOf(this._view._filterValue) != -1 ||
           aAction.typeName.toLowerCase().indexOf(this._view._filterValue) != -1 || 
           aAction.type.toLowerCase().indexOf(this._view._filterValue) != -1 ||
           aAction.action.toLowerCase().indexOf(this._view._filterValue) != -1;
  },
  
  _filterActions: function (aFilterValue)
  {
    this._view._filterValue = aFilterValue;
    var actions = [];
    var collection = this._view._usingExclusionSet ? this._view._exclusionSet : this._actions;
    for (var i = 0; i < collection.length; ++i) {
      var action = collection[i];
      if (this._actionMatchesFilter(action)) 
        actions.push(action);
    }
    return actions;
  },
  
  _lastSelectedRanges: [],
  _saveState: function ()
  {
    // Save selection
    var seln = this._view.selection;
    this._lastSelectedRanges = [];
    var rangeCount = seln.getRangeCount();
    for (var i = 0; i < rangeCount; ++i) {
      var min = {}; var max = {};
      seln.getRangeAt(i, min, max);
      this._lastSelectedRanges.push({ min: min.value, max: max.value });
    }
  },
  
  _filterTimeout: -1,
  onFilterInput: function ()
  {
    if (this._filterTimeout != -1)
      clearTimeout(this._filterTimeout);
   
    function filterActions()
    {
      var filter = document.getElementById("filter").value;
      if (filter == "") {
        gDownloadActionsWindow.clearFilter();
        return;
      }        
      var view = gDownloadActionsWindow._view;
      view._filterSet = gDownloadActionsWindow._filterActions(filter);
      if (!view._filtered) {
        // Save Display Info for the Non-Filtered mode when we first
        // enter Filtered mode. 
        gDownloadActionsWindow._saveState();
        view._filtered = true;
      }

      // Clear the display
      gDownloadActionsWindow._updateRowCount(view._filterSet.length);
      
      // if the view is not empty then select the first item
      if (view.rowCount > 0)
        view.selection.select(0);

      document.getElementById("actionsIntro").value = gDownloadActionsWindow._bundle.getString("actionsFiltered");
      document.getElementById("clearFilter").disabled = false;
    }
    window.filterActions = filterActions;
    this._filterTimeout = setTimeout("filterActions();", 500);
  },
  
  onFilterKeyPress: function (aEvent)
  {
    if (aEvent.keyCode == 27) // ESC key
      this.clearFilter();
  },
  
  focusFilterBox: function ()
  { 
    var filter = document.getElementById("filter");
    filter.focus();
    filter.select();
  }  
};

