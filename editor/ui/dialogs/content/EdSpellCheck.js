/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *  
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 */

var MisspelledWord;
var spellChecker;
var allowSelectWord = true;
var PreviousReplaceWord = "";

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;
  dump("EditorShell found for Spell Checker dialog\n");

  // Get the spellChecker shell
  spellChecker = editorShell.QueryInterface(Components.interfaces.nsIEditorSpellCheck);
  if (!spellChecker) {
    dump("SpellChecker not found!!!\n");
    window.close();
  }

  // Create dialog object to store controls for easy access
  dialog = new Object;
  if (!dialog)
  {
    dump("Failed to create dialog object!!!\n");
    Close();
  }
  dialog.MisspelledWordLabel = document.getElementById("MisspelledWordLabel");
  dialog.MisspelledWord      = document.getElementById("MisspelledWord");
  dialog.ReplaceButton       = document.getElementById("Replace");
  dialog.IgnoreButton        = document.getElementById("Ignore");
  dialog.CloseButton         = document.getElementById("Close");
  dialog.ReplaceWordInput    = document.getElementById("ReplaceWord");
  dialog.SuggestedList       = document.getElementById("SuggestedList");
  dialog.LanguageMenulist    = document.getElementById("LanguageMenulist");

  if (!dialog.MisspelledWord ||
      !dialog.ReplaceWordInput ||
      !dialog.SuggestedList  ||
      !dialog.LanguageMenulist )
  {
    dump("Not all dialog controls were found!!!\n");
  }
  // NOTE: We shouldn't have been created if there was no misspelled word
  
  // The first misspelled word is passed as the 2nd extra parameter in window.openDialog()
  MisspelledWord = window.arguments[1];
  // Initial replace word is the misspelled word;
  dialog.ReplaceWordInput.value = MisspelledWord;
  PreviousReplaceWord = MisspelledWord;
  
  if (MisspelledWord.length > 0) {
    dump("First misspelled word = "+MisspelledWord+"\n");
    // Put word in the borderless button used to show the misspelled word
    dialog.MisspelledWord.setAttribute("value", MisspelledWord);
    // Get the list of suggested replacements
    FillSuggestedList();
  }

//dump("4: replace word="+dialog.ReplaceWordInput.value+"\n");
  if (dialog.LanguageMenulist)
  {
    // Fill in the language menulist and sync it up
    // with the spellchecker's current language.

    var curLang;

    try {
      curLang = spellChecker.GetCurrentDictionary();
    } catch(ex) {
      dump(ex);
      curLang = "";
    }

    InitLanguageMenu(curLang);
  }

  DoEnabling();

//dump("5: replace word="+dialog.ReplaceWordInput.value+"\n");
  SetTextfieldFocus(dialog.ReplaceWordInput);
//dump("End of Startup: replace word="+dialog.ReplaceWordInput.value+", misspelled word="+MisspelledWord+"\n");

  SetWindowLocation();
}

function InitLanguageMenu(curLang)
{
  ClearMenulist(dialog.LanguageMenulist);

  var o1 = {};
  var o2 = {};

  // Get the list of dictionaries from
  // the spellchecker.

  spellChecker.GetDictionaryList(o1, o2);

  var dictList = o1.value;
  var count    = o2.value;

  // Load the string bundle that will help us map
  // RFC 1766 strings to UI strings.

  var bundle = srGetStrBundle("resource:/res/acceptlanguage.properties"); 
  var menuStr;
  var defaultIndex = 0;

  for (var i = 0; i < dictList.length; i++)
  {
    try {
      menuStr = bundle.GetStringFromName(dictList[i]+".title");

      if (!menuStr)
        menuStr = dictList[i];

      if (curLang && dictList[i] == curLang)
        defaultIndex = i;
    } catch (ex) {
      // GetStringFromName throws an exception when
      // a key is not found in the bundle. In that
      // case, just use the original dictList string.

      menuStr = dictList[i];
    }

    AppendValueAndDataToMenulist(dialog.LanguageMenulist, menuStr, dictList[i]);
  }

  // Now make sure the correct item in the menu list is selected.

  if (dictList.length > 0)
    dialog.LanguageMenulist.selectedIndex = defaultIndex;
}

function DoEnabling()
{
  if (MisspelledWord.length == 0)
  {
    // No more misspelled words
    dialog.MisspelledWord.setAttribute("value",GetString("CheckSpellingDone"));
    
    dialog.ReplaceButton.removeAttribute("default");
    dialog.IgnoreButton.removeAttribute("default");
    dialog.CloseButton.setAttribute("default","true");

    SetElementEnabledById("MisspelledWordLabel", false);
    SetElementEnabledById("ReplaceWordLabel", false);
    SetElementEnabledById("ReplaceWord", false);
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
    SetElementEnabledById("CheckWord", true);
    SetElementEnabledById("SuggestedListLabel", true);
    SetElementEnabledById("SuggestedList", true);
    SetElementEnabledById("Ignore", true);
    SetElementEnabledById("IgnoreAll", true);
    SetElementEnabledById("AddToDictionary", true);

    dialog.CloseButton.removeAttribute("default");
    SetReplaceEnable();
  }
}

function NextWord()
{
  MisspelledWord = spellChecker.GetNextMisspelledWord();
  SetWidgetsForMisspelledWord();
}

function SetWidgetsForMisspelledWord()
{
  dialog.MisspelledWord.setAttribute("value",MisspelledWord);

  FillSuggestedList();

  // Initial replace word is misspelled word 
  dialog.ReplaceWordInput.value = MisspelledWord;
  PreviousReplaceWord = MisspelledWord;
  
  DoEnabling();
  
  if (MisspelledWord)
    SetTextfieldFocus(dialog.ReplaceWordInput);
}

function CheckWord()
{
  word = dialog.ReplaceWordInput.value;
  if (word != "") {
    //dump("CheckWord: Word in edit field="+word+"\n");
    isMisspelled = spellChecker.CheckCurrentWord(word);
    if (isMisspelled) {
      dump("CheckWord says word was misspelled\n");
      MisspelledWord = word;
      FillSuggestedList();
    } else {
      ClearTreelist(dialog.SuggestedList);
      var item = AppendStringToTreelistById(dialog.SuggestedList, "CorrectSpelling");
      if (item) item.setAttribute("disabled", "true");
      // Suppress being able to select the message text
      allowSelectWord = false;
    }
  }
}

function SelectSuggestedWord()
{
  if (allowSelectWord)
  {
    var index = dialog.SuggestedList.selectedIndex;
    if (index == -1)
    {
      dialog.ReplaceWordInput.value = PreviousReplaceWord;
    }
    else
    {
      var selValue = GetSelectedTreelistValue(dialog.SuggestedList);
      dialog.ReplaceWordInput.value = selValue;
    }
    SetReplaceEnable();
  }
}

function ChangeReplaceWord()
{
  // Calling this triggers SelectSuggestedWord(),
  //  so temporarily suppress the effect of that
  var saveAllow = allowSelectWord;
  allowSelectWord = false;

  // Unselect the word in the suggested list when user edits the replacement word
  dialog.SuggestedList.selectedIndex = -1;

  allowSelectWord = saveAllow;

  // Remember the new word
  PreviousReplaceWord = dialog.ReplaceWordInput.value;

  SetReplaceEnable();
}

function Ignore()
{
  NextWord();
}

function IgnoreAll()
{
  if (MisspelledWord != "") {
    spellChecker.IgnoreWordAllOccurrences(MisspelledWord);
  }
  NextWord();
}

function Replace()
{
  newWord = dialog.ReplaceWordInput.value;
  //dump("New = "+newWord+" Misspelled = "+MisspelledWord+"\n");
  if (MisspelledWord != "" && MisspelledWord != newWord)
  {
    editorShell.BeginBatchChanges();
    isMisspelled = spellChecker.ReplaceWord(MisspelledWord, newWord, false);
    editorShell.EndBatchChanges();
  }
  NextWord();
}

function ReplaceAll()
{
  dump("SpellCheck: ReplaceAll\n");
  newWord = dialog.ReplaceWordInput.value;
  if (MisspelledWord != "" && MisspelledWord != newWord)
  {
    editorShell.BeginBatchChanges();
    isMisspelled = spellChecker.ReplaceWord(MisspelledWord, newWord, true);
    editorShell.EndBatchChanges();
  }
  NextWord();
}

function AddToDictionary()
{
  if (MisspelledWord != "") {
    spellChecker.AddWordToDictionary(MisspelledWord);
  }
  NextWord();
}

function EditDictionary()
{
  window.openDialog("chrome://editor/content/EdDictionary.xul", "_blank", "chrome,close,titlebar,modal", "", MisspelledWord);
}

function SelectLanguage()
{
  var item = dialog.LanguageMenulist.selectedItem;

  try {
    spellChecker.SetCurrentDictionary(item.data);
  } catch (ex) {
    dump(ex);
  }
}

function Recheck()
{
  //TODO: Should we bother to add a "Recheck" method to interface?
  try {
    var curLang = spellChecker.GetCurrentDictionary();

    spellChecker.UninitSpellChecker();
    spellChecker.InitSpellChecker();
    spellChecker.SetCurrentDictionary(curLang);
    MisspelledWord = spellChecker.GetNextMisspelledWord();
    SetWidgetsForMisspelledWord();
  } catch(ex) {
    dump(ex);
  }
}

function FillSuggestedList()
{
  list = dialog.SuggestedList;

  // Clear the current contents of the list
  allowSelectWord = false;
  ClearTreelist(list);

//dump("PreviousReplaceWord="+PreviousReplaceWord+"\n");
  if (MisspelledWord.length > 0)
  {
    // Get suggested words until an empty string is returned
    var count = 0;
    do {
      word = spellChecker.GetSuggestedWord();
      dump("Suggested Word="+word+"|\n");
      if (word.length > 0) {
        AppendStringToTreelist(list, word);
        count++;
      }
    } while (word.length > 0);
    
    var len = list.getAttribute("length");

    if (count == 0) {
      // No suggestions - show a message but don't let user select it
      var item = AppendStringToTreelistById(list, "NoSuggestedWords");
      if (item) item.setAttribute("disabled", "true");
      allowSelectWord = false;
    } else {
      allowSelectWord = true;
    }
  }
  SetReplaceEnable();
}

function SetReplaceEnable()
{
  // Enable "Change..." buttons only if new word is different than misspelled
  var newWord = dialog.ReplaceWordInput.value;
//dump("SetReplaceEnabled: newWord ="+newWord+"\n");
  var enable = newWord.length > 0 && newWord != MisspelledWord;
  SetElementEnabledById("Replace", enable);
  SetElementEnabledById("ReplaceAll", enable);
  if (enable)
  {
    dialog.ReplaceButton.setAttribute("default","true");
    dialog.IgnoreButton.removeAttribute("default");
  }
  else
  {
    dialog.IgnoreButton.setAttribute("default","true");
    dialog.ReplaceButton.removeAttribute("default");
  }
}

function doDefault()
{
  if (dialog.ReplaceButton.getAttribute("default") == "true")
    Replace();
  else if (dialog.IgnoreButton.getAttribute("default") == "true")
    Ignore();
  else if (dialog.CloseButton.getAttribute("default") == "true")
    onClose();
}

function onClose()
{
  // Shutdown the spell check and close the dialog
  spellChecker.UninitSpellChecker();
  SaveWindowLocation();
  window.close();
}

