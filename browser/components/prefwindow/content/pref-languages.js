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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adrian Havill <havill@redhat.com>
 *   Steffen Wilberg <steffen.wilberg@web.de>
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

//GLOBALS

//locale bundles
var gRegionsBundle;
var gLanguagesBundle;
var gAcceptedBundle;
var gPrefLangBundle;

//dictionary of all supported locales
var gAvailLanguageDict;

//XUL listbox handles
var gAvailableLanguages;
var gActiveLanguages;

// label and accesskey of the available_languages menulist
var gSelectLabel;
var gSelectAccesskey;

//XUL window pref window interface object
var gPrefString = new String();

//Reg expression for splitting multiple pref values
var gSeparatorRe = /\s*,\s*/;

// PrefWindow
var gPrefWindow;


function Init()
{
  gRegionsBundle   = document.getElementById("bundle_regions");
  gLanguagesBundle = document.getElementById("bundle_languages");
  gPrefLangBundle  = document.getElementById("bundle_prefLang");
  gAcceptedBundle  = document.getElementById("bundle_accepted");
  ReadAvailableLanguages();

  try {
    window.opener.top.initPanel(window.location.href, window);
  }
  catch(ex) { } //pref service backup

  gActiveLanguages = document.getElementById("active_languages");
  gPrefString = gActiveLanguages.getAttribute("prefvalue");
  LoadActiveLanguages();

  gPrefWindow = window.opener.parent.hPrefWindow;
  gAvailableLanguages = document.getElementById("available_languages");
  SelectLanguage();

  LoadAvailableLanguages();
  gSelectLabel = gAvailableLanguages.getAttribute("label");
  gSelectAccesskey = gAvailableLanguages.getAttribute("accesskey");
}


function onLanguagesDialogOK()
{
  gPrefWindow.wsm.savePageData(window.location.href, window);
  return true;
}


function ReadAvailableLanguages()
{
  gAvailLanguageDict   = new Array();
  var visible = new String();
  var str = new String();
  var i =0;

  var gAcceptedBundleEnum = gAcceptedBundle.stringBundle.getSimpleEnumeration();

  var curItem;
  var stringName;
  var stringNameProperty;

  while (gAcceptedBundleEnum.hasMoreElements()) {

    //progress through the bundle
    curItem = gAcceptedBundleEnum.getNext();

    //"unpack" the item, nsIPropertyElement is now partially scriptable
    curItem = curItem.QueryInterface(Components.interfaces.nsIPropertyElement);

    //dump string name (key)
    stringName = curItem.key;
    stringNameProperty = stringName.split(".");

    if (stringNameProperty[1] == "accept") {

      //dump the UI string (value)
      visible   = curItem.value;

      //if (visible == "true") {

        str = stringNameProperty[0];
        var stringLangRegion = stringNameProperty[0].split("-");

        if (stringLangRegion[0]) {
          var tit;
          var language;
          var region;
          var use_region_format = false;

          try {
            language = gLanguagesBundle.getString(stringLangRegion[0]);
          }
          catch (ex) {
            language = "";
          }

          if (stringLangRegion.length > 1) {

            try {
              region = gRegionsBundle.getString(stringLangRegion[1]);
              use_region_format = true;
            }
            catch (ex) { }
          }

          if (use_region_format) {
            tit = gPrefLangBundle.stringBundle.formatStringFromName("languageRegionCodeFormat",
                                                                    [language, region, str], 3);
          } else {
            tit = gPrefLangBundle.stringBundle.formatStringFromName("languageCodeFormat",
                                                                    [language, str], 2);
          }

        } //if language

        if (str && tit) {

          gAvailLanguageDict[i] = new Array(3);
          gAvailLanguageDict[i][0] = tit;
          gAvailLanguageDict[i][1] = str;
          gAvailLanguageDict[i][2] = visible;
          i++;

        } // if str && tit
      //} //if visible
    } //if accepted
  } //while
  gAvailLanguageDict.sort( // sort on first element
    function (a, b) {
      if (a[0] < b[0]) return -1;
      if (a[0] > b[0]) return  1;
      return 0;
    });
} //ReadAvailableLanguages


function LoadAvailableLanguages()
{
  if (gAvailLanguageDict)
    for (var i = 0; i < gAvailLanguageDict.length; i++)
      if (gAvailLanguageDict[i][2] == "true")
        AddMenuOrListItem(document, gAvailableLanguages.menupopup, "menuitem", gAvailLanguageDict[i][1], gAvailLanguageDict[i][0]);
}


function LoadActiveLanguages()
{
  if (gPrefString) {
    var arrayOfPrefs = gPrefString.split(gSeparatorRe);

    for (var i = 0; i < arrayOfPrefs.length; i++) {
      var str = arrayOfPrefs[i];
      var tit = GetLanguageTitle(str);

      if (str) {
        if (!tit)
           tit = "[" + str + "]";
        AddMenuOrListItem(document, gActiveLanguages, "listitem", str, tit);
      } //if
    } //for
  } //if
}


function LangAlreadyActive(langId)
{
  var found = false;
  try {
    var arrayOfPrefs = gPrefString.split(gSeparatorRe);

    if (arrayOfPrefs)
      for (var i = 0; i < arrayOfPrefs.length; i++) {
      if (arrayOfPrefs[i] == langId) {
        found = true;
        break;
      }
    }

    return found;
  }

  catch(ex){
    return false;
  }
} //LangAlreadyActive


function SelectAvailableLanguage()
{
    var selItem  = gAvailableLanguages.selectedItem;
    var languageId   = selItem.getAttribute("id");
    var addButton = document.getElementById("add");

    // since we're not displaying "select" anymore, don't underline some random "s"
    gAvailableLanguages.removeAttribute("accesskey");

    // if the langauge is not already active, activate the "add" button
    if (!LangAlreadyActive(languageId))
      addButton.disabled = false;
    else
      addButton.disabled = true;
}

function AddAvailableLanguage()
{
  var selItem  = gAvailableLanguages.selectedItem;
  var languageId   = selItem.getAttribute("id");
  var languageName = selItem.getAttribute("label");
  var addButton = document.getElementById("add");

  if (!LangAlreadyActive(languageId))
    AddMenuOrListItem(document, gActiveLanguages, "listitem", languageId, languageName);

  // restore the "select" label and accesskey, disable the "add" button again
  gAvailableLanguages.setAttribute("label", gSelectLabel);
  gAvailableLanguages.setAttribute("accesskey", gSelectAccesskey);
  addButton.disabled = true;

  // select the item we just added in the active_languages listbox
  var lastItem = gActiveLanguages.lastChild;
  gActiveLanguages.selectItem(lastItem);
  gActiveLanguages.ensureElementIsVisible(lastItem);

  UpdateSavePrefString();
}


function RemoveActiveLanguage()
{
  var nextNode = null;
  var numSelected = gActiveLanguages.selectedItems.length;
  var deleted_all = false;

  while (gActiveLanguages.selectedItems.length > 0) {
    var selectedNode = gActiveLanguages.selectedItems[0];

    nextNode = selectedNode.nextSibling;
    if (!nextNode && selectedNode.previousSibling)
      nextNode = selectedNode.previousSibling;

    gActiveLanguages.removeChild(selectedNode);
  } //while

  if (nextNode)
    gActiveLanguages.selectItem(nextNode)

  UpdateSavePrefString();
}


function GetLanguageTitle(id)
{

  if (gAvailLanguageDict)
    for (var j = 0; j < gAvailLanguageDict.length; j++) {

      if ( gAvailLanguageDict[j][1] == id) {
        //title =
        return gAvailLanguageDict[j][0];
      }
    }
  return "";
}


function AddMenuOrListItem(doc, listbox, type, langID, langTitle)
{
  try {  //let's beef up our error handling for languages without label / title

    // Create a listitem for the new Language
    var item = doc.createElement(type);

    // Copy over the attributes
    item.setAttribute("label", langTitle);
    item.setAttribute("id", langID);
    listbox.appendChild(item);

  }
  catch (ex) { }
}


function UpdateSavePrefString()
{
  var num_languages = 0;
  gPrefString = "";

  for (var item = gActiveLanguages.firstChild; item != null; item = item.nextSibling) {

    var languageId = item.getAttribute("id");

    if (languageId.length > 1) {
      num_languages++;

      //separate >1 languages by commas
      if (num_languages > 1) {
        gPrefString = gPrefString + "," + " " + languageId;
      } else {
        gPrefString = languageId;
      } //if
    } //if
  }//for

  gActiveLanguages.setAttribute("prefvalue", gPrefString);
}


function MoveUp() {

  if (gActiveLanguages.selectedItems.length == 1) {
    var selected = gActiveLanguages.selectedItems[0];
    var before = selected.previousSibling
    if (before) {
      before.parentNode.insertBefore(selected, before);
      gActiveLanguages.selectItem(selected);
      gActiveLanguages.ensureElementIsVisible(selected);
    }
  }

  if (gActiveLanguages.selectedIndex == 0)
  {
    // selected item is first
    var moveUp = document.getElementById("up");
    moveUp.disabled = true;
  }

  if (gActiveLanguages.childNodes.length > 1)
  {
    // more than one item so we can move selected item back down
    var moveDown = document.getElementById("down");
    moveDown.disabled = false;
  }

  UpdateSavePrefString();
} //MoveUp


function MoveDown() {

  if (gActiveLanguages.selectedItems.length == 1) {
    var selected = gActiveLanguages.selectedItems[0];
    if (selected.nextSibling) {
      if (selected.nextSibling.nextSibling) {
        gActiveLanguages.insertBefore(selected, selected.nextSibling.nextSibling);
      }
      else {
        gActiveLanguages.appendChild(selected);
      }
      gActiveLanguages.selectItem(selected);
    }
  }

  if (gActiveLanguages.selectedIndex == gActiveLanguages.childNodes.length - 1)
  {
    // selected item is last
    var moveDown = document.getElementById("down");
    moveDown.disabled = true;
  }

  if (gActiveLanguages.childNodes.length > 1)
  {
    // more than one item so we can move selected item back up
    var moveUp = document.getElementById("up");
    moveUp.disabled = false;
  }

  UpdateSavePrefString();

} //MoveDown


function SelectLanguage() {
  if (gActiveLanguages.selectedItems.length) {
    document.getElementById("remove").disabled = false;

    var selected = gActiveLanguages.selectedItems[0];
    document.getElementById("down").disabled = !selected.nextSibling;
    document.getElementById("up").disabled = !selected.previousSibling;
  }
  else {
    document.getElementById("remove").disabled = true;
    document.getElementById("down").disabled = true;
    document.getElementById("up").disabled = true;
  }

  if (gPrefWindow.getPrefIsLocked(document.getElementById("up").getAttribute("prefstring")))
    document.getElementById("up").disabled = true;
  if (gPrefWindow.getPrefIsLocked(document.getElementById("down").getAttribute("prefstring")))
    document.getElementById("down").disabled = true;
  if (gPrefWindow.getPrefIsLocked(document.getElementById("remove").getAttribute("prefstring")))
    document.getElementById("remove").disabled = true;
  if (gPrefWindow.getPrefIsLocked(gAvailableLanguages.getAttribute("prefstring")))
    gAvailableLanguages.disabled = true; // the "add" button just stays disabled
} // SelectLanguage
