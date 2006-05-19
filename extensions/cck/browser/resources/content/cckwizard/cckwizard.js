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
 * The Original Code is the Client Customization Kit (CCK).
 *
 * The Initial Developer of the Original Code is IBM Corp.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

var currentconfigname;
var currentconfigpath;
var configarray = new Array();

const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
var gPrefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(nsIPrefBranch);
                            
var gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                               .getService(Components.interfaces.nsIPromptService);

function choosefile(labelname)
{
  try {
    var nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    var bundle = document.getElementById("bundle_cckwizard");
    fp.init(window, bundle.getString("chooseFile"), nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterAll);

   if (fp.show() == nsIFilePicker.returnOK && fp.fileURL.spec && fp.fileURL.spec.length > 0) {
     var label = document.getElementById(labelname);
     label.value = fp.file.path;
   }
  }
  catch(ex) {
  }
}

function choosedir(labelname)
{
  try {
    var keepgoing = true;
    while (keepgoing) {
      var nsIFilePicker = Components.interfaces.nsIFilePicker;
      var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
      var bundle = document.getElementById("bundle_cckwizard");
      fp.init(window, bundle.getString("chooseDirectory"), nsIFilePicker.modeGetFolder);
      fp.appendFilters(nsIFilePicker.filterHTML | nsIFilePicker.filterText |
                       nsIFilePicker.filterAll | nsIFilePicker.filterImages | nsIFilePicker.filterXML);

      if (fp.show() == nsIFilePicker.returnOK && fp.fileURL.spec && fp.fileURL.spec.length > 0) {
        var label = document.getElementById(labelname);
        label.value = fp.file.path;
      }
      keepgoing = false;
    }
  }
  catch(ex) {
  }
}

function chooseimage(labelname, imagename)
{
  try {
    var nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    var bundle = document.getElementById("bundle_cckwizard");
    fp.init(window, bundle.getString("chooseImage"), nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterImages);

   if (fp.show() == nsIFilePicker.returnOK && fp.fileURL.spec && fp.fileURL.spec.length > 0) {
     var label = document.getElementById(labelname);
     label.value = fp.file.path;
     document.getElementById(imagename).src = fp.fileURL.spec;
   }
  }
  catch(ex) {
  }
}

function initimage(labelname, imagename)
{
  var sourcefile = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  try {
    sourcefile.initWithPath(document.getElementById(labelname).value);
    var ioServ = Components.classes["@mozilla.org/network/io-service;1"]
                           .getService(Components.interfaces.nsIIOService);
    var foo = ioServ.newFileURI(sourcefile);
    document.getElementById(imagename).src = foo.spec;
  } catch (e) {
    document.getElementById(imagename).src = '';
  }
}



function CreateConfig()
{
  window.openDialog("chrome://cckwizard/content/config.xul","createconfig","chrome,centerscreen,modal");
  updateconfiglist();
}

function CopyConfig()
{
  window.openDialog("chrome://cckwizard/content/config.xul","copyconfig","chrome,centerscreen,modal");
  
  updateconfiglist();
}

function DeleteConfig()
{
  var bundle = document.getElementById("bundle_cckwizard");

  var button = gPromptService.confirmEx(window, bundle.getString("windowTitle"), bundle.getString("deleteConfirm"),
                                        gPromptService.BUTTON_TITLE_YES * gPromptService.BUTTON_POS_0 +
                                        gPromptService.BUTTON_TITLE_NO * gPromptService.BUTTON_POS_1,
                                        null, null, null, null, {});
  if (button == 0) {
    gPrefBranch.deleteBranch("cck.config."+currentconfigname);
    currentconfigname = "";
    currentconfigpath = "";
    updateconfiglist();
  }
}

function SetSaveOnExitPref()
{
    gPrefBranch.setBoolPref("cck.save_on_exit", document.getElementById("saveOnExit").checked);
}

function OpenCCKWizard()
{
   try {
     document.getElementById("saveOnExit").checked = gPrefBranch.getBoolPref("cck.save_on_exit");
   } catch (ex) {
   }
   try {
     document.getElementById("zipLocation").value = gPrefBranch.getCharPref("cck.path_to_zip");
   } catch (ex) {
   }
   

}

function ShowMain()
{
   document.getElementById('example-window').canRewind = false;
   updateconfiglist();
}

function updateconfiglist()
{
  var menulist = document.getElementById('byb-configs')
  menulist.selectedIndex = -1;
  menulist.removeAllItems();
  var configname;
  var selecteditem = false;


  
  var list = gPrefBranch.getChildList("cck.config.", {});
  for (var i = 0; i < list.length; ++i) {
    configname = list[i].replace(/cck.config./g, "");
    var menulistitem = menulist.appendItem(configname,configname);
    menulistitem.minWidth=menulist.width;
    if (configname == currentconfigname) {
      menulist.selectedItem = menulistitem;
      selecteditem = true;
      document.getElementById('example-window').canAdvance = true;
      document.getElementById('byb-configs').disabled = false;
      document.getElementById('deleteconfig').disabled = false;
      document.getElementById('showconfig').disabled = false;
      document.getElementById('copyconfig').disabled = false;
    }
  }
  if ((!selecteditem) && (list.length > 0)) {
    menulist.selectedIndex = 0;
    setcurrentconfig(list[0].replace(/cck.config./g, ""));
  }
  if (list.length == 0) {
    document.getElementById('example-window').canAdvance = false;
    document.getElementById('byb-configs').disabled = true;
    document.getElementById('deleteconfig').disabled = true;
    document.getElementById('showconfig').disabled = true;
    document.getElementById('copyconfig').disabled = true;
    currentconfigname = "";
    currentconfigpath = "";
  }
}

function setcurrentconfig(newconfig)
{
  var destdir = Components.classes["@mozilla.org/file/local;1"]
                          .createInstance(Components.interfaces.nsILocalFile);

  if (currentconfigpath) {
    destdir.initWithPath(currentconfigpath);
    CCKWriteConfigFile(destdir);
  }
  currentconfigname = newconfig;
  currentconfigpath = gPrefBranch.getCharPref("cck.config." + currentconfigname);
  destdir.initWithPath(currentconfigpath);
  ClearAll();
  CCKReadConfigFile(destdir);
}

function saveconfig()
{


  if (currentconfigpath) {
  var destdir = Components.classes["@mozilla.org/file/local;1"]
                          .createInstance(Components.interfaces.nsILocalFile);
  


    destdir.initWithPath(currentconfigpath);
    CCKWriteConfigFile(destdir);
  }

}

function CloseCCKWizard()
{
  if (document.getElementById('example-window').pageIndex == 0)
    return;
  var saveOnExit;
  try {
    saveOnExit = gPrefBranch.getBoolPref("cck.save_on_exit");
 } catch (ex) {
    saveOnExit = false;
 }

  var button;
  if (!saveOnExit) {
    var bundle = document.getElementById("bundle_cckwizard");

    var button = gPromptService.confirmEx(window, bundle.getString("windowTitle"), bundle.getString("cancelConfirm"),
                                          (gPromptService.BUTTON_TITLE_YES * gPromptService.BUTTON_POS_0) +
                                          (gPromptService.BUTTON_TITLE_NO * gPromptService.BUTTON_POS_1),
                                          null, null, null, null, {});
  } else {
    button = 0;
  }
  
  if (button == 0) {
    saveconfig();
  }
  gPrefBranch.setCharPref("cck.path_to_zip", document.getElementById("zipLocation").value);
}


function OnConfigLoad()
{
  configCheckOKButton();
}


function ClearAll()
{
    /* clear out all data */
    var elements = document.getElementsByAttribute("id", "*");
    for (var i=0; i < elements.length; i++) {
      if ((elements[i].nodeName == "textbox") ||
          (elements[i].nodeName == "radiogroup") ||
          (elements[i].id == "RootKey1") ||
          (elements[i].id == "Type1")) {
        if ((elements[i].id != "saveOnExit") && (elements[i].id != "zipLocation")) {
          elements[i].value = "";
        }
      } else if (elements[i].nodeName == "checkbox") {
        if (elements[i].id != "saveOnExit")
          elements[i].checked = false;
      } else if (elements[i].className == "ccklist") {
        document.getElementById(elements[i].id).clear();
      }
    } 
}

function OnConfigOK()
{
  if (!(ValidateDir('cnc-location'))) {
    return false;
  }
  var configname = document.getElementById('cnc-name').value;
  var configlocation = document.getElementById('cnc-location').value;
  if (window.name == 'copyconfig') {
    var destdir = Components.classes["@mozilla.org/file/local;1"]
                            .createInstance(Components.interfaces.nsILocalFile);
    destdir.initWithPath(configlocation);
    this.opener.CCKWriteConfigFile(destdir);
  }
  gPrefBranch.setCharPref("cck.config." + configname, configlocation);
  this.opener.setcurrentconfig(configname);

}

function configCheckOKButton()
{
  if ((document.getElementById("cnc-name").value) && (document.getElementById("cnc-location").value)) {
    document.documentElement.getButton("accept").setAttribute( "disabled", "false" );
  } else {
    document.documentElement.getButton("accept").setAttribute( "disabled", "true" );  
  }
}

function onNewPreference()
{
  window.openDialog("chrome://cckwizard/content/pref.xul","newpref","chrome,centerscreen,modal");
}

function onEditPreference()
{
  window.openDialog("chrome://cckwizard/content/pref.xul","editpref","chrome,centerscreen,modal");
}

function OnPrefLoad()
{
  listbox = this.opener.document.getElementById('prefList');    
  if (window.name == 'editpref') {
    window.title = listbox.selectedItem.cck['type'];
    if (listbox.selectedItem.cck['type'] == "integer") {
      document.getElementById('prefvalue').preftype = nsIPrefBranch.PREF_INT;
    }
    document.getElementById('prefname').value = listbox.selectedItem.label;
    document.getElementById('prefvalue').value = listbox.selectedItem.value;
    document.getElementById('prefname').disabled = true;
    if (listbox.selectedItem.cck['lock'] == "true")
      document.getElementById('lockPref').checked = true;
    if (listbox.selectedItem.cck['type'] == "boolean") {
      document.getElementById('prefvalue').hidden = true;
      document.getElementById('prefvalueboolean').hidden = false;
      document.getElementById('prefvalueboolean').value = listbox.selectedItem.value;
    }
  }
  prefCheckOKButton();
  
}

function prefCheckOKButton()
{
  if (document.getElementById("prefname").value) {
    document.documentElement.getButton("accept").setAttribute( "disabled", "false" );
  } else {
    document.documentElement.getButton("accept").setAttribute( "disabled", "true" );  
  }
}

function prefSetPrefValue()
{
  var prefname = document.getElementById('prefname').value;
  try {
    var preftype = gPrefBranch.getPrefType(prefname);
    switch (preftype) {
      case nsIPrefBranch.PREF_STRING:
        document.getElementById('prefvalue').value = gPrefBranch.getCharPref(prefname);
        document.getElementById('prefvalue').hidden = false;
        document.getElementById('prefvalueboolean').hidden = true;
        document.getElementById('prefvalue').preftype = nsIPrefBranch.PREF_STRING;
        break;
      case nsIPrefBranch.PREF_INT:
        document.getElementById('prefvalue').value = gPrefBranch.getIntPref(prefname);
        document.getElementById('prefvalue').hidden = false;
        document.getElementById('prefvalueboolean').hidden = true;
        document.getElementById('prefvalue').preftype = nsIPrefBranch.PREF_INT;
        break;
      case nsIPrefBranch.PREF_BOOL:
        document.getElementById('prefvalue').value = gPrefBranch.getBoolPref(prefname);
        document.getElementById('prefvalue').hidden = true;
        document.getElementById('prefvalueboolean').hidden = false;
        document.getElementById('prefvalueboolean').value = gPrefBranch.getBoolPref(prefname);
        document.getElementById('prefvalue').preftype = nsIPrefBranch.PREF_BOOL;
        break;
      default:
        document.getElementById('prefvalue').hidden = false;
        document.getElementById('prefvalueboolean').hidden = true;
        break;
    }
  } catch (ex) {
    document.getElementById('prefvalue').hidden = false;
    document.getElementById('prefvalueboolean').hidden = true;
  }
}

function OnPrefOK()
{
  var bundle = this.opener.document.getElementById("bundle_cckwizard");
  
  listbox = this.opener.document.getElementById("prefList");
  for (var i=0; i < listbox.getRowCount(); i++) {
    if ((document.getElementById('prefname').value == listbox.getItemAtIndex(i).label) && (window.name == 'newpref')) {
      gPromptService.alert(window, bundle.getString("windowTitle"),
                           bundle.getString("prefExistsError"));
      return false;                           
    }
  }

  

  if (((document.getElementById('prefname').value == "browser.startup.homepage") || (document.getElementById('prefname').value == "browser.throbber.url")) && 
      (document.getElementById('prefvalue').value.length > 0)) {
    gPromptService.alert(window, bundle.getString("windowTitle"),
                         bundle.getString("lockError"));
    return false;
  }

  var value = document.getElementById('prefvalue').value;
  
  if (document.getElementById('prefvalue').preftype == nsIPrefBranch.PREF_INT) {
    if (parseInt(value) != value) {
      gPromptService.alert(window, bundle.getString("windowTitle"),
                           bundle.getString("intError"));
      return false;
    }
  }

  listbox = this.opener.document.getElementById('prefList');
  var listitem;
  if (window.name == 'newpref') {
    var preftype;
    if ((value.toLowerCase() == "true") || (value.toLowerCase() == "false")) {
      preftype = "boolean";            
    } else if (parseInt(value) == value) {
      preftype = "integer";      
    } else {
      preftype  = "string";      
      if (value.charAt(0) == '"')
        value = value.substring(1,value.length);
      if (value.charAt(value.length-1) == '"')
        if (value.charAt(value.length-2) != '\\')
          value = value.substring(0,value.length-1);
    }
    listitem = listbox.appendItem(document.getElementById('prefname').value, value);
    listitem.cck['type'] = preftype;
  } else {
    listitem = listbox.selectedItem;
    listitem.setAttribute("label", document.getElementById('prefname').value);
    value = document.getElementById('prefvalue').value;
    if (value.charAt(0) == '"')
      value = value.substring(1,value.length);
    if (value.charAt(value.length-1) == '"')
      if (value.charAt(value.length-2) != '\\')
        value = value.substring(0,value.length-1);
    listitem.setAttribute("value", value);
  }
  if (document.getElementById('lockPref').checked) {
    listitem.cck['lock'] = "true";
  } else {
    listitem.cck['lock'] = "";
  }
}

function getPageId()
{
  temp = document.getElementById('example-window');
  if (!temp)
    temp = this.opener.document.getElementById('example-window');
  return temp.currentPage.id;
}


function onNewBookmark()
{
  window.openDialog("chrome://cckwizard/content/bookmark.xul","newbookmark","chrome,centerscreen,modal");
}

function onEditBookmark()
{
  window.openDialog("chrome://cckwizard/content/bookmark.xul","editbookmark","chrome,centerscreen,modal");
}

function OnBookmarkLoad()
{
  listbox = this.opener.document.getElementById(getPageId() +'.bookmarkList');    
  if (window.name == 'editbookmark') {
    document.getElementById('bookmarkname').value = listbox.selectedItem.label;
    document.getElementById('bookmarkurl').value = listbox.selectedItem.value;
    if (listbox.selectedItem.cck['type'] == "live")
      document.getElementById('liveBookmark').checked = true;
  }
  bookmarkCheckOKButton();
}

function bookmarkCheckOKButton()
{
  if ((document.getElementById("bookmarkname").value) && (document.getElementById("bookmarkurl").value)) {
    document.documentElement.getButton("accept").setAttribute( "disabled", "false" );
  } else {
    document.documentElement.getButton("accept").setAttribute( "disabled", "true" );  
  }
}

function OnBookmarkOK()
{

  listbox = this.opener.document.getElementById(getPageId() +'.bookmarkList');
  var listitem;
  if (window.name == 'newbookmark') {
    listitem = listbox.appendItem(document.getElementById('bookmarkname').value, document.getElementById('bookmarkurl').value);
    listitem.setAttribute("class", "listitem-iconic");
  } else {
    listitem = listbox.selectedItem;
    listitem.setAttribute("label", document.getElementById('bookmarkname').value);
    listitem.setAttribute("value", document.getElementById('bookmarkurl').value);
  }
  if (document.getElementById('liveBookmark').checked) {
    listitem.cck['type'] = "live";
    listitem.setAttribute("image", "chrome://browser/skin/page-livemarks.png");
  } else {
    listitem.setAttribute("image", "chrome://browser/skin/Bookmarks-folder.png");
    listitem.cck['type'] = "";
  }
}

function enableBookmarkButtons() {
  listbox = document.getElementById(getPageId() +'.bookmarkList');
  if (listbox.selectedItem) {
    document.getElementById(getPageId() +'editBookmarkButton').disabled = false;
    document.getElementById(getPageId() +'deleteBookmarkButton').disabled = false;
  } else {
    document.getElementById(getPageId() +'editBookmarkButton').disabled = true;
    document.getElementById(getPageId() +'deleteBookmarkButton').disabled = true;
  }
}

function onNewBrowserPlugin()
{
  window.openDialog("chrome://cckwizard/content/plugin.xul","newplugin","chrome,centerscreen,modal");
}

function onEditBrowserPlugin()
{
  window.openDialog("chrome://cckwizard/content/plugin.xul","editplugin","chrome,centerscreen,modal");
}

function OnPluginLoad()
{
  listbox = this.opener.document.getElementById('browserPluginList');    
  if (window.name == 'editplugin') {
    document.getElementById('pluginpath').value = listbox.selectedItem.label;
    document.getElementById('plugintype').value = listbox.selectedItem.value;
  }
  pluginCheckOKButton();
  
}

function pluginCheckOKButton()
{
  if (document.getElementById("pluginpath").value) {
    document.documentElement.getButton("accept").setAttribute( "disabled", "false" );
  } else {
    document.documentElement.getButton("accept").setAttribute( "disabled", "true" );  
  }
}

function OnBrowserPluginOK()
{
  if (!(ValidateFile('pluginpath'))) {
    return false;
  }

  listbox = this.opener.document.getElementById('browserPluginList');    
  if (window.name == 'newplugin') {
    listitem = listbox.appendItem(document.getElementById('pluginpath').value, document.getElementById('plugintype').value);
  } else {
    listbox.selectedItem.label = document.getElementById('pluginpath').value;
    listbox.selectedItem.value = document.getElementById('plugintype').selectedItem.value;
  }
}

function onNewRegKey()
{
  window.openDialog("chrome://cckwizard/content/reg.xul","newreg","chrome,centerscreen,modal");
}

function onEditRegKey()
{
  window.openDialog("chrome://cckwizard/content/reg.xul","editreg","chrome,centerscreen,modal");
}

function OnRegLoad()
{
  listbox = this.opener.document.getElementById('regList');
  if (window.name == 'editreg') {
    document.getElementById('PrettyName').value = listbox.selectedItem.label;
    document.getElementById('RootKey').value = listbox.selectedItem.cck['rootkey'];
    document.getElementById('Key').value = listbox.selectedItem.cck['key'];
    document.getElementById('Name').value = listbox.selectedItem.cck['name'];
    document.getElementById('NameValue').value = listbox.selectedItem.cck['namevalue'];
    document.getElementById('Type').value = listbox.selectedItem.cck['type'];
  }
  regCheckOKButton();
}

function regCheckOKButton()
{
  if ((document.getElementById("PrettyName").value) &&
      (document.getElementById("Key").value) &&
      (document.getElementById("Name").value) &&
      (document.getElementById("NameValue").value)) {
      document.documentElement.getButton("accept").setAttribute( "disabled", "false" );
  } else {
    document.documentElement.getButton("accept").setAttribute( "disabled", "true" );  
  }
}

function OnRegOK()
{
  listbox = this.opener.document.getElementById('regList');
  var listitem;
  if (window.name == 'newreg') {
    listitem = listbox.appendItem(document.getElementById('PrettyName').value, "");
  } else {
    listitem = listbox.selectedItem;
    listitem.setAttribute("label", document.getElementById('PrettyName').value);
  }
  listitem.cck['rootkey'] = document.getElementById('RootKey').value;
  listitem.cck['key'] = document.getElementById('Key').value;
  listitem.cck['name'] = document.getElementById('Name').value;
  listitem.cck['namevalue'] = document.getElementById('NameValue').value;
  listitem.cck['type'] = document.getElementById('Type').value;
}

function onNewSearchEngine()
{
  window.openDialog("chrome://cckwizard/content/searchengine.xul","newsearchengine","chrome,centerscreen,modal");
}

function onEditSearchEngine()
{
  window.openDialog("chrome://cckwizard/content/searchengine.xul","editsearchengine","chrome,centerscreen,modal");
}

function OnSearchEngineLoad()
{
  listbox = this.opener.document.getElementById('searchEngineList');    
  if (window.name == 'editsearchengine') {
    document.getElementById('searchengine').value = listbox.selectedItem.label;
    document.getElementById('searchengineicon').value = listbox.selectedItem.value;
    document.getElementById('icon').src = listbox.selectedItem.value;
  }
  searchEngineCheckOKButton();
  
}

function searchEngineCheckOKButton()
{
  if ((document.getElementById("searchengine").value) && (document.getElementById("searchengineicon").value)) {
    document.documentElement.getButton("accept").setAttribute( "disabled", "false" );
  } else {
    document.documentElement.getButton("accept").setAttribute( "disabled", "true" );  
  }
}

function OnSearchEngineOK()
{
  if (!(ValidateFile('searchengine', 'searchengineicon'))) {
    return false;
  }



  listbox = this.opener.document.getElementById('searchEngineList');
  var listitem;
  if (window.name == 'newsearchengine') {
    listitem = listbox.appendItem(document.getElementById('searchengine').value, document.getElementById('searchengineicon').value);
    listitem.setAttribute("class", "listitem-iconic");    
  } else {
    listitem = listbox.selectedItem;
    listbox.selectedItem.label = document.getElementById('searchengine').value;
    listbox.selectedItem.value = document.getElementById('searchengineicon').value;
  }
  var sourcefile = Components.classes["@mozilla.org/file/local;1"]
                             .createInstance(Components.interfaces.nsILocalFile);
  sourcefile.initWithPath(document.getElementById('searchengineicon').value);
  var ioServ = Components.classes["@mozilla.org/network/io-service;1"]
                         .getService(Components.interfaces.nsIIOService);
  var imgfile = ioServ.newFileURI(sourcefile);
  listitem.setAttribute("image", imgfile.spec);
}

function onNewCert()
{
  window.openDialog("chrome://cckwizard/content/cert.xul","newcert","chrome,centerscreen,modal");
}

function onEditCert()
{
  window.openDialog("chrome://cckwizard/content/cert.xul","editcert","chrome,centerscreen,modal")
}

function OnCertLoad()
{
  listbox = this.opener.document.getElementById('certList');    
  if (window.name == 'editcert') {
    document.getElementById('certpath').value = listbox.selectedItem.label;
    var trustString = listbox.selectedItem.value;
    if (trustString.charAt(0) == 'C') {
      document.getElementById("trustSSL").checked = true;
    }
    if (trustString.charAt(2) == 'C') {
      document.getElementById("trustEmail").checked = true;
    }
    if (trustString.charAt(4) == 'C') {
      document.getElementById("trustObjSign").checked = true;
    }
  }
  certCheckOKButton();
}

function certCheckOKButton()
{
  if (document.getElementById("certpath").value) {
    document.documentElement.getButton("accept").setAttribute( "disabled", "false" );
  } else {
    document.documentElement.getButton("accept").setAttribute( "disabled", "true" );  
  }
}

function OnCertOK()
{
  if (!(ValidateFile('certpath'))) {
    return false;
  }

  var trustString = "";
  if (document.getElementById("trustSSL").checked) {
    trustString += "C,"
  } else {
    trustString += "c,"
  }
  if (document.getElementById("trustEmail").checked) {
    trustString += "C,"
  } else {
    trustString += "c,"
  }
  if (document.getElementById("trustObjSign").checked) {
    trustString += "C"
  } else {
    trustString += "c"
  }

  listbox = this.opener.document.getElementById('certList');
  var listitem;
  if (window.name == 'newcert') {
    listitem = listbox.appendItem(document.getElementById('certpath').value, trustString);
  } else {
    listitem = listbox.selectedItem;
    listbox.selectedItem.label = document.getElementById('certpath').value;
    listbox.selectedItem.value = trustString;
  }
}




function onNewBundle()
{
  try {
    var nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, "Choose File...", nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterHTML | nsIFilePicker.filterText |
                     nsIFilePicker.filterAll | nsIFilePicker.filterImages | nsIFilePicker.filterXML);

    if (fp.show() == nsIFilePicker.returnOK && fp.fileURL.spec && fp.fileURL.spec.length > 0) {
      listbox = document.getElementById('bundleList');
      listitem = listbox.appendItem(fp.file.path, "");
    }
  }
  catch(ex) {
  }
}

function onEditBundle()
{
  listbox = document.getElementById('bundleList');
  filename = listbox.selectedItem.label;
  var sourcefile = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  try {
    sourcefile.initWithPath(filename);
    var ioServ = Components.classes["@mozilla.org/network/io-service;1"]
                           .getService(Components.interfaces.nsIIOService);
                           
  } catch (ex) {
  }

  try {
    var nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    var bundle = document.getElementById("bundle_cckwizard");
    fp.init(window, bundle.getString("chooseFile"), nsIFilePicker.modeOpen);
    fp.displayDirectory = sourcefile.parent;
    fp.defaultString = sourcefile.leafName;
    fp.appendFilters(nsIFilePicker.filterAll);
    if (fp.show() == nsIFilePicker.returnOK && fp.fileURL.spec && fp.fileURL.spec.length > 0) {
      listbox.selectedItem.label = fp.file.path;
    }
  }
  catch(ex) {
  }
}

function CreateCCK()
{ 
  gPrefBranch.setCharPref("cck.path_to_zip", document.getElementById("zipLocation").value);
/* ---------- */
  var destdir = Components.classes["@mozilla.org/file/local;1"]
                          .createInstance(Components.interfaces.nsILocalFile);
  destdir.initWithPath(currentconfigpath);
  
  CCKWriteConfigFile(destdir);

  destdir.append("jar");
  try {
    destdir.remove(true);
  } catch(ex) {}
  
  destdir.append("content");
  destdir.append("cck");
  try {
    destdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
  } catch(ex) {}
  
  CCKWriteXULOverlay(destdir);
  CCKWriteDTD(destdir);
  CCKWriteCSS(destdir);
  CCKWriteProperties(destdir);
  CCKCopyFile(document.getElementById("iconURL").value, destdir);
  CCKCopyFile(document.getElementById("LargeAnimPath").value, destdir);
  CCKCopyFile(document.getElementById("LargeStillPath").value, destdir);
  CCKCopyChromeToFile("cck.js", destdir)
  if (document.getElementById("noaboutconfig").checked)
    CCKCopyChromeToFile("cck-config.css", destdir)

  listbox = document.getElementById('certList');

  for (var i=0; i < listbox.getRowCount(); i++) {
    listitem = listbox.getItemAtIndex(i);
    CCKCopyFile(listitem.getAttribute("label"), destdir);
  }

/* copy/create contents.rdf if 1.0 */
  var zipdir = Components.classes["@mozilla.org/file/local;1"]
                         .createInstance(Components.interfaces.nsILocalFile);
  zipdir.initWithPath(currentconfigpath);
  zipdir.append("jar");
  CCKZip("cck.jar", zipdir, "content");
  
/* ---------- */

  destdir.initWithPath(currentconfigpath);
  destdir.append("xpi");
  try {
    destdir.remove(true);
  } catch(ex) {}
  try {
    destdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);    
  } catch(ex) {}
  
  CCKWriteConfigFile(destdir);  
  destdir.append("chrome");
  try {
    destdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
  } catch(ex) {}

  zipdir.append("cck.jar");

  CCKCopyFile(zipdir.path, destdir);

/* ---------- */

  destdir.initWithPath(currentconfigpath);
  destdir.append("xpi");
  destdir.append("components");
  try {
    destdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
  } catch(ex) {}

  CCKCopyChromeToFile("cckService.js", destdir);
  if (document.getElementById("noaboutconfig").checked)
    CCKCopyChromeToFile("disableAboutConfig.js", destdir);
  
/* ---------- */

  destdir.initWithPath(currentconfigpath);
  destdir.append("xpi");
  destdir.append("defaults");
  destdir.append("preferences");
  try {
    destdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
  } catch(ex) {}

  CCKWriteDefaultJS(destdir)
  
/* ---------- */

  destdir.initWithPath(currentconfigpath);
  destdir.append("xpi");
  destdir.append("platform");
  try {
    destdir.remove(true);
    destdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
  } catch(ex) {}

  listbox = document.getElementById('browserPluginList');

  for (var i=0; i < listbox.getRowCount(); i++) {
    listitem = listbox.getItemAtIndex(i);
    var pluginsubdir = destdir.clone();
    /* If there is no value, assume windows - this should only happen for migration */
    if (listitem.getAttribute("value")) {
      pluginsubdir.append(listitem.getAttribute("value"));
    } else {
      pluginsubdir.append("WINNT_x86-msvc");
    }
    pluginsubdir.append("plugins");
    try {
      pluginsubdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
    } catch(ex) {}
    CCKCopyFile(listitem.getAttribute("label"), pluginsubdir);
  }

  destdir.initWithPath(currentconfigpath);
  destdir.append("xpi");
  destdir.append("searchplugins");
  try {
    destdir.remove(true);
    destdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
  } catch(ex) {}
  
  listbox = document.getElementById('searchEngineList');

  for (var i=0; i < listbox.getRowCount(); i++) {
    listitem = listbox.getItemAtIndex(i);
    CCKCopyFile(listitem.getAttribute("label"), destdir);
    CCKCopyFile(listitem.getAttribute("value"), destdir);
  }

  destdir.initWithPath(currentconfigpath);
  destdir.append("xpi");

  CCKCopyChromeToFile("chrome.manifest", destdir)
  CCKWriteInstallRDF(destdir);
         
  CCKWriteInstallJS(destdir);
  var filename = document.getElementById("filename").value;
  if (filename.length == 0)
    filename = "cck";
  filename += ".xpi";

  CCKZip("cck.xpi", destdir,
         "chrome", "components", "defaults", "platform", "searchplugins", "chrome.manifest", "install.rdf", "install.js", "cck.config");

  var outputdir = Components.classes["@mozilla.org/file/local;1"]
                            .createInstance(Components.interfaces.nsILocalFile);

  outputdir.initWithPath(currentconfigpath);
  destdir.append("cck.xpi");
  
  if (document.getElementById('bundleList').getRowCount() == 0) {
    outputdir.append(filename);
    try {
      outputdir.remove(true);
    } catch(ex) {}
    outputdir = outputdir.parent;
    destdir.copyTo(outputdir, filename);
  } else {
    var packagedir = Components.classes["@mozilla.org/file/local;1"]
                               .createInstance(Components.interfaces.nsILocalFile);                   
                                                                                          
    packagedir.initWithPath(currentconfigpath);
    packagedir.append("package");
  
    try {
      packagedir.remove(true);
      packagedir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
    } catch(ex) {}

    CCKCopyFile(destdir.path, packagedir);

    listbox = document.getElementById('bundleList');

    for (var i=0; i < listbox.getRowCount(); i++) {    
      listitem = listbox.getItemAtIndex(i);
      CCKCopyFile(listitem.getAttribute("label"), packagedir);
    }

    CCKCopyChromeToFile("install.rdf.mip", packagedir)

    packagedir.append("install.rdf.mip");
    packagedir.moveTo(packagedir.parent, "install.rdf");

    packagedir = packagedir.parent;

    CCKZip("cck.zip", packagedir, "*.xpi", "*.jar", "install.rdf");
    packagedir.append("cck.zip");
    outputdir.append(filename);
    try {
      outputdir.remove(true);
    } catch(ex) {}
    outputdir = outputdir.parent;
    packagedir.copyTo(outputdir, filename);
  }

  var bundle = document.getElementById("bundle_cckwizard");

  outputdir.append(filename);

  gPromptService.alert(window, bundle.getString("windowTitle"),
                       bundle.getString("outputLocation") + outputdir.path);
}

/* This function takes a file in the chromedir and creates a real file */

function CCKCopyChromeToFile(chromefile, location)
{
  var file = location.clone();
  file.append(chromefile);

  try {
    file.remove(false);                         
  } catch (ex) {
  }
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);

  fos.init(file, -1, -1, false);

  var ioService=Components.classes["@mozilla.org/network/io-service;1"]
    .getService(Components.interfaces.nsIIOService);
  var scriptableStream=Components
    .classes["@mozilla.org/scriptableinputstream;1"]
    .getService(Components.interfaces.nsIScriptableInputStream);
    
  var channel=ioService.newChannel("chrome://cckwizard/content/srcfiles/" + chromefile + ".in",null,null);
  var input=channel.open();
  scriptableStream.init(input);
  var str=scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();

  fos.write(str, str.length); 
  fos.close();
}


/* This function creates a given zipfile in a given location */
/* It takes as parameters the names of all the files/directories to be contained in the ZIP file */
/* It works by creating a CMD file to generate the ZIP */
/* unless we have the spiffy ZipWriterCompoent from maf.mozdev.org */

function CCKZip(zipfile, location)
{
  var file = location.clone();
  file.append(zipfile);
  try {
    file.remove(false);
  } catch (ex) {}

  if ((document.getElementById("zipLocation").value == "") && (Components.interfaces.IZipWriterComponent)) {
    var archivefileobj = location.clone();
    archivefileobj.append(zipfile);
  
    try {
      var zipwriterobj = Components.classes["@ottley.org/libzip/zip-writer;1"]
                                   .createInstance(Components.interfaces.IZipWriterComponent);
                                
      zipwriterobj.CURR_COMPRESS_LEVEL = Components.interfaces.IZipWriterComponent.COMPRESS_LEVEL9;
        
      var sourcepathobj = Components.classes["@mozilla.org/file/local;1"]
                                    .createInstance(Components.interfaces.nsILocalFile);
      sourcepathobj.initWithPath(location.path);
  
      zipwriterobj.init(archivefileobj);
      
      zipwriterobj.basepath = sourcepathobj;
      
      var zipentriestoadd = new Array();
      
      for (var i=2; i < arguments.length; i++) {
        var sourcepathobj = location.clone();
        sourcepathobj.append(arguments[i]);
        if (sourcepathobj.exists() && sourcepathobj.isDirectory()) {
          var entries = sourcepathobj.directoryEntries;
  
          while (entries.hasMoreElements()) {
            zipentriestoadd.push(entries.getNext());
          }
        } else if (sourcepathobj.exists()) {
            zipentriestoadd.push(sourcepathobj);
        }
      }
      
      // Add files depth first
      while (zipentriestoadd.length > 0) {
        var zipentry = zipentriestoadd.pop();
        
        zipentry.QueryInterface(Components.interfaces.nsILocalFile);
       
        if (!zipentry.isDirectory()) {
          zipwriterobj.add(zipentry);
        }
        
        if (zipentry.exists() && zipentry.isDirectory()) {
          var entries = zipentry.directoryEntries;
  
          while (entries.hasMoreElements()) {
            zipentriestoadd.push(entries.getNext());
          }
        }        
      }
  
      zipwriterobj.commitUpdates();
      return;
    } catch (e) {
      gPromptService.alert(window, "", "ZIPWriterComponent error - attempting ZIP");
    }
  }
  
  var zipLocation = document.getElementById("zipLocation").value;
  if (zipLocation.length == 0) {
    zipLocation = "zip";
  }

  platform = navigator.platform;
  var file = location.clone();
             
  if (navigator.platform == "Win32")
    file.append("ccktemp.cmd");
  else
    file.append("ccktemp.sh");  
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);
  fos.init(file, -1, -1, false);
  
  var line = "cd ";
  // this param causes a drive switch on win32
  if (navigator.platform == "Win32")
    line += "/d ";
  line += "\"" + location.path + "\"\n";
  fos.write(line, line.length);
  if (navigator.platform == "Win32")
    line =  "\"" + zipLocation + "\" -r \"" + location.path + "\\" + zipfile + "\"";
  else
    line = zipLocation + " -r \"" + location.path + "/" + zipfile + "\"";  
  for (var i=2; i < arguments.length; i++) {
    line += " " + arguments[i];
  }
  line += "\n";
  fos.write(line, line.length);  
  fos.close();

  var sh;

  // create an nsILocalFile for the executable
  if (navigator.platform != "Win32") {
    sh = Components.classes["@mozilla.org/file/local;1"]
                   .createInstance(Components.interfaces.nsILocalFile);
    sh.initWithPath("/bin/sh");
  }
  // create an nsIProcess
  var process = Components.classes["@mozilla.org/process/util;1"]
                          .createInstance(Components.interfaces.nsIProcess);
                          
  if (navigator.platform == "Win32")
    process.init(file);
  else
    process.init(sh);

  var args = [file.path];
  
  process.run(true, args, args.length);
  file.remove(false);
  var file = location.clone();
  file.append(zipfile);
  if (!file.exists()) {
    var bundle = document.getElementById("bundle_cckwizard");
    gPromptService.alert(window, bundle.getString("windowTitle"),
                       bundle.getString("zipError"));
  }
}

function CCKWriteXULOverlay(destdir)
{
  var tooltipXUL  = '  <button id="navigator-throbber" tooltiptext="&throbber.tooltip;"/>\n';

  var titlebarXUL = '  <window id="main-window" titlemodifier="&mainWindow.titlemodifier;"/>\n';

  var helpmenu1   = '  <menupopup id="menu_HelpPopup">\n';
  var helpmenu2   = '    <menuseparator insertafter="aboutSeparator"/>\n';
  var helpmenu3   = '    <menuitem label="&cckHelp.label;" insertafter="aboutSeparator"\n';
  var helpmenu4   = '              accesskey="&cckHelp.accesskey;"\n';
  var helpmenu5   = '              oncommand="openUILink(getCCKLink(\'cckhelp.url\'), event, false, true);"\n';
  var helpmenu6   = '              onclick="checkForMiddleClick(this, event);"/>\n';
  var helpmenu7   = '  </menupopup>\n';

  var file = destdir.clone();
  file.append("cck-browser-overlay.xul");
  try {
    file.remove(false);                         
  } catch (ex) {}
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                      .createInstance(Components.interfaces.nsIFileOutputStream);

  fos.init(file, -1, -1, false);
  
  var ioService=Components.classes["@mozilla.org/network/io-service;1"]
                          .getService(Components.interfaces.nsIIOService);
  var scriptableStream=Components.classes["@mozilla.org/scriptableinputstream;1"]
                                 .getService(Components.interfaces.nsIScriptableInputStream);
    
  var channel=ioService.newChannel("chrome://cckwizard/content/srcfiles/cck-browser-overlay.xul.in",null,null);
  var input=channel.open();
  scriptableStream.init(input);
  var str=scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();

  var tooltip = document.getElementById("AnimatedLogoTooltip").value;
  if (tooltip && (tooltip.length > 0))
    str = str.replace(/%button%/g, tooltipXUL);
  else
    str = str.replace(/%button%/g, "");

  var titlebar = document.getElementById("CompanyName").value;
  if (titlebar && (titlebar.length > 0))
      str = str.replace(/%window%/g, titlebarXUL);
  else
    str = str.replace(/%window%/g, "");

  var helpmenu = document.getElementById("HelpMenuCommandName").value;
  if (helpmenu && (helpmenu.length > 0)) {
    var helpmenuXUL = helpmenu1 + helpmenu2 + helpmenu3;
    var helpmenuakey = document.getElementById("HelpMenuCommandAccesskey").value;
    if (helpmenuakey && (helpmenuakey.length > 0)) {
      helpmenuXUL += helpmenu4;
    }
    helpmenuXUL += helpmenu5 + helpmenu6 + helpmenu7;
    str = str.replace(/%menupopup%/g, helpmenuXUL);
  } else {
    str = str.replace(/%menupopup%/g, "");
  }    

  fos.write(str, str.length); 
  fos.close();
}

function CCKWriteCSS(destdir)
{

var animated1 = '#navigator-throbber[busy="true"] {\n';
var animated2 = 'toolbar[iconsize="small"] #navigator-throbber[busy="true"],\n';
var animated3 = 'toolbar[mode="text"] #navigator-throbber[busy="true"] {\n';
var atrest1 = '#navigator-throbber {\n';
var atrest2 = 'toolbar[iconsize="small"] #navigator-throbber,\n';
var atrest3 = 'toolbar[mode="text"] #navigator-throbber {\n';
var liststyleimage = '  list-style-image: url("chrome://cck/content/';
var liststyleimageend = '");\n}\n';

  var file = destdir.clone();
  file.append("cck.css");
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);
  fos.init(file, -1, -1, false);

  var animatedlogopath = document.getElementById("LargeAnimPath").value;
  if (animatedlogopath && (animatedlogopath.length > 0)) {
    var file = Components.classes["@mozilla.org/file/local;1"]
                         .createInstance(Components.interfaces.nsILocalFile);
    file.initWithPath(animatedlogopath);

    fos.write(animated1, animated1.length);
    fos.write(liststyleimage, liststyleimage.length);
    fos.write(file.leafName, file.leafName.length);
    fos.write(liststyleimageend, liststyleimageend.length);

    fos.write(animated2, animated2.length);
    fos.write(animated3, animated3.length);
    fos.write(liststyleimage, liststyleimage.length);
    fos.write(file.leafName, file.leafName.length);
    fos.write(liststyleimageend, liststyleimageend.length);
  }
  var atrestlogopath = document.getElementById("LargeStillPath").value;
  if (atrestlogopath && (atrestlogopath.length > 0)) {
    var file = Components.classes["@mozilla.org/file/local;1"]
                         .createInstance(Components.interfaces.nsILocalFile);
    file.initWithPath(atrestlogopath);

    fos.write(atrest1, atrest1.length);
    fos.write(liststyleimage, liststyleimage.length);
    fos.write(file.leafName, file.leafName.length);
    fos.write(liststyleimageend, liststyleimageend.length);

    fos.write(atrest2, atrest2.length);
    fos.write(atrest3, atrest3.length);
    fos.write(liststyleimage, liststyleimage.length);
    fos.write(file.leafName, file.leafName.length);
    fos.write(liststyleimageend, liststyleimageend.length);
  }
  fos.close();
}

function CCKWriteDTD(destdir)
{
  var file = destdir.clone();
  file.append("cck.dtd");
  try {
    file.remove(false);                         
  } catch (ex) {}
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                      .createInstance(Components.interfaces.nsIFileOutputStream);
  var cos = Components.classes["@mozilla.org/intl/converter-output-stream;1"]
                      .createInstance(Components.interfaces.nsIConverterOutputStream);
  
  fos.init(file, -1, -1, false);
  cos.init(fos, null, 0, null);
  
  var ioService=Components.classes["@mozilla.org/network/io-service;1"]
                          .getService(Components.interfaces.nsIIOService);
  var scriptableStream=Components.classes["@mozilla.org/scriptableinputstream;1"]
                                 .getService(Components.interfaces.nsIScriptableInputStream);
    
  var channel=ioService.newChannel("chrome://cckwizard/content/srcfiles/cck.dtd.in",null,null);
  var input=channel.open();
  scriptableStream.init(input);
  var str=scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();

  str = str.replace(/%throbber.tooltip%/g, htmlEscape(document.getElementById("AnimatedLogoTooltip").value));
  str = str.replace(/%mainWindow.titlemodifier%/g, htmlEscape(document.getElementById("CompanyName").value));
  str = str.replace(/%cckHelp.label%/g, htmlEscape(document.getElementById("HelpMenuCommandName").value));
  str = str.replace(/%cckHelp.accesskey%/g, document.getElementById("HelpMenuCommandAccesskey").value);
  cos.writeString(str); 
  cos.close();
  fos.close();
}


function CCKWriteProperties(destdir)
{
  var file = destdir.clone();
  file.append("cck.properties");
  
  try {
    file.remove(false);                         
  } catch (ex) {}
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                      .createInstance(Components.interfaces.nsIFileOutputStream);
  var cos = Components.classes["@mozilla.org/intl/converter-output-stream;1"]
                      .createInstance(Components.interfaces.nsIConverterOutputStream);

  fos.init(file, -1, -1, false);
  cos.init(fos, null, 0, null);
  
  var ioService=Components.classes["@mozilla.org/network/io-service;1"]
                          .getService(Components.interfaces.nsIIOService);
  var scriptableStream=Components.classes["@mozilla.org/scriptableinputstream;1"]
                                 .getService(Components.interfaces.nsIScriptableInputStream);
    
  var channel=ioService.newChannel("chrome://cckwizard/content/srcfiles/cck.properties.in",null,null);
  var input=channel.open();
  scriptableStream.init(input);
  var str=scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();

  str = str.replace(/%id%/g, document.getElementById("id").value);
  str = str.replace(/%OrganizationName%/g, document.getElementById("OrganizationName").value);
  str = str.replace(/%browser.throbber.url%/g, document.getElementById("AnimatedLogoURL").value);
  str = str.replace(/%cckhelp.url%/g, document.getElementById("HelpMenuCommandURL").value);
  str = str.replace(/%browser.startup.homepage%/g, document.getElementById("HomePageURL").value);
  var overrideurl = document.getElementById('HomePageOverrideURL').value;
  if (overrideurl && overrideurl.length) {
    str = str.replace(/%startup.homepage_override_url%/g, overrideurl);
  } else {
    str = str.replace(/%startup.homepage_override_url%/g, document.getElementById("HomePageURL").value);
  }

  str = str.replace(/%PopupAllowedSites%/g, document.getElementById("PopupAllowedSites").value);
  str = str.replace(/%InstallAllowedSites%/g, document.getElementById("InstallAllowedSites").value);
  cos.writeString(str);
  
  if (document.getElementById("hidden").checked)
  {
    str = "hidden=true\n";
    cos.writeString(str);
  }

  if (document.getElementById("locked").checked)
  {
    str = "locked=true\n";
    cos.writeString(str);
  }

  radio = document.getElementById('ToolbarLocation');
  str = "ToolbarLocation=" + radio.value + "\n";
  cos.writeString(str);

/* Add toolbar/bookmark stuff at end */
  str = document.getElementById('ToolbarFolder1').value;
  if (str && str.length) {
    str = "ToolbarFolder1=" + str + "\n";
    cos.writeString(str);
    listbox = document.getElementById('tbFolder.bookmarkList');    
    for (var j=0; j < listbox.getRowCount(); j++) {
      listitem = listbox.getItemAtIndex(j);
      str = "ToolbarFolder1.BookmarkTitle" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
      cos.writeString(str);
      var str = "ToolbarFolder1.BookmarkURL" + (j+1) + "=" + listitem.getAttribute("value") + "\n";
      cos.writeString(str);
      if (listitem.cck['type'] && listitem.cck['type'].length) {
        var str = "ToolbarFolder1.BookmarkType" + (j+1) + "=" + listitem.cck['type'] + "\n";
        cos.writeString(str);
      }
    }
  }

  listbox = document.getElementById('tb.bookmarkList');    
  for (var j=0; j < listbox.getRowCount(); j++) {
    listitem = listbox.getItemAtIndex(j);
    str = "ToolbarBookmarkTitle" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
    cos.writeString(str);
    var str = "ToolbarBookmarkURL" + (j+1) + "=" + listitem.getAttribute("value") + "\n";
    cos.writeString(str);
    if (listitem.cck['type'] && listitem.cck['type'].length) {
      var str = "ToolbarBookmarkType" + (j+1) + "=" + listitem.cck['type'] + "\n";
      cos.writeString(str);
    }
  }

  radio = document.getElementById('BookmarkLocation');
  str = "BookmarkLocation=" + radio.value + "\n";
  cos.writeString(str);

  str = document.getElementById('BookmarkFolder1').value;
  if (str && str.length) {
    str = "BookmarkFolder1=" + str + "\n";
    cos.writeString(str);
    listbox = document.getElementById('bmFolder.bookmarkList');    
    for (var j=0; j < listbox.getRowCount(); j++) {
      listitem = listbox.getItemAtIndex(j);
      str = "BookmarkFolder1.BookmarkTitle" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
      cos.writeString(str);
      var str = "BookmarkFolder1.BookmarkURL" + (j+1) + "=" + listitem.getAttribute("value") + "\n";
      cos.writeString(str);
      if (listitem.cck['type'] && listitem.cck['type'].length) {
        var str = "BookmarkFolder1.BookmarkType" + (j+1) + "=" + listitem.cck['type'] + "\n";
        cos.writeString(str);
      }
    }
  }

  listbox = document.getElementById('bm.bookmarkList');    
  for (var j=0; j < listbox.getRowCount(); j++) {
    listitem = listbox.getItemAtIndex(j);
    str = "BookmarkTitle" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
    cos.writeString(str);
    var str = "BookmarkURL" + (j+1) + "=" + listitem.getAttribute("value") + "\n";
    cos.writeString(str);
    if (listitem.cck['type'] && listitem.cck['type'].length) {
      var str = "BookmarkType" + (j+1) + "=" + listitem.cck['type'] + "\n";
      cos.writeString(str);
    }
  }

  
  // Registry Keys
  listbox = document.getElementById("regList");
  for (var i=0; i < listbox.getRowCount(); i++) {
    listitem = listbox.getItemAtIndex(i);
    str = "RegName" + (i+1) + "=" + listitem.getAttribute("label") + "\n";
    cos.writeString(str);
    str = "RootKey" + (i+1) + "=" + listitem.cck['rootkey'] + "\n";
    cos.writeString(str);
    str = "Key" + (i+1) + "=" + listitem.cck['key'] + "\n";
    str = str.replace(/\\/g, "\\\\");
    cos.writeString(str);
    str = "Name" + (i+1) + "=" + listitem.cck['name'] + "\n";
    cos.writeString(str);
    str = "NameValue" + (i+1) + "=" + listitem.cck['namevalue'] + "\n";
    cos.writeString(str);
    str = "Type" + (i+1) + "=" + listitem.cck['type'] + "\n";
    cos.writeString(str);
  }

  // Pref locks
  listbox = document.getElementById("prefList");
  var j = 1;
  for (var i=0; i < listbox.getRowCount(); i++) {
    listitem = listbox.getItemAtIndex(i);
    if (listitem.cck['lock'] == "true") {
      str = "LockPref" + (j) + "=" + listitem.getAttribute("label") + "\n";
      cos.writeString(str);
      j++;
    }
  }

  
  listbox = document.getElementById('certList');

  for (var i=0; i < listbox.getRowCount(); i++) {
    listitem = listbox.getItemAtIndex(i);
    var file = Components.classes["@mozilla.org/file/local;1"]
                         .createInstance(Components.interfaces.nsILocalFile);
    file.initWithPath(listitem.getAttribute("label"));
    str = "Cert"+ (i+1) + "=" + file.leafName + "\n";
    cos.writeString(str);
    str = "CertTrust" + (i+1) + "=" + listitem.getAttribute("value") + "\n";
    cos.writeString(str);
  }

  cos.close();
  fos.close();
}

function prefIsLocked(prefname)
{
  listbox = document.getElementById("prefList");
  for (var i=0; i < listbox.getRowCount(); i++) {
    listitem = listbox.getItemAtIndex(i);
    if (prefname == listitem.getAttribute("label"))
      if (listitem.cck['lock'] == "true")
        return true;
  }

}

function CCKWriteDefaultJS(destdir)
{
  var throbber1 = 'pref("browser.throbber.url",            "';
  var homepage1 = 'pref("browser.startup.homepage",        "';
  var homepage2 = 'pref("startup.homepage_override_url",   "chrome://cck/content/cck.properties");\n';
  var chromeurl =   "chrome://cck/content/cck.properties";
  var prefend = '");\n';
  var useragent1begin = 'pref("general.useragent.vendorComment", "CK-';
  var useragent2begin = 'pref("general.useragent.extra.cck", "(CK-';

  var useragent1end = '");\n';
  var useragent2end = ')");\n';

  var file = destdir.clone();
  file.append("firefox-cck.js");
             
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);
  fos.init(file, -1, -1, false);

  var logobuttonurl = document.getElementById("AnimatedLogoURL").value;
  if (logobuttonurl && (logobuttonurl.length > 0)) {
    fos.write(throbber1, throbber1.length);
    if (prefIsLocked("browser.throbber.url")) {
      fos.write(logobuttonurl, logobuttonurl.length);
    } else {
      fos.write(chromeurl, chromeurl.length);
    }
    fos.write(prefend, prefend.length);
  }

  var browserstartuppage = document.getElementById("HomePageURL").value;
  var overrideurl = document.getElementById('HomePageOverrideURL').value;
  if (browserstartuppage && (browserstartuppage.length > 0)) {
    fos.write(homepage1, homepage1.length);
    if (prefIsLocked("browser.throbber.url")) {
      fos.write(browserstartuppage, browserstartuppage.length);
    } else {    
      fos.write(chromeurl, chromeurl.length);
    }
    fos.write(prefend, prefend.length);

    fos.write(homepage2, homepage2.length);
  } else if (overrideurl && overrideurl.length) {
    fos.write(homepage2, homepage2.length);
  }
  
  var useragent = document.getElementById("OrganizationName").value;
  if (useragent && (useragent.length > 0)) {
    fos.write(useragent1begin, useragent1begin.length);
    fos.write(useragent, useragent.length);
    fos.write(useragent1end, useragent1end.length);
    fos.write(useragent2begin, useragent2begin.length);
    fos.write(useragent, useragent.length);
    fos.write(useragent2end, useragent2end.length);
  }
  
  // Preferences
  listbox = document.getElementById("prefList");
  for (var i=0; i < listbox.getRowCount(); i++) {
    listitem = listbox.getItemAtIndex(i);
    /* allow for locking prefs without setting value */
    if (listitem.getAttribute("value").length) {
      var line;
      /* If it is a string, put quotes around it */
      if (listitem.cck['type'] == "string") {
        line = 'pref("' + listitem.getAttribute("label") + '", ' + '"' + listitem.getAttribute("value") + '"' + ');\n';
      } else {
        line = 'pref("' + listitem.getAttribute("label") + '", ' + listitem.getAttribute("value") + ');\n';
      }
      fos.write(line, line.length);
    }
  }
  
  var radiogroup = document.getElementById("ProxyType");
  if (radiogroup.value == "")
    radiogroup.value = "0";

  switch ( radiogroup.value ) {
    case "1":
      var proxystringlist = ["HTTPproxyname","SSLproxyname","FTPproxyname","Gopherproxyname","NoProxyname","autoproxyurl" ];
  
      for (i = 0; i < proxystringlist.length; i++) {
        var proxyitem = document.getElementById(proxystringlist[i]);
        if (proxyitem.value.length > 0) {
          var line = 'pref("' + proxyitem.getAttribute("preference") + '", "' + proxyitem.value + '");\n';
          fos.write(line, line.length);
        }
      }
  
      var proxyintegerlist = ["HTTPportno","SSLportno","FTPportno","Gopherportno","socksv","ProxyType"];

      for (i = 0; i < proxyintegerlist.length; i++) {
        var proxyitem = document.getElementById(proxyintegerlist[i]);
        if (proxyitem.value.length > 0) {
          var line = 'pref("' + proxyitem.getAttribute("preference") + '", ' + proxyitem.value + ');\n';
          fos.write(line, line.length);
        }
      }
  
      var proxyitem = document.getElementById("shareAllProxies");
      var line = 'pref("' + proxyitem.getAttribute("preference") + '", ' + proxyitem.checked + ');\n';
      fos.write(line, line.length);
      break;
    case "2":
      var proxystringlist = ["autoproxyurl"];
  
      for (i = 0; i < proxystringlist.length; i++) {
        var proxyitem = document.getElementById(proxystringlist[i]);
        if (proxyitem.value.length > 0) {
          var line = 'pref("' + proxyitem.getAttribute("preference") + '", "' + proxyitem.value + '");\n';
          fos.write(line, line.length);
        }
      }
      
      var proxyintegerlist = ["ProxyType"];

      for (i = 0; i < proxyintegerlist.length; i++) {
        var proxyitem = document.getElementById(proxyintegerlist[i]);
        if (proxyitem.value.length > 0) {
          var line = 'pref("' + proxyitem.getAttribute("preference") + '", ' + proxyitem.value + ');\n';
          fos.write(line, line.length);
        }
      }

      break;
  }

  fos.close();
}

function CCKWriteInstallRDF(destdir)
{
  var idline =          "<em:id>%id%</em:id>";
  var nameline =        "<em:name>%name%</em:name>";
  var versionline =     "<em:version>%version%</em:version>";
  var descriptionline = "<em:description>%description%</em:description>";
  var creatorline =     "<em:creator>%creator%</em:creator>";
  var homepageURLline = "<em:homepageURL>%homepageURL%</em:homepageURL>";
  var updateURLline =   "<em:updateURL>%updateURL%</em:updateURL>";  
  var iconURLline =     "<em:iconURL>chrome://cck/content/%iconURL%</em:iconURL>";


  var file = destdir.clone();

  file.append("install.rdf");
  try {
    file.remove(false);                         
  } catch (ex) {
  }
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);
  var cos = Components.classes["@mozilla.org/intl/converter-output-stream;1"]
                      .createInstance(Components.interfaces.nsIConverterOutputStream);

  fos.init(file, -1, -1, false);
  cos.init(fos, null, 0, null);

  var ioService=Components.classes["@mozilla.org/network/io-service;1"]
    .getService(Components.interfaces.nsIIOService);
  var scriptableStream=Components
    .classes["@mozilla.org/scriptableinputstream;1"]
    .getService(Components.interfaces.nsIScriptableInputStream);

  var channel=ioService.newChannel("chrome://cckwizard/content/srcfiles/install.rdf.in",null,null);
  var input=channel.open();
  scriptableStream.init(input);
  var str=scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();
  
  var id = document.getElementById("id").value;
  if (id && (id.length > 0)) {
    str = str.replace(/%idline%/g, idline);
    str = str.replace(/%id%/g, document.getElementById("id").value);
  }

  var name = document.getElementById("name").value;
  if (name && (name.length > 0)) {
    str = str.replace(/%nameline%/g, nameline);
    str = str.replace(/%name%/g, htmlEscape(document.getElementById("name").value));
  } else {
    str = str.replace(/%nameline%/g, "");
  }

  var version = document.getElementById("version").value;
  if (version && (version.length > 0)) {
    str = str.replace(/%versionline%/g, versionline);
    str = str.replace(/%version%/g, document.getElementById("version").value);
  } else {
    str = str.replace(/%versionline%/g, "");
  }

  var description = document.getElementById("description").value;
  if (description && (description.length > 0)) {
    str = str.replace(/%descriptionline%/g, descriptionline);
    str = str.replace(/%description%/g, htmlEscape(document.getElementById("description").value));
  } else {
    str = str.replace(/%descrptionline%/g, "");
  }

  var creator = document.getElementById("creator").value;
  if (creator && (creator.length > 0)) {
    str = str.replace(/%creatorline%/g, creatorline);
    str = str.replace(/%creator%/g, htmlEscape(document.getElementById("creator").value));
  } else {
    str = str.replace(/%creatorline%/g, "");
  }

  var homepageURL = document.getElementById("homepageURL").value;
  if (homepageURL && (homepageURL.length > 0)) {
    str = str.replace(/%homepageURLline%/g, homepageURLline);
    str = str.replace(/%homepageURL%/g, document.getElementById("homepageURL").value);
  } else {
    str = str.replace(/%homepageURLline%/g, "");
  }

  var updateURL = document.getElementById("updateURL").value;
  if (updateURL && (updateURL.length > 0)) {
    str = str.replace(/%updateURLline%/g, updateURLline);
    str = str.replace(/%updateURL%/g, document.getElementById("updateURL").value);
  } else {
    str = str.replace(/%updateURLline%/g, "");
  }

  var iconURL = document.getElementById("iconURL").value;
  if (iconURL && (iconURL.length > 0)) {
    var sourcefile = Components.classes["@mozilla.org/file/local;1"]
                               .createInstance(Components.interfaces.nsILocalFile);
    sourcefile.initWithPath(iconURL);
    str = str.replace(/%iconURLline%/g, iconURLline);
    str = str.replace(/%iconURL%/g, sourcefile.leafName);
  } else {
    str = str.replace(/%iconURLline%/g, "");
  }

  cos.writeString(str);
  cos.close();
  fos.close();
}

function CCKWriteInstallJS(destdir)
{
  var file = destdir.clone();
  file.append("install.js");
  try {
    file.remove(false);                         
  } catch (ex) {
  }
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);

  fos.init(file, -1, -1, false);
  var ioService=Components.classes["@mozilla.org/network/io-service;1"]
    .getService(Components.interfaces.nsIIOService);
  var scriptableStream=Components
    .classes["@mozilla.org/scriptableinputstream;1"]
    .getService(Components.interfaces.nsIScriptableInputStream);

  var channel=ioService.newChannel("chrome://cckwizard/content/srcfiles/install.js.in",null,null);
  var input=channel.open();
  scriptableStream.init(input);
  var str=scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();

  str = str.replace(/%id%/g, document.getElementById("id").value);
  str = str.replace(/%name%/g, document.getElementById("name").value);

  if (document.getElementById('browserPluginList').getRowCount() > 0)
    str = str.replace(/%plugins%/g, 'addDirectory("", "%version%", "platform", cckextensiondir, "platform", true);');
  else
    str = str.replace(/%plugins%/g, '');

  if (document.getElementById('searchEngineList').getRowCount() > 0)
    str = str.replace(/%searchplugins%/g, 'addDirectory("", "%version%", "searchplugins", cckextensiondir, "searchplugins", true);');
  else
    str = str.replace(/%searchplugins%/g, '');
    
  str = str.replace(/%installrdf%/g, 'addFile("", "%version%", "install.rdf", cckextensiondir, "", true);');  

  str = str.replace(/%version%/g, document.getElementById("version").value);
  
  fos.write(str, str.length); 
  fos.close();
}


/* This function copies a source file to a destination directory, including */
/* deleting the file at the destination if it exists */

function CCKCopyFile(source, destination)
{
  if (source.length == 0)
    return false;
  
  var sourcefile = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  sourcefile.initWithPath(source);

  var destfile = destination.clone();
  destfile.append(sourcefile.leafName);

  try {
    destfile.remove(false);
  } catch (ex) {}
  
  try {
    sourcefile.copyTo(destination, "");
  } catch (ex) {
      var bundle = document.getElementById("bundle_cckwizard");
      var consoleService = Components.classes["@mozilla.org/consoleservice;1"]
                                     .getService(Components.interfaces.nsIConsoleService);
      consoleService.logStringMessage(bundle.getString("windowTitle") + ": " + ex + "\n\nSource: " +  source + "\n\nDestination: " + destination.path );
      throw("Stopping Javascript execution");
  }
  
  return true;
}


function ShowConfigInfo()
{
  window.openDialog("chrome://cckwizard/content/showconfig.xul","showconfig","chrome,centerscreen,modal");
}

function InitConfigInfo()
{
  var file = Components.classes["@mozilla.org/file/local;1"]
                          .createInstance(Components.interfaces.nsILocalFile);
  file.initWithPath(this.opener.currentconfigpath);

  file.append("cck.config");
  
  if (!file.exists())
    return;

  var stream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                         .createInstance(Components.interfaces.nsIFileInputStream);

  stream.init(file, 0x01, 0644, 0);
  
  var lis = stream.QueryInterface(Components.interfaces.nsILineInputStream);
  var line = {value:null};
  
  var box = document.getElementById("showconfigy");
  
  do {
    var more = lis.readLine(line);
    var str = line.value;
    box.value += str;
    box.value += "\n";
  } while (more);
  
  stream.close();
}



function CCKWriteConfigFile(destdir)
{
  var file = destdir.clone();
  file.append("cck.config");
             
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);
                       
  fos.init(file, -1, -1, false);

  var elements = document.getElementsByAttribute("id", "*")
  for (var i=0; i < elements.length; i++) {
    if ((elements[i].nodeName == "textbox") ||
        (elements[i].id == "RootKey1") ||
        (elements[i].id == "Type1")) {
      if ((elements[i].id != "saveOnExit") && (elements[i].id != "zipLocation")) {
        if (elements[i].value.length > 0) {
          var line = elements[i].getAttribute("id") + "=" + elements[i].value + "\n";
          fos.write(line, line.length);
        }
      }
    } else if (elements[i].nodeName == "radiogroup") {
        if ((elements[i].value.length > 0) && (elements[i].value != "0")) {
          var line = elements[i].getAttribute("id") + "=" + elements[i].value + "\n";
          fos.write(line, line.length);
        }
    } else if (elements[i].nodeName == "checkbox") {
      if (elements[i].id != "saveOnExit") {
        if (elements[i].checked) {
          var line = elements[i].getAttribute("id") + "=" + elements[i].checked + "\n";
          fos.write(line, line.length);
        }
      }
    } else if (elements[i].id == "prefList") {
      listbox = document.getElementById('prefList');    
      for (var j=0; j < listbox.getRowCount(); j++) {
        listitem = listbox.getItemAtIndex(j);
        var line = "PreferenceName" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
        fos.write(line, line.length);
        if (listitem.getAttribute("value").length) {
          var line = "PreferenceValue" + (j+1) + "=" + listitem.getAttribute("value") + "\n";
          fos.write(line, line.length);
        }
	      if (listitem.cck['type'].length > 0) {
          var line = "PreferenceType" + (j+1) + "=" + listitem.cck['type'] + "\n";
          fos.write(line, line.length);
	      }
	      if (listitem.cck['lock'].length > 0) {
          var line = "PreferenceLock" + (j+1) + "=" + listitem.cck['lock'] + "\n";
          fos.write(line, line.length);
	      }
      }
    } else if (elements[i].id == "browserPluginList") {
      listbox = document.getElementById('browserPluginList');    
      for (var j=0; j < listbox.getRowCount(); j++) {
        listitem = listbox.getItemAtIndex(j);
        var line = "BrowserPluginPath" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
        fos.write(line, line.length);
        if (listitem.getAttribute("value")) {
          var line = "BrowserPluginType" + (j+1) + "=" + listitem.getAttribute("value") + "\n";
          fos.write(line, line.length);
	      }
      }
    } else if (elements[i].id == "tbFolder.bookmarkList") {
      listbox = document.getElementById('tbFolder.bookmarkList');
      for (var j=0; j < listbox.getRowCount(); j++) {
        listitem = listbox.getItemAtIndex(j);
        var line = "ToolbarFolder1.BookmarkTitle" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
        fos.write(line, line.length);
        if (listitem.getAttribute("value")) {
          var line = "ToolbarFolder1.BookmarkURL" + (j+1) + "=" + listitem.getAttribute("value") + "\n";
          fos.write(line, line.length);
	      }
	      if (listitem.cck['type'].length > 0) {
          var line = "ToolbarFolder1.BookmarkType" + (j+1) + "=" + listitem.cck['type'] + "\n";
          fos.write(line, line.length);
	      }
      }
    } else if (elements[i].id == "tb.bookmarkList") {
      listbox = document.getElementById('tb.bookmarkList');
      for (var j=0; j < listbox.getRowCount(); j++) {
        listitem = listbox.getItemAtIndex(j);
        var line = "ToolbarBookmarkTitle" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
        fos.write(line, line.length);
	      if (listitem.getAttribute("value")) {
          var line = "ToolbarBookmarkURL" + (j+1) + "=" + listitem.getAttribute("value") + "\n";
          fos.write(line, line.length);
	      }
	      if (listitem.cck['type'].length > 0) {
          var line = "ToolbarBookmarkType" + (j+1) + "=" + listitem.cck['type'] + "\n";
          fos.write(line, line.length);
	      }
      }
      
    } else if (elements[i].id == "bmFolder.bookmarkList") {
      listbox = document.getElementById('bmFolder.bookmarkList');
      for (var j=0; j < listbox.getRowCount(); j++) {
        listitem = listbox.getItemAtIndex(j);
        var line = "BookmarkFolder1.BookmarkTitle" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
        fos.write(line, line.length);
	      if (listitem.getAttribute("value")) {
          var line = "BookmarkFolder1.BookmarkURL" + (j+1) + "=" + listitem.getAttribute("value") + "\n";
          fos.write(line, line.length);
	      }
	      if (listitem.cck['type'].length > 0) {
          var line = "BookmarkFolder1.BookmarkType" + (j+1) + "=" + listitem.cck['type'] + "\n";
          fos.write(line, line.length);
	      }
      }
      
    } else if (elements[i].id == "bm.bookmarkList") {
      listbox = document.getElementById('bm.bookmarkList');
      for (var j=0; j < listbox.getRowCount(); j++) {
        listitem = listbox.getItemAtIndex(j);
        var line = "BookmarkTitle" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
        fos.write(line, line.length);
	      if (listitem.getAttribute("value")) {
          var line = "BookmarkURL" + (j+1) + "=" + listitem.getAttribute("value") + "\n";
          fos.write(line, line.length);
	      }
	      if (listitem.cck['type'].length > 0) {
          var line = "BookmarkType" + (j+1) + "=" + listitem.cck['type'] + "\n";
          fos.write(line, line.length);
	      }
      }
      
    } else if (elements[i].id == "regList") {
      listbox = document.getElementById('regList');    
      for (var j=0; j < listbox.getRowCount(); j++) {
        listitem = listbox.getItemAtIndex(j);
        var line = "RegName" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
        fos.write(line, line.length);
        var line = "RootKey" + (j+1) + "=" + listitem.cck['rootkey'] + "\n";
        fos.write(line, line.length);
        var line = "Key" + (j+1) + "=" + listitem.cck['key'] + "\n";
        fos.write(line, line.length);
        var line = "Name" + (j+1) + "=" + listitem.cck['name'] + "\n";
        fos.write(line, line.length);
        var line = "NameValue" + (j+1) + "=" + listitem.cck['namevalue'] + "\n";
        fos.write(line, line.length);
        var line = "Type" + (j+1) + "=" + listitem.cck['type'] + "\n";
        fos.write(line, line.length);
      }
    } else if (elements[i].id == "searchEngineList") {
      listbox = document.getElementById('searchEngineList');    
      for (var j=0; j < listbox.getRowCount(); j++) {
        listitem = listbox.getItemAtIndex(j);
        var line = "SearchEngine" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
        fos.write(line, line.length);
        var line = "SearchEngineIcon" + (j+1) + "=" + listitem.getAttribute("value") + "\n";
        fos.write(line, line.length);      
      }
    } else if (elements[i].id == "bundleList") {
      listbox = document.getElementById('bundleList')    
      for (var j=0; j < listbox.getRowCount(); j++) {
        listitem = listbox.getItemAtIndex(j);
        var line = "BundlePath" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
        fos.write(line, line.length);
      }
    } else if (elements[i].id == "certList") {
      listbox = document.getElementById('certList')    
      for (var j=0; j < listbox.getRowCount(); j++) {
        listitem = listbox.getItemAtIndex(j);
        var line = "CertPath" + (j+1) + "=" + listitem.getAttribute("label") + "\n";
        fos.write(line, line.length);
        var line = "CertTrust" + (j+1) + "=" + listitem.getAttribute("value") + "\n";
        fos.write(line, line.length);
      }
    }
  }
  fos.close();
}

function CCKReadConfigFile(srcdir)
{
  var file = srcdir.clone();
  file.append("cck.config");
  
  if (!file.exists()) {
    DoEnabling();
    toggleProxySettings();
    return;
  }
  
  var stream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                         .createInstance(Components.interfaces.nsIFileInputStream);

  stream.init(file, 0x01, 0644, 0);
  var lis = stream.QueryInterface(Components.interfaces.nsILineInputStream);
  var line = {value:null};
  
  configarray = new Array();
  do {
    var more = lis.readLine(line);
    var str = line.value;
    var equals = str.indexOf('=');
    if (equals != -1) {
      firstpart = str.substring(0,equals);
      secondpart = str.substring(equals+1);
      configarray[firstpart] = secondpart;
      try {
        (document.getElementById(firstpart).value = secondpart)
      } catch (ex) {}
    }
  } while (more);
  
  // handle prefs
  listbox = document.getElementById('prefList');
  listbox.clear();

  var i = 1;
  while( prefname = configarray['PreferenceName' + i]) {
    /* Old config file - figure out pref type */
    if (!(configarray['PreferenceType' + i])) {
      /* We're going to use this a lot */
      value = configarray['PreferenceValue' + i];
      if ((value.toLowerCase() == "true") || (value.toLowerCase() == "false")) {
        configarray['PreferenceType' + i] = "boolean";
        value = value.toLowerCase();
      } else if (parseInt(value) == value) {
        configarray['PreferenceType' + i] = "integer";
      } else {
        /* Remove opening and closing quotes if they exist */
        configarray['PreferenceType' + i] = "string";
        if (value.charAt(0) == '"')
          value = value.substring(1,value.length);
        if (value.charAt(value.length-1) == '"')
          if (value.charAt(value.length-2) != '\\')
            value = value.substring(0,value.length-1);
      }
      configarray['PreferenceValue' + i] = value;
    }
    if (configarray['PreferenceValue' + i]) {
      listitem = listbox.appendItem(prefname, configarray['PreferenceValue' + i]);
    } else {
      listitem = listbox.appendItem(prefname, "");
    }
    
    if (configarray['PreferenceLock' + i] == "true") {
      listitem.cck['lock'] = "true";
    } else {
      listitem.cck['lock'] = "";
    }
    listitem.cck['type'] = configarray['PreferenceType' + i];
    i++;
  }  

  // handle plugins
  listbox = document.getElementById('browserPluginList');
  listbox.clear();
  

  var i = 1;
  while( pluginname = configarray['BrowserPluginPath' + i]) {
    if (configarray['BrowserPluginType' + i]) {
      listbox.appendItem(pluginname, configarray['BrowserPluginType' + i]);
    } else {
      listbox.appendItem(pluginname, null);
    }
    i++;
  }  

  // handle toolbar folder with bookmarks
  listbox = document.getElementById('tbFolder.bookmarkList');
  listbox.clear();

  var i = 1;
  while( name = configarray['ToolbarFolder1.BookmarkTitle' + i]) {
    listitem = listbox.appendItem(name, configarray['ToolbarFolder1.BookmarkURL' + i]);
    listitem.setAttribute("class", "listitem-iconic");
    if (configarray['ToolbarFolder1.BookmarkType' + i] == "live") {
      listitem.cck['type'] = "live";
      listitem.setAttribute("image", "chrome://browser/skin/page-livemarks.png");
    } else {
      listitem.cck['type'] = "";
      listitem.setAttribute("image", "chrome://browser/skin/Bookmarks-folder.png");
    }
    i++;
  }
  // handle toolbar bookmarks
  listbox = document.getElementById('tb.bookmarkList');
  listbox.clear();

  var i = 1;
  while( name = configarray['ToolbarBookmarkTitle' + i]) {
    listitem = listbox.appendItem(name, configarray['ToolbarBookmarkURL' + i]);
    listitem.setAttribute("class", "listitem-iconic");
    if (configarray['ToolbarBookmarkType' + i] == "live") {
      listitem.cck['type'] = "live";
      listitem.setAttribute("image", "chrome://browser/skin/page-livemarks.png");
    } else {
      listitem.cck['type'] = "";
      listitem.setAttribute("image", "chrome://browser/skin/Bookmarks-folder.png");
    }
    i++;
  }  
  
  // handle folder with bookmarks
  listbox = document.getElementById('bmFolder.bookmarkList');
  listbox.clear();

  var i = 1;
  while( name = configarray['BookmarkFolder1.BookmarkTitle' + i]) {
    listitem = listbox.appendItem(name, configarray['BookmarkFolder1.BookmarkURL' + i]);
    listitem.setAttribute("class", "listitem-iconic");
    if (configarray['BookmarkFolder1.BookmarkType' + i] == "live") {
      listitem.cck['type'] = "live";
      listitem.setAttribute("image", "chrome://browser/skin/page-livemarks.png");
    } else {
      listitem.cck['type'] = "";
      listitem.setAttribute("image", "chrome://browser/skin/Bookmarks-folder.png");
    }
    i++;
  }
  // handle bookmarks
  listbox = document.getElementById('bm.bookmarkList');
  listbox.clear();

  var i = 1;
  while( name = configarray['BookmarkTitle' + i]) {
    listitem = listbox.appendItem(name, configarray['BookmarkURL' + i]);
    listitem.setAttribute("class", "listitem-iconic");
    if (configarray['BookmarkType' + i] == "live") {
      listitem.cck['type'] = "live";
      listitem.setAttribute("image", "chrome://browser/skin/page-livemarks.png");
    } else {
      listitem.cck['type'] = "";
      listitem.setAttribute("image", "chrome://browser/skin/Bookmarks-folder.png");
    }
    i++;
  }  


  
  
  // handle registry items
  listbox = document.getElementById('regList');
  listbox.clear();

  var i = 1;
  while( regname = configarray['RegName' + i]) {
    var listitem = listbox.appendItem(regname, "");
    listitem.cck['rootkey'] = configarray['RootKey' + i];
    listitem.cck['key'] = configarray['Key' + i];
    listitem.cck['name'] = configarray['Name' + i];
    listitem.cck['namevalue'] = configarray['NameValue' + i];
    listitem.cck['type'] = configarray['Type' + i];
    i++;
  }

  // cert list
  listbox = document.getElementById('certList');
  listbox.clear();

  var i = 1;
  while( certpath = configarray['CertPath' + i]) {
    var listitem;
    if (configarray['CertTrust' + i]) {
      listitem = listbox.appendItem(certpath, configarray['CertTrust' + i]);
    } else {
      listitem = listbox.appendItem(certpath, "C,C,C");
    }
    i++;
  }

  // bundle list
  listbox = document.getElementById('bundleList');
  listbox.clear();

  var i = 1;
  while( bundlepath = configarray['BundlePath' + i]) {
    var listitem = listbox.appendItem(bundlepath, "");
    i++;
  }  

  var sourcefile = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);

  // handle searchengines
  listbox = document.getElementById('searchEngineList');
  listbox.clear();

  /* I changed the name from SearchPlugin to SearchEngine. */
  /* This code is to support old config files */
  var searchname = "SearchEngine";
  if  (configarray['SearchPlugin1']) {
    searchname = "SearchPlugin";
  }
  
  var i = 1;
  while(searchenginename = configarray[searchname + i]) {
    listitem = listbox.appendItem(searchenginename, configarray[searchname + 'Icon' + i]);
    listitem.setAttribute("class", "listitem-iconic");
    try {
      sourcefile.initWithPath(configarray[searchname + 'Icon' + i]);
      var ioServ = Components.classes["@mozilla.org/network/io-service;1"]
                             .getService(Components.interfaces.nsIIOService);
      var imgfile = ioServ.newFileURI(sourcefile);
      listitem.setAttribute("image", imgfile.spec);
    } catch (e) {
    }
    i++;
  }

  var hidden = document.getElementById("hidden");
  hidden.checked = configarray["hidden"];

  var locked = document.getElementById("locked");
  locked.checked = configarray["locked"];
  
  var aboutconfig = document.getElementById("noaboutconfig");
  aboutconfig.checked = configarray["noaboutconfig"];


  var proxyitem = document.getElementById("shareAllProxies");
  proxyitem.checked = configarray["shareAllProxies"];
  
  var item = document.getElementById("ToolbarLocation");
  if (configarray["ToolbarLocation"]) {
    item.value = configarray["ToolbarLocation"];
  } else {
    item.value = "Last";
  }
  
  var item = document.getElementById("BookmarkLocation");
  if (configarray["BookmarkLocation"]) {
    item.value = configarray["BookmarkLocation"];
  } else {
    item.value = "Last";
  }
  
  
  
  DoEnabling();
  toggleProxySettings();
  
  stream.close();
}

function Validate(field, message)
{
  var gIDTest = /^(\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}|[a-z0-9-\._]*\@[a-z0-9-\._]+)$/i;
  
  for (var i=0; i < arguments.length; i+=2) {
    /* special case ID */
    if (document.getElementById(arguments[i]).id == "id") {
      if (!gIDTest.test(document.getElementById(arguments[i]).value)) {
        var bundle = document.getElementById("bundle_cckwizard");
        gPromptService.alert(window, bundle.getString("windowTitle"), arguments[i+1]);
        return false;
      }
    } else {
      if (document.getElementById(arguments[i]).value == '') {
        var bundle = document.getElementById("bundle_cckwizard");
        gPromptService.alert(window, bundle.getString("windowTitle"), arguments[i+1]);
        return false;
      }
    }
  }
  return true;
}

function ValidateNoSpace(field, message)
{
  for (var i=0; i < arguments.length; i+=2) {
    var str = document.getElementById(arguments[i]).value;
    if ((str == '') || (str.match(" "))) {
      var bundle = document.getElementById("bundle_cckwizard");
      gPromptService.alert(window, bundle.getString("windowTitle"), arguments[i+1]);
      return false;
    }
  }
  return true;
}


function ValidateFile()
{
  for (var i=0; i < arguments.length; i++) {
    var filename = document.getElementById(arguments[i]).value;
    if (filename.length > 0) {
      var file = Components.classes["@mozilla.org/file/local;1"]
                           .createInstance(Components.interfaces.nsILocalFile);
      try {
        file.initWithPath(filename);
      } catch (e) {
        gPromptService.alert(window, "", "File " + filename + " not found");
        return false;
      }
      if (!file.exists() || file.isDirectory()) {
        gPromptService.alert(window, "", "File " + filename + " not found");
        return false;
      }
    }
  }
  return true;
}

function ValidateDir()
{
  for (var i=0; i < arguments.length; i++) {
    var filename = document.getElementById(arguments[i]).value;
    if (filename.length > 0) {
      var file = Components.classes["@mozilla.org/file/local;1"]
                           .createInstance(Components.interfaces.nsILocalFile);
      try {
        file.initWithPath(filename);
      } catch (e) {
        gPromptService.alert(window, "", "Directory " + filename + " not found");
        return false;
      }
      if (!file.exists()) {
        var bundle = document.getElementById("bundle_cckwizard");
        var button = gPromptService.confirmEx(window, bundle.getString("windowTitle"), bundle.getString("createDir").replace(/%S/g, filename),
                                              gPromptService.BUTTON_TITLE_YES * gPromptService.BUTTON_POS_0 +
                                              gPromptService.BUTTON_TITLE_NO * gPromptService.BUTTON_POS_1,
                                              null, null, null, null, {});
        if (button == 0) {
          try {
            file.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
          } catch (ex) {
            gPromptService.alert(window, bundle.getString("windowTitle"),
                                 bundle.getString("createDirError").replace(/%S/g, filename));
            return false;
          }
        }
      } else if (!file.isDirectory()) {
        gPromptService.alert(window, "", "Directory " + filename + " not found");
        return false;
      }
    }
  }
  return true;
}

function toggleProxySettings()
{
  var http = document.getElementById("HTTPproxyname");
  var httpPort = document.getElementById("HTTPportno");
  var ftp = document.getElementById("FTPproxyname");
  var ftpPort = document.getElementById("FTPportno");
  var gopher = document.getElementById("Gopherproxyname");
  var gopherPort = document.getElementById("Gopherportno");
  var ssl = document.getElementById("SSLproxyname");
  var sslPort = document.getElementById("SSLportno");
  var socks = document.getElementById("SOCKShostname");
  var socksPort = document.getElementById("SOCKSportno");
  var socksVersion = document.getElementById("socksv");
  var socksVersion4 = document.getElementById("SOCKSVersion4");
  var socksVersion5 = document.getElementById("SOCKSVersion5");
  
  // arrays
  var urls = [ftp,gopher,ssl];
  var ports = [ftpPort,gopherPort,sslPort];
  var allFields = [ftp,gopher,ssl,ftpPort,gopherPort,sslPort,socks,socksPort,socksVersion,socksVersion4,socksVersion5];

  if ((document.getElementById("shareAllProxies").checked) || document.getElementById("ProxyType").value != "1") {
    for (i = 0; i < allFields.length; i++)
      allFields[i].setAttribute("disabled", "true");
  } else {
    for (i = 0; i < allFields.length; i++) {
      allFields[i].removeAttribute("disabled");
    }
  }
}

function DoEnabling()
{
  var i;
  var ftp = document.getElementById("FTPproxyname");
  var ftpPort = document.getElementById("FTPportno");
  var gopher = document.getElementById("Gopherproxyname");
  var gopherPort = document.getElementById("Gopherportno");
  var http = document.getElementById("HTTPproxyname");
  var httpPort = document.getElementById("HTTPportno");
  var socks = document.getElementById("SOCKShostname");
  var socksPort = document.getElementById("SOCKSportno");
  var socksVersion = document.getElementById("socksv");
  var socksVersion4 = document.getElementById("SOCKSVersion4");
  var socksVersion5 = document.getElementById("SOCKSVersion5");
  var ssl = document.getElementById("SSLproxyname");
  var sslPort = document.getElementById("SSLportno");
  var noProxy = document.getElementById("NoProxyname");
  var autoURL = document.getElementById("autoproxyurl");
  var shareAllProxies = document.getElementById("shareAllProxies");

  // convenience arrays
  var manual = [ftp, ftpPort, gopher, gopherPort, http, httpPort, socks, socksPort, socksVersion, socksVersion4, socksVersion5, ssl, sslPort, noProxy, shareAllProxies];
  var manual2 = [http, httpPort, noProxy, shareAllProxies];
  var auto = [autoURL];

  // radio buttons
  var radiogroup = document.getElementById("ProxyType");
  if (radiogroup.value == "")
    radiogroup.value = "0";

  switch ( radiogroup.value ) {
    case "0":
    case "4":
      for (i = 0; i < manual.length; i++)
        manual[i].setAttribute( "disabled", "true" );
      for (i = 0; i < auto.length; i++)
        auto[i].setAttribute( "disabled", "true" );
      break;
    case "1":
      for (i = 0; i < auto.length; i++)
        auto[i].setAttribute( "disabled", "true" );
      if (!radiogroup.disabled && !shareAllProxies.checked) {
        for (i = 0; i < manual.length; i++) {
           manual[i].removeAttribute( "disabled" );
        }
      } else {
        for (i = 0; i < manual.length; i++)
          manual[i].setAttribute("disabled", "true");
        for (i = 0; i < manual2.length; i++) {
          manual2[i].removeAttribute( "disabled" );
        }
      }
      break;
    case "2":
    default:
      for (i = 0; i < manual.length; i++)
        manual[i].setAttribute("disabled", "true");
      if (!radiogroup.disabled)
        for (i = 0; i < auto.length; i++)
          auto[i].removeAttribute("disabled");
      break;
  }
}

function htmlEscape(s)
{
  s=s.replace(/&/g,'&amp;');
  s=s.replace(/>/g,'&gt;');
  s=s.replace(/</g,'&lt;');
  s=s.replace(/"/g, '&quot;');
  return s;
}

