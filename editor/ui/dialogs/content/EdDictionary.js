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

var spellChecker;
var WordToAdd;
var WordInput;
var DictionaryList;

function Startup()
{
  if (!InitEditorShell())
    return;
  dump("EditoreditorShell found for dialog\n");

  // Get the spellChecker shell
  spellChecker = editorShell.QueryInterface(Components.interfaces.nsIEditorSpellCheck);
  if (!spellChecker)
  {
    dump("SpellChecker not found!!!\n");
    window.close();
  }
  // The word to add word is passed as the 2nd extra parameter in window.openDialog()
  WordToAdd = window.arguments[1];
  
  WordInput = document.getElementById("WordInput");
  DictionaryList = document.getElementById("DictionaryList");
  
  WordInput.value = WordToAdd;
  FillDictionaryList();

  // Select the supplied word if it is already in the list
  SelectWordToAddInList();
  SetTextfieldFocus(WordInput);

  SetWindowLocation();
}

function ValidateWordToAdd()
{
  WordToAdd = TrimString(WordInput.value);
  if (WordToAdd.length > 0)
  {
    return true;
  } else {
    return false;
  }
}    

function SelectWordToAddInList()
{
  for (var index = 0; index < DictionaryList.getAttribute("length"); index++)
  {
    if (WordToAdd == GetTreelistValueAt(DictionaryList,index))
    {
      DictionaryList.selectedIndex = index;
      break;
    }
  }
}

function AddWord()
{
  if (ValidateWordToAdd())
  {
    try {
      spellChecker.AddWordToDictionary(WordToAdd);
    }
    catch (e) {
      dump("Exception occured in spellChecker.AddWordToDictionary\nWord to add probably already existed\n");
    }

    // Rebuild the dialog list
    FillDictionaryList();

    SelectWordToAddInList();
  }
}

function ReplaceWord()
{
  if (ValidateWordToAdd())
  {
    selIndex = DictionaryList.selectedIndex;
    if (selIndex >= 0)
    {

      WordToRemove = GetSelectedTreelistValue(DictionaryList);
dump("Word to remove: "+WordToRemove+"\n");
      spellChecker.RemoveWordFromDictionary(WordToRemove);
      try {
        // Add to the dictionary list
        spellChecker.AddWordToDictionary(WordToAdd);
        // Just change the text on the selected item
        //  instead of rebuilding the list
        ReplaceStringInTreeList(DictionaryList, selIndex, WordToAdd);
      } catch (e) {
        // Rebuild list and select the word - it was probably already in the list
        dump("Exception occured adding word in ReplaceWord\n");
        FillDictionaryList();
        SelectWordToAddInList();
      }
    }
  }
}

function RemoveWord()
{
  selIndex = DictionaryList.selectedIndex;
dump("RemoveWord/n");
  if (selIndex >= 0)
  {
    word = GetSelectedTreelistValue(DictionaryList);

    // Remove word from list
    RemoveSelectedTreelistItem(DictionaryList);

    // Remove from dictionary
    try {
      //Not working: BUG 43348
      spellChecker.RemoveWordFromDictionary(word);
    }
    catch (e)
    {
      dump("Failed to remove word from dictionary\n");
    }

    ResetSelectedItem(selIndex);
  }
}

function FillDictionaryList()
{
  selIndex = DictionaryList.selectedIndex;

  // Clear the current contents of the list
  ClearTreelist(DictionaryList);
  // Get the list from the spell checker
  spellChecker.GetPersonalDictionary()

  // Get words until an empty string is returned
  do {
    word = spellChecker.GetPersonalDictionaryWord();
    if (word != "")
      AppendStringToTreelist(DictionaryList, word);

  } while (word != "");

  ResetSelectedItem(selIndex);
}

function ResetSelectedItem(index)
{
  lastIndex = DictionaryList.getAttribute("length") - 1;
  if (index > lastIndex)
    index = lastIndex;

  // If we didn't have a selected item, 
  //  set it to the first item
  if (index == -1 && lastIndex >= 0)
    index = 0;

dump("ResetSelectedItem to index="+index+"\n");

  DictionaryList.selectedIndex = index;
}

function Close()
{
  // Close the dialog
  SaveWindowLocation();
  window.close();
}
