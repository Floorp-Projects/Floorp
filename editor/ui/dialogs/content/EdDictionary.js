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

var gSpellChecker;
var gWordToAdd;

function Startup()
{
  if (!GetCurrentEditor())
  {
    window.close();
    return;
  }
  // Get the SpellChecker shell
  if ("gSpellChecker" in window.opener && window.opener.gSpellChecker)
    gSpellChecker = window.opener.gSpellChecker;

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
  for (var i = 0; i < gDialog.DictionaryList.getRowCount(); i++)
  {

    var wordInList = gDialog.DictionaryList.getItemAtIndex(i);
    if (wordInList && gWordToAdd == wordInList.label)
    {
      gDialog.DictionaryList.selectedIndex = i;
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
    var selItem = gDialog.DictionaryList.selectedItem;
    if (selItem)
    {
      try {
        gSpellChecker.RemoveWordFromDictionary(selItem.label);
      } catch (e) {}

      try {
        // Add to the dictionary list
        gSpellChecker.AddWordToDictionary(gWordToAdd);

        // Just change the text on the selected item
        //  instead of rebuilding the list
        selItem.label = gWordToAdd; 
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
    var word = gDialog.DictionaryList.selectedItem.label;

    // Remove word from list
    gDialog.DictionaryList.removeItemAt(selIndex);

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
  ClearListbox(gDialog.DictionaryList);

  // Get the list from the spell checker
  gSpellChecker.GetPersonalDictionary()

  var haveList = false;

  // Get words until an empty string is returned
  do {
    var word = gSpellChecker.GetPersonalDictionaryWord();
    if (word != "")
    {
      gDialog.DictionaryList.appendItem(word, "");
      haveList = true;
    }
  } while (word != "");
  
  //XXX: BUG 74467: If list is empty, it doesn't layout to full height correctly
  //     (ignores "rows" attribute) (bug is latered, so we are fixing here for now)
  if (!haveList)
      gDialog.DictionaryList.appendItem("", "");

  ResetSelectedItem(selIndex);
}

function ResetSelectedItem(index)
{
  var lastIndex = gDialog.DictionaryList.getRowCount() - 1;
  if (index > lastIndex)
    index = lastIndex;

  // If we didn't have a selected item, 
  //  set it to the first item
  if (index == -1 && lastIndex >= 0)
    index = 0;

  gDialog.DictionaryList.selectedIndex = index;
}

function onClose()
{
  return true;
}
