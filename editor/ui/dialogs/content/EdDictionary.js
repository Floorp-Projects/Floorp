/*  */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
  // Create dialog object to store controls for easy access
  dialog = new Object;
  if (!dialog)
  {
    dump("Failed to create dialog object!!!\n");
    Close();
  }
  // The word to add word is passed as the 2nd extra parameter in window.openDialog()
  gWordToAdd = window.arguments[1];
  
  dialog.WordInput = document.getElementById("WordInput");
  dialog.DictionaryList = document.getElementById("DictionaryList");
  
  dialog.WordInput.value = gWordToAdd;
  FillDictionaryList();

  // Select the supplied word if it is already in the list
  SelectWordToAddInList();
  SetTextboxFocus(dialog.WordInput);

  SetWindowLocation();
}

function ValidateWordToAdd()
{
  gWordToAdd = TrimString(dialog.WordInput.value);
  if (gWordToAdd.length > 0)
  {
    return true;
  } else {
    return false;
  }
}    

function SelectWordToAddInList()
{
  for (var index = 0; index < dialog.DictionaryList.getAttribute("length"); index++)
  {
    if (gWordToAdd == GetTreelistValueAt(dialog.DictionaryList,index))
    {
      dialog.DictionaryList.selectedIndex = index;
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
    dialog.WordInput.value = "";
  }
}

function ReplaceWord()
{
  if (ValidateWordToAdd())
  {
    var selIndex = dialog.DictionaryList.selectedIndex;
    if (selIndex >= 0)
    {
      gSpellChecker.RemoveWordFromDictionary(GetSelectedTreelistValue(dialog.DictionaryList));
      try {
        // Add to the dictionary list
        gSpellChecker.AddWordToDictionary(gWordToAdd);
        // Just change the text on the selected item
        //  instead of rebuilding the list
        ReplaceStringInTreeList(dialog.DictionaryList, selIndex, gWordToAdd);
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
  var selIndex = dialog.DictionaryList.selectedIndex;
  if (selIndex >= 0)
  {
    var word = GetSelectedTreelistValue(dialog.DictionaryList);

    // Remove word from list
    RemoveSelectedTreelistItem(dialog.DictionaryList);

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
  var selIndex = dialog.DictionaryList.selectedIndex;

  // Clear the current contents of the list
  ClearTreelist(dialog.DictionaryList);
  // Get the list from the spell checker
  gSpellChecker.GetPersonalDictionary()

  var haveList = false;

  // Get words until an empty string is returned
  do {
    var word = gSpellChecker.GetPersonalDictionaryWord();
    if (word != "")
    {
      AppendStringToTreelist(dialog.DictionaryList, word);
      haveList = true;
    }
  } while (word != "");
  
  //XXX: BUG 74467: If list is empty, tree doesn't layout to full height correctly
  //     (ignores "rows" attribute) (bug is latered, so we are fixing here for now)
  if (!haveList)
    AppendStringToTreelist(dialog.DictionaryList, " ");

  ResetSelectedItem(selIndex);
}

function ResetSelectedItem(index)
{
  var lastIndex = dialog.DictionaryList.getAttribute("length") - 1;
  if (index > lastIndex)
    index = lastIndex;

  // If we didn't have a selected item, 
  //  set it to the first item
  if (index == -1 && lastIndex >= 0)
    index = 0;

dump("ResetSelectedItem to index="+index+"\n");

  dialog.DictionaryList.selectedIndex = index;
}

function Close()
{
  // Close the dialog
  SaveWindowLocation();
  window.close();
}
