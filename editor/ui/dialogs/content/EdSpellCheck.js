/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Charles Manske (cmanske@netscape.com)
 *   Neil Rashbrook (neil@parkwaycc.co.uk)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var gMisspelledWord;
var gSpellChecker = null;
var gAllowSelectWord = true;
var gPreviousReplaceWord = "";
var gFirstTime = true;
var gLastSelectedLang = null;
var gDictCount = 0;

function Startup()
{
  var sendMailMessageMode = false;

  var editor = GetCurrentEditor();
  if (!editor)
  {
    window.close();
    return;
  }

  // Get the spellChecker shell
  gSpellChecker = Components.classes['@mozilla.org/editor/editorspellchecker;1'].createInstance(Components.interfaces.nsIEditorSpellCheck);
  if (!gSpellChecker)
  {
    dump("SpellChecker not found!!!\n");
    window.close();
    return;
  }

  // Start the spell checker module.
  try {
    // TxtSrv Filter Contract Id
    var filterContractId;
    sendMailMessageMode = window.arguments[0];
    var skipBlockQuotes = window.arguments[1];
    var enableSelectionChecking = window.arguments[2];

    if (skipBlockQuotes)
      filterContractId = "@mozilla.org/editor/txtsrvfiltermail;1";
    else
      filterContractId = "@mozilla.org/editor/txtsrvfilter;1";

    gSpellChecker.setFilter(Components.classes[filterContractId].createInstance(Components.interfaces.nsITextServicesFilter));
    gSpellChecker.InitSpellChecker(editor, enableSelectionChecking);

  }
  catch(ex) {
   dump("*** Exception error: InitSpellChecker\n");
    window.close();
    return;
  }

  gDialog.MisspelledWordLabel = document.getElementById("MisspelledWordLabel");
  gDialog.MisspelledWord      = document.getElementById("MisspelledWord");
  gDialog.ReplaceButton       = document.getElementById("Replace");
  gDialog.IgnoreButton        = document.getElementById("Ignore");
  gDialog.StopButton          = document.getElementById("Stop");
  gDialog.CloseButton         = document.getElementById("Close");
  gDialog.ReplaceWordInput    = document.getElementById("ReplaceWordInput");
  gDialog.SuggestedList       = document.getElementById("SuggestedList");
  gDialog.LanguageMenulist    = document.getElementById("LanguageMenulist");

  // Fill in the language menulist and sync it up
  // with the spellchecker's current language.

  var curLang;

  try {
    curLang = gSpellChecker.GetCurrentDictionary();
  } catch(ex) {
    curLang = "";
  }

  InitLanguageMenu(curLang);
  
  // Get the first misspelled word and setup all UI
  NextWord();

  // When startup param is true, setup different UI when spell checking 
  //   just before sending mail message  
  if (sendMailMessageMode)
  {
    // If no misspelled words found, simply close dialog and send message
    if (!gMisspelledWord)
    {
      onClose();
      return;
    }

    // Hide "Close" button and use "Send" instead
    gDialog.CloseButton.hidden = true;
    gDialog.CloseButton = document.getElementById("Send");
    gDialog.CloseButton.hidden = false;
  }
  else
  {
    // Normal spell checking - hide the "Stop" button
    // (Note that this button is the "Cancel" button for
    //  Esc keybinding and related window close actions)
    gDialog.StopButton.hidden = true;
  }

  // Clear flag that determines message when
  //  no misspelled word is found
  //  (different message when used for the first time)
  gFirstTime = false;
}

function InitLanguageMenu(aCurLang)
{

  var o1 = {};
  var o2 = {};

  // Get the list of dictionaries from
  // the spellchecker.

  try
  {
    gSpellChecker.GetDictionaryList(o1, o2);
  }
  catch(ex)
  {
    dump("Failed to get DictionaryList!\n");
    return;
  }

  var dictList = o1.value;
  var count    = o2.value;

  // If we're not just starting up and dictionary count
  // hasn't changed then no need to update the menu.
  if (gDictCount == count)
    return;

  // Store current dictionary count.
  gDictCount = count;

  // Load the string bundles that will help us map
  // RFC 1766 strings to UI strings.

  // Load the language string bundle.
  var languageBundle = document.getElementById("languageBundle");
  var regionBundle = null;
  // If we have a language string bundle, load the region string bundle.
  if (languageBundle)
    regionBundle = document.getElementById("regionBundle");
  
  var menuStr2;
  var isoStrArray;
  var langId;
  var langLabel;
  var i;

  for (i = 0; i < count; i++)
  {
    try
    {
      langId = dictList[i];
      isoStrArray = dictList[i].split("-");

      if (languageBundle && isoStrArray[0])
        langLabel = languageBundle.getString(isoStrArray[0].toLowerCase());

      if (regionBundle && langLabel && isoStrArray.length > 1 && isoStrArray[1])
      {
        menuStr2 = regionBundle.getString(isoStrArray[1].toLowerCase());
        if (menuStr2)
          langLabel += "/" + menuStr2;
      }

      if (langLabel && isoStrArray.length > 2 && isoStrArray[2])
        langLabel += " (" + isoStrArray[2] + ")";

      if (!langLabel)
        langLabel = langId;
    }
    catch (ex)
    {
      // getString throws an exception when a key is not found in the
      // bundle. In that case, just use the original dictList string.
      langLabel = langId;
    }
    dictList[i] = [langLabel, langId];
  }
  
  // sort by locale-aware collation
  dictList.sort(
    function compareFn(a, b)
    {
      return a[0].localeCompare(b[0]);
    }
  );

  // Remove any languages from the list.
  var languageMenuPopup = gDialog.LanguageMenulist.firstChild;
  while (languageMenuPopup.firstChild.localName != "menuseparator")
    languageMenuPopup.removeChild(languageMenuPopup.firstChild);

  var defaultItem = null;

  for (i = 0; i < count; i++)
  {
    var item = gDialog.LanguageMenulist.insertItemAt(i, dictList[i][0], dictList[i][1]);
    if (aCurLang && dictList[i][1] == aCurLang)
      defaultItem = item;
  }

  // Now make sure the correct item in the menu list is selected.
  if (defaultItem)
  {
    gDialog.LanguageMenulist.selectedItem = defaultItem;
    gLastSelectedLang = defaultItem;
  }
}

function DoEnabling()
{
  if (!gMisspelledWord)
  {
    // No more misspelled words
    gDialog.MisspelledWord.setAttribute("value",GetString( gFirstTime ? "NoMisspelledWord" : "CheckSpellingDone"));

    gDialog.ReplaceButton.removeAttribute("default");
    gDialog.IgnoreButton.removeAttribute("default");

    gDialog.CloseButton.setAttribute("default","true");
    // Shouldn't have to do this if "default" is true?
    gDialog.CloseButton.focus();

    SetElementEnabledById("MisspelledWordLabel", false);
    SetElementEnabledById("ReplaceWordLabel", false);
    SetElementEnabledById("ReplaceWordInput", false);
    SetElementEnabledById("CheckWord", false);
    SetElementEnabledById("SuggestedListLabel", false);
    SetElementEnabledById("SuggestedList", false);
    SetElementEnabledById("Ignore", false);
    SetElementEnabledById("IgnoreAll", false);
    SetElementEnabledById("Replace", false);
    SetElementEnabledById("ReplaceAll", false);
    SetElementEnabledById("AddToDictionary", false);
  } else {
    SetElementEnabledById("MisspelledWordLabel", true);
    SetElementEnabledById("ReplaceWordLabel", true);
    SetElementEnabledById("ReplaceWordInput", true);
    SetElementEnabledById("CheckWord", true);
    SetElementEnabledById("SuggestedListLabel", true);
    SetElementEnabledById("SuggestedList", true);
    SetElementEnabledById("Ignore", true);
    SetElementEnabledById("IgnoreAll", true);
    SetElementEnabledById("AddToDictionary", true);

    gDialog.CloseButton.removeAttribute("default");
    SetReplaceEnable();
  }
}

function NextWord()
{
  gMisspelledWord = gSpellChecker.GetNextMisspelledWord();
  SetWidgetsForMisspelledWord();
}

function SetWidgetsForMisspelledWord()
{
  gDialog.MisspelledWord.setAttribute("value", TruncateStringAtWordEnd(gMisspelledWord, 30, true));


  // Initial replace word is misspelled word
  gDialog.ReplaceWordInput.value = gMisspelledWord;
  gPreviousReplaceWord = gMisspelledWord;

  // This sets gDialog.ReplaceWordInput to first suggested word in list
  FillSuggestedList(gMisspelledWord);

  DoEnabling();

  if (gMisspelledWord)
    SetTextboxFocus(gDialog.ReplaceWordInput);
}

function CheckWord()
{
  var word = gDialog.ReplaceWordInput.value;
  if (word) 
  {
    if (gSpellChecker.CheckCurrentWord(word))
    {
      FillSuggestedList(word);
      SetReplaceEnable();
    } 
    else 
    {
      ClearListbox(gDialog.SuggestedList);
      var item = gDialog.SuggestedList.appendItem(GetString("CorrectSpelling"), "");
      if (item) item.setAttribute("disabled", "true");
      // Suppress being able to select the message text
      gAllowSelectWord = false;
    }
  }
}

function SelectSuggestedWord()
{
  if (gAllowSelectWord)
  {
    var selectedItem
    if (gDialog.SuggestedList.selectedItem)
    {
      var selValue = gDialog.SuggestedList.selectedItem.getAttribute("label");
      gDialog.ReplaceWordInput.value = selValue;
      gPreviousReplaceWord = selValue;
    }
    else
    {
      gDialog.ReplaceWordInput.value = gPreviousReplaceWord;
    }
    SetReplaceEnable();
  }
}

function ChangeReplaceWord()
{
  // Calling this triggers SelectSuggestedWord(),
  //  so temporarily suppress the effect of that
  var saveAllow = gAllowSelectWord;
  gAllowSelectWord = false;

  // Select matching word in list
  var newIndex = -1;
  var newSelectedItem;
  var replaceWord = TrimString(gDialog.ReplaceWordInput.value);
  if (replaceWord)
  {
    for (var i = 0; i < gDialog.SuggestedList.getRowCount(); i++)
    {
      var item = gDialog.SuggestedList.getItemAtIndex(i);
      if (item.getAttribute("label") == replaceWord)
      {
        newSelectedItem = item;
        break;
      }
    }
  }
  gDialog.SuggestedList.selectedItem = newSelectedItem;

  gAllowSelectWord = saveAllow;

  // Remember the new word
  gPreviousReplaceWord = gDialog.ReplaceWordInput.value;

  SetReplaceEnable();
}

function Ignore()
{
  NextWord();
}

function IgnoreAll()
{
  if (gMisspelledWord) {
    gSpellChecker.IgnoreWordAllOccurrences(gMisspelledWord);
  }
  NextWord();
}

function Replace(newWord)
{
  if (!newWord)
    return;

  if (gMisspelledWord && gMisspelledWord != newWord)
  {
    var editor = GetCurrentEditor();
    editor.beginTransaction();
    try {
      gSpellChecker.ReplaceWord(gMisspelledWord, newWord, false);
    } catch (e) {}
    editor.endTransaction();
  }
  NextWord();
}

function ReplaceAll()
{
  var newWord = gDialog.ReplaceWordInput.value;
  if (gMisspelledWord && gMisspelledWord != newWord)
  {
    var editor = GetCurrentEditor();
    editor.beginTransaction();
    try {
      gSpellChecker.ReplaceWord(gMisspelledWord, newWord, true);
    } catch (e) {}
    editor.endTransaction();
  }
  NextWord();
}

function AddToDictionary()
{
  if (gMisspelledWord) {
    gSpellChecker.AddWordToDictionary(gMisspelledWord);
  }
  NextWord();
}

function EditDictionary()
{
  window.openDialog("chrome://editor/content/EdDictionary.xul", "_blank", "chrome,close,titlebar,modal", "", gMisspelledWord);
}

function SelectLanguage()
{
  try {
    var item = gDialog.LanguageMenulist.selectedItem;
    if (item.value != "more-cmd") {
      gSpellChecker.SetCurrentDictionary(item.value);
      gLastSelectedLang = item;
    }
    else {
      var dictionaryUrl = getDictionaryURL();
                      
      var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                                .getService(Components.interfaces.nsIIOService);
      uri = ioService.newURI(dictionaryUrl, null, null);
      var protocolSvc = Components.classes["@mozilla.org/uriloader/external-protocol-service;1"]
                                  .getService(Components.interfaces.nsIExternalProtocolService);
      if (protocolSvc.isExposedProtocol(uri.scheme))
        opener.openDialog(getBrowserURL(), "_blank", "all,dialog=no", dictionaryUrl);
      else
        protocolSvc.loadUrl(uri);

      if (gLastSelectedLang)
        gDialog.LanguageMenulist.selectedItem = gLastSelectedLang;
    }
  } catch (ex) {
    dump(ex);
  }
}

function getDictionaryURL()
{
  var formatter = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"]
                  .getService(Components.interfaces.nsIURLFormatter);
                  
  return formatter.formatURLPref("spellchecker.dictionaries.download.url");
}

function Recheck()
{
  //TODO: Should we bother to add a "Recheck" method to interface?
  try {
    var curLang = gSpellChecker.GetCurrentDictionary();
    gSpellChecker.UninitSpellChecker();
    gSpellChecker.InitSpellChecker(GetCurrentEditor(), false);
    gSpellChecker.SetCurrentDictionary(curLang);
    gMisspelledWord = gSpellChecker.GetNextMisspelledWord();
    SetWidgetsForMisspelledWord();
  } catch(ex) {
    dump(ex);
  }
}

function FillSuggestedList(misspelledWord)
{
  var list = gDialog.SuggestedList;

  // Clear the current contents of the list
  gAllowSelectWord = false;
  ClearListbox(list);
  var item;

  if (misspelledWord.length > 0)
  {
    // Get suggested words until an empty string is returned
    var count = 0;
    var firstWord = 0;
    do {
      var word = gSpellChecker.GetSuggestedWord();
      if (count==0)
        firstWord = word;
      if (word.length > 0)
      {
        list.appendItem(word, "");
        count++;
      }
    } while (word.length > 0);

    if (count == 0)
    {
      // No suggestions - show a message but don't let user select it
      item = list.appendItem(GetString("NoSuggestedWords"));
      if (item) item.setAttribute("disabled", "true");
      gAllowSelectWord = false;
    } else {
      gAllowSelectWord = true;
      // Initialize with first suggested list by selecting it
      gDialog.SuggestedList.selectedIndex = 0;
    }
  } 
  else
  {
    item = list.appendItem("", "");
    if (item)
      item.setAttribute("disabled", "true");
  }
}

function SetReplaceEnable()
{
  // Enable "Change..." buttons only if new word is different than misspelled
  var newWord = gDialog.ReplaceWordInput.value;
  var enable = newWord.length > 0 && newWord != gMisspelledWord;
  SetElementEnabledById("Replace", enable);
  SetElementEnabledById("ReplaceAll", enable);
  if (enable)
  {
    gDialog.ReplaceButton.setAttribute("default","true");
    gDialog.IgnoreButton.removeAttribute("default");
  }
  else
  {
    gDialog.IgnoreButton.setAttribute("default","true");
    gDialog.ReplaceButton.removeAttribute("default");
  }
}

function doDefault()
{
  if (gDialog.ReplaceButton.getAttribute("default") == "true")
    Replace();
  else if (gDialog.IgnoreButton.getAttribute("default") == "true")
    Ignore();
  else if (gDialog.CloseButton.getAttribute("default") == "true")
    onClose();

  return false;
}

function ExitSpellChecker()
{
  if (gSpellChecker)
  {
    try
    {
      var curLang = gSpellChecker.GetCurrentDictionary();
      gSpellChecker.UninitSpellChecker();
      if ("@mozilla.org/spellchecker;1" in Components.classes) {
        var spellChecker = Components.classes["@mozilla.org/spellchecker/myspell;1"]
                                     .getService(Components.interfaces.mozISpellCheckingEngine);
        spellChecker.dictionary = curLang;
      }
      // now check the document over again with the new dictionary
      // if we have an inline spellchecker
      if (("InlineSpellCheckerUI" in window.opener) &&
          window.opener.InlineSpellCheckerUI.enabled)
        window.opener.InlineSpellCheckerUI.mInlineSpellChecker.spellCheckRange(null);
    }
    finally
    {
      gSpellChecker = null;
    }
  }
}

function CancelSpellCheck()
{
  ExitSpellChecker();

  // Signal to calling window that we canceled
  window.opener.cancelSendMessage = true;
  return true;
}

function onClose()
{
  ExitSpellChecker();

  window.opener.cancelSendMessage = false;
  window.close();
}
