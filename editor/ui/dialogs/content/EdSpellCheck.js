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
  // Save as a property of the window so it can be used by child dialogs
  window.spellChecker = spellChecker;

  // Create dialog object to store controls for easy access
  dialog = new Object;
  if (!dialog)
  {
    dump("Failed to create dialog object!!!\n");
    window.close();
  }
  dialog.MisspelledWordLabel = document.getElementById("MisspelledWordLabel");
  dialog.MisspelledWord = document.getElementById("MisspelledWord");
  dialog.ReplaceWordInput = document.getElementById("ReplaceWord");
  dialog.SuggestedList = document.getElementById("SuggestedList");
  dialog.LanguageList = document.getElementById("LanguageList");

  if (!dialog.MisspelledWord ||
      !dialog.ReplaceWordInput ||
      !dialog.SuggestedList  ||
      !dialog.LanguageList )
  {
    dump("Not all dialog controls were found!!!\n");
  }
  // NOTE: We shouldn't have been created if there was no misspelled word
  
  // The first misspelled word is passed as the 2nd extra parameter in window.openDialog()
  MisspelledWord = window.arguments[1];
  
  if (MisspelledWord.length > 0) {
    dump("First misspelled word = "+MisspelledWord+"\n");
    // Put word in the borderless button used to show the misspelled word
    dialog.MisspelledWord.setAttribute("value", MisspelledWord);
    // Get the list of suggested replacements
    FillSuggestedList();
  }
  // Initial replace word is the misspelled word;
  dialog.ReplaceWordInput.value = MisspelledWord;

  //Use English for now TODO: Kin needs to finish this work so we can fill in list
  dialog.LanguageList.selectedIndex = 0;
  dump("Language Listed Index = "+dialog.LanguageList.selectedIndex+"\n");

  DoEnabling();

  dialog.SuggestedList.focus();  
}

function DoEnabling()
{
  if (MisspelledWord.length == 0)
  {
    dialog.MisspelledWordLabel.setAttribute("value",GetString("CheckSpellingDone"));
    
    SetElementEnabledByID("MisspelledWord", false);
    SetElementEnabledByID("ReplaceWordLabel", false);
    SetElementEnabledByID("ReplaceWord", false);
    SetElementEnabledByID("CheckWord", false);
    SetElementEnabledByID("SuggestedListLabel", false);
    SetElementEnabledByID("SuggestedList", false);
    SetElementEnabledByID("Ignore", false);
    SetElementEnabledByID("IgnoreAll", false);
    SetElementEnabledByID("Replace", false);
    SetElementEnabledByID("ReplaceAll", false);
    SetElementEnabledByID("AddToDictionary", false);
  } else {
    dialog.MisspelledWordLabel.setAttribute("value",GetString("MisspelledWordLabel"));

    SetElementEnabledByID("MisspelledWord", true);
    SetElementEnabledByID("ReplaceWordLabel", true);
    SetElementEnabledByID("ReplaceWord", true);
    SetElementEnabledByID("CheckWord", true);
    SetElementEnabledByID("SuggestedListLabel", true);
    SetElementEnabledByID("SuggestedList", true);
    SetElementEnabledByID("Ignore", true);
    SetElementEnabledByID("IgnoreAll", true);
    SetElementEnabledByID("Replace", true);
    SetElementEnabledByID("ReplaceAll", true);
    SetElementEnabledByID("AddToDictionary", true);
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
  
  DoEnabling();
  
  // EXPERIMENTAL: Automatically shift focus to replace word editfield
  if (MisspelledWord)
    dialog.ReplaceWordInput.focus();
}

function CheckWord()
{
  //dump("SpellCheck: CheckWord\n");
  word = dialog.ReplaceWordInput.value;
  if (word != "") {
    //dump("CheckWord: Word in edit field="+word+"\n");
    isMisspelled = spellChecker.CheckCurrentWord(word);
    if (isMisspelled) {
      dump("CheckWord says word was misspelled\n");
      MisspelledWord = word;
      FillSuggestedList();
    } else {
      ClearList(dialog.SuggestedList);
      AppendStringToList(dialog.SuggestedList, GetString("CorrectSpelling"));
      // Suppress being able to select the message text
      allowSelectWord = false;
    }
  }
}

function SelectSuggestedWord()
{
  dump("SpellCheck: SelectSuggestedWord\n");
  if (allowSelectWord)
    dialog.ReplaceWordInput.value = dialog.SuggestedList.options[dialog.SuggestedList.selectedIndex].value;
}

function ChangeReplaceWord()
{
  // Unselect the word in the suggested list when user edits the replacement word
  dialog.SuggestedList.selectedIndex = -1;
}

function Ignore()
{
  dump("SpellCheck: Ignore\n");
  NextWord();
}

function IgnoreAll()
{
  dump("SpellCheck: IgnoreAll\n");
  if (MisspelledWord != "") {
    spellChecker.IgnoreWordAllOccurrences(MisspelledWord);
  }
  NextWord();
}

function Replace()
{
  dump("SpellCheck: Replace\n");
  newWord = dialog.ReplaceWordInput.value;
  //dump("New = "+newWord+" Misspelled = "+MisspelledWord+"\n");
  if (MisspelledWord != "" && MisspelledWord != newWord) {
    isMisspelled = spellChecker.ReplaceWord(MisspelledWord, newWord, false);
  }
  NextWord();
}

function ReplaceAll()
{
  dump("SpellCheck: ReplaceAll\n");
  newWord = dialog.ReplaceWordInput.value;
  if (MisspelledWord != "" && MisspelledWord != newWord) {
    isMisspelled = spellChecker.ReplaceWord(MisspelledWord, newWord, true);
  }
  NextWord();
}

function AddToDictionary()
{
  dump("SpellCheck: AddToDictionary\n");
  if (MisspelledWord != "") {
    spellChecker.AddWordToDictionary(MisspelledWord);
  }
}

function EditDictionary()
{
  window.openDialog("chrome://editor/content/EdDictionary.xul", "_blank", "chrome,close,titlebar,modal", "", MisspelledWord);
}

function SelectLanguage()
{
  // A bug in combobox prevents this from working
  dump("SpellCheck: SelectLanguage.\n");
}

function Recheck()
{
  //TODO: Should we bother to add a "Recheck" method to interface?
  spellChecker.CloseSpellChecking();
  MisspelledWord = spellChecker.StartSpellChecking();
  SetWidgetsForMisspelledWord();
}

function Close()
{
  // Shutdown the spell check and close the dialog
  spellChecker.CloseSpellChecking();
  window.close();
}

function FillSuggestedList()
{
  list = dialog.SuggestedList;

  // Clear the current contents of the list
  ClearList(list);

  if (MisspelledWord.length > 0)
  {
    // Get suggested words until an empty string is returned
    do {
      word = spellChecker.GetSuggestedWord();
      dump("Suggested Word = "+word+"\n");
      if (word != "") {
        AppendStringToList(list, word);
      }
    } while (word != "");
    if (list.length == 0) {
      // No suggestions - show a message but don't let user select it
      AppendStringToList(list, GetString("NoSuggestedWords"));
      allowSelectWord = false;
    } else {
      allowSelectWord = true;
    }
  }
}

