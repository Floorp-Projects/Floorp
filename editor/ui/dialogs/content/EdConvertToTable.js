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

var dialog;
var gIndex;
var gCommaIndex = "0";
var gSpaceIndex = "1";
var gOtherIndex = "2";

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, onCancel);

  // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.sepRadioGroup      = document.getElementById("SepRadioGroup");
  dialog.sepCharacterInput  = document.getElementById("SepCharacterInput");
  dialog.deleteSepCharacter = document.getElementById("DeleteSepCharacter");
  
  // We persist the user's separator character
  dialog.sepCharacterInput.value = dialog.sepRadioGroup.getAttribute("character");

  // Always default to deleting the separator character
  dialog.deleteSepCharacter.checked = true;

  gIndex = dialog.sepRadioGroup.getAttribute("index");
dump("***** Previous index of radio group in EdConvertToTable="+gIndex+"\n");

  switch (gIndex)
  {
    case gCommaIndex:
    default:
      document.getElementById("comma").checked = true;
      break;
    case gSpaceIndex:
      document.getElementById("space").checked = true;
      break;
    case gOtherIndex:
      document.getElementById("other").checked = true;
      break;
  }

  // Set initial enable state on character input
  SelectCharacter(gIndex);

  SetTextboxFocus(dialog.sepRadioGroup);

  SetWindowLocation();
}

function SelectCharacter(radioGroupIndex)
{
  gIndex = radioGroupIndex;
dump("***** SelectCharacter index of radio group ="+gIndex+"\n");
  SetElementEnabledById("SepCharacterInput", gIndex == gOtherIndex);
}

function onOK()
{
  // 1 = OutputSelectionOnly, 1024 = OutputLFLineBreak
  // 256 = OutputEncodeEntities
  var str = editorShell.GetContentsAs("text/html", 1+1024);

  // Replace nbsp with spaces:
  str = str.replace(/\u00a0/g, " ");

  // Trim trailing <p> or <br>
  str = str.replace(/\s*<br>\s*$/, "");
  str = str.replace(/\s*<p>\s*$/, "");

  // Trim leading and trailing spaces
  str = str.replace(/^\s+/, "");
  str = str.replace(/\s+$/, "");
  // Trim whitespace adjacent to <p> and <br> tags
  str = str.replace(/\s+<p>\s+/g, "<br>");
  str = str.replace(/\s+<br>\s+/g, "<br>");

  var sepCharacter = "";
  switch ( gIndex )
  {
    case gCommaIndex:
      sepCharacter = ",";
      break;
    case gSpaceIndex:
      sepCharacter = " ";
      break;
    case gOtherIndex:
      sepCharacter = dialog.sepCharacterInput.value.slice(0,1);
      break;
  }

  if (sepCharacter.length == 0)
  {
    ShowInputErrorMessage(editorShell.GetString("NoSeparatorCharacter"));
    SetTextboxFocus(dialog.sepCharacterInput);
    return false;
  }

  // Replace separator characters with table cells
  var replaceString;
  if (dialog.deleteSepCharacter.checked)
  {
    replaceString = "";
    // Replace one or more adjacent spaces
    if (sepCharacter == " ")
      sepCharacter += "+"; 
  }  
  else
  {
    // Don't delete separator character,
    //  so include it at start of string to replace
    replaceString = sepCharacter;
  }
  replaceString += " <td>"; 

  var pattern = new RegExp("\\" + sepCharacter, "g");
  //pattern.global = true;
  str = str.replace(pattern, replaceString);

  // End table row and start another for each br or p
  str = str.replace(/\s*<br>\s*/g, "</tr>\n<tr><td>");

  // Add the table tags and the opening and closing tr/td tags
  str = "<table border=\"1\">\n<tr><td>" + str + "</tr>\n</table>\n";

  editorShell.BeginBatchChanges();
  editorShell.InsertSource(str);
  
  // Get the table just inserted
  var selection = editorShell.editorSelection;

  editorShell.NormalizeTable(null);
  editorShell.EndBatchChanges();

  // Save persisted attributes
  dialog.sepRadioGroup.setAttribute("index", gIndex);
  if (gIndex == gOtherIndex)
    dialog.sepRadioGroup.setAttribute("character", sepCharacter);

  SaveWindowLocation();
  return true;
}
