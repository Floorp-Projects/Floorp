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

var misspelledWord;
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

  dialog.wordInput = document.getElementById("Word");
  dialog.suggestedList = document.getElementById("SuggestedList");
  dialog.languageList = document.getElementById("");
  if (!dialog.wordInput ||
      !dialog.suggestedList  ||
      !dialog.languageList )
  {
    dump("Not all dialog controls were found!!!\n");
  }
  // NOTE: We shouldn't have been created if there was no misspelled word
  
  // The first misspelled word is passed as the 2nd extra parameter in window.openDialog()
  misspelledWord = window.arguments[1];
  
  if (misspelledWord != "") {
    dump("First misspelled word = "+misspelledWord+"\n");
    // Put word in input field
    dialog.wordInput.value = misspelledWord;
    // Get the list of suggested replacements
    FillSuggestedList();
  } else {
    dump("No misspelled word found\n");
  }

  dialog.suggestedList.focus();  
}

function NextWord()
{
  misspelledWord = spellChecker.GetNextMisspelledWord();
  dialog.wordInput.value = misspelledWord;
  if (misspelledWord == "") {
    dump("FINISHED SPELL CHECKING\n");
    FillSuggestedList("(no suggestions)");
  } else {
    FillSuggestedList();
  }
}

function CheckWord()
{
  dump("SpellCheck: CheckWord\n");
  word = dialog.wordInput.value;
  if (word != "") {
    dump("CheckWord: Word in edit field="+word+"\n");
    isMisspelled = spellChecker.CheckCurrentWord(word);
    if (isMisspelled) {
      dump("CheckWord says word was misspelled\n");
      misspelledWord = word;
      FillSuggestedList();
    } else {
      ClearList(dialog.suggestedList);
      AppendStringToList(dialog.suggestedList, editorShell.GetString("CorrectSpelling"));
      // Suppress being able to select the message text
      allowSelectWord = false;
    }
  }
}

function SelectSuggestedWord()
{
  dump("SpellCheck: SelectSuggestedWord\n");
  if (allowSelectWord)
    dialog.wordInput.value = dialog.suggestedList.options[dialog.suggestedList.selectedIndex].value;
}

function Ignore()
{
  dump("SpellCheck: Ignore\n");
  NextWord();
}

function IgnoreAll()
{
  dump("SpellCheck: IgnoreAll\n");
  if (misspelledWord != "") {
    spellChecker.IgnoreWordAllOccurrences(misspelledWord);
  }
  NextWord();
}

function Replace()
{
  dump("SpellCheck: Replace\n");
  newWord = dialog.wordInput.value;
  if (misspelledWord != "" && misspelledWord != newWord) {
    isMisspelled = spellChecker.ReplaceWord(misspelledWord, newWord, false);
  }
  NextWord();
}

function ReplaceAll()
{
  dump("SpellCheck: ReplaceAll\n");
  newWord = dialog.wordInput.value;
  if (misspelledWord != "" && misspelledWord != newWord) {
    isMisspelled = spellChecker.ReplaceWord(misspelledWord, newWord, true);
  }
  NextWord();
}

function AddToDictionary()
{
  dump("SpellCheck: AddToDictionary\n");
  if (misspelledWord != "") {
    spellChecker.AddWordToDictionary(misspelledWord);
  }
}

function EditDictionary()
{
  window.openDialog("chrome://editor/content/EdDictionary.xul", "Dictionary", "chrome,close,titlebar,modal", "", misspelledWord);
}

function SelectLanguage()
{
  // A bug in combobox prevents this from working
  dump("SpellCheck: SelectLanguage.\n");
}

function Close()
{
  dump("SpellCheck: Spell Checker is closing...\n");
  // Shutdown the spell check and close the dialog
  spellChecker.CloseSpellChecking();
  window.close();
}

function FillSuggestedList(firstWord)
{
  list = dialog.suggestedList;

  // Clear the current contents of the list
  ClearList(list);

  // We may have the initial word
  if (firstWord && firstWord != "") {
    dump("First Word = "+firstWord+"\n");
    AppendStringToList(list, firstWord);
  }

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
    AppendStringToList(list, editorShell.GetString("NoSuggestedWords"));
    allowSelectWord = false;
  } else {
    allowSelectWord = true;
  }
}

