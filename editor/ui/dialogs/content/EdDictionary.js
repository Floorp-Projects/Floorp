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

var gSpellChecker;
var gWordToAdd;

function Startup()
{
  if (!InitEditorShell())
    return;
  dump("EditoreditorShell found for dialog\n");

  // Get the SpellChecker shell
  gSpellChecker = editorShell.QueryInterface(Components.interfaces.nsIEditorSpellCheck);
  if (!gSpellChecker)
  {
    dump("SpellChecker not found!!!\n");
    window.close();
    return;
  }
  // The word to add word is passed as the 2nd extra parameter in window.openDialog()
  gWordToAdd = window.arguments[1];
  
  gDialog.WordInput = document.getElementById("WordInput");
  gDialog.DictionaryList = document.getElementById("DictionaryList");
  
  gDialog.WordInput.value = gWordToAdd;
  FillDictionaryList();

  // Select the supplied word if it is already in the list
  SelectWordToAddInList();
  SetTextboxFocus(gDialog.WordInput);

  SetWindowLocation();
}

function ValidateWordToAdd()
{
  gWordToAdd = TrimString(gDialog.WordInput.value);
  if (gWordToAdd.length > 0)
  {
    return true;
  } else {
    return false;
  }
}    

function SelectWordToAddInList()
{
  for (var index = 0; index < gDialog.DictionaryList.getAttribute("length"); index++)
  {
    if (gWordToAdd == GetTreelistValueAt(gDialog.DictionaryList,index))
    {
      gDialog.DictionaryList.selectedIndex = index;
      break;
    }
  }
}

function AddWord()
{
  if (ValidateWordToAdd())
  {
    try {
      gSpellChecker.AddWordToDictionary(gWordToAdd);
    }
    catch (e) {
      dump("Exception occured in gSpellChecker.AddWordToDictionary\nWord to add probably already existed\n");
    }

    // Rebuild the dialog list
    FillDictionaryList();

    SelectWordToAddInList();
    gDialog.WordInput.value = "";
  }
}

function ReplaceWord()
{
  if (ValidateWordToAdd())
  {
    var selIndex = gDialog.DictionaryList.selectedIndex;
    if (selIndex >= 0)
    {
      gSpellChecker.RemoveWordFromDictionary(GetSelectedTreelistValue(gDialog.DictionaryList));
      try {
        // Add to the dictionary list
        gSpellChecker.AddWordToDictionary(gWordToAdd);
        // Just change the text on the selected item
        //  instead of rebuilding the list
        ReplaceStringInTreeList(gDialog.DictionaryList, selIndex, gWordToAdd);
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
  var selIndex = gDialog.DictionaryList.selectedIndex;
  if (selIndex >= 0)
  {
    var word = GetSelectedTreelistValue(gDialog.DictionaryList);

    // Remove word from list
    RemoveSelectedTreelistItem(gDialog.DictionaryList);

    // Remove from dictionary
    try {
      //Not working: BUG 43348
      gSpellChecker.RemoveWordFromDictionary(word);
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
  var selIndex = gDialog.DictionaryList.selectedIndex;

  // Clear the current contents of the list
  ClearTreelist(gDialog.DictionaryList);
  // Get the list from the spell checker
  gSpellChecker.GetPersonalDictionary()

  var haveList = false;

  // Get words until an empty string is returned
  do {
    var word = gSpellChecker.GetPersonalDictionaryWord();
    if (word != "")
    {
      AppendStringToTreelist(gDialog.DictionaryList, word);
      haveList = true;
    }
  } while (word != "");
  
  //XXX: BUG 74467: If list is empty, tree doesn't layout to full height correctly
  //     (ignores "rows" attribute) (bug is latered, so we are fixing here for now)
  if (!haveList)
    AppendStringToTreelist(gDialog.DictionaryList, " ");

  ResetSelectedItem(selIndex);
}

function ResetSelectedItem(index)
{
  var lastIndex = gDialog.DictionaryList.getAttribute("length") - 1;
  if (index > lastIndex)
    index = lastIndex;

  // If we didn't have a selected item, 
  //  set it to the first item
  if (index == -1 && lastIndex >= 0)
    index = 0;

dump("ResetSelectedItem to index="+index+"\n");

  gDialog.DictionaryList.selectedIndex = index;
}

function Close()
{
  // Close the dialog
  SaveWindowLocation();
  window.close();
}
