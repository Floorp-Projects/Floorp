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

  gDialog.sepRadioGroup      = document.getElementById("SepRadioGroup");
  gDialog.sepCharacterInput  = document.getElementById("SepCharacterInput");
  gDialog.deleteSepCharacter = document.getElementById("DeleteSepCharacter");
  gDialog.collapseSpaces     = document.getElementById("CollapseSpaces");

  // We persist the user's separator character
  gDialog.sepCharacterInput.value = gDialog.sepRadioGroup.getAttribute("character");

  gIndex = gDialog.sepRadioGroup.getAttribute("index");

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

  // Set initial enable state on character input and "collapse" checkbox
  SelectCharacter(gIndex);

  SetTextboxFocus(gDialog.sepRadioGroup);

  SetWindowLocation();
}

function InputSepCharacter()
{
  var str = gDialog.sepCharacterInput.value;

  // Limit input to 1 character
  if (str.length > 1)
    str.slice(0,1);

  // We can never allow tag or entity delimeters for separator character
  if (str == "<" || str == ">" || str == "&" || str == ";" || str == " ")
    str = "";

  gDialog.sepCharacterInput.value = str;
}

function SelectCharacter(radioGroupIndex)
{
  gIndex = radioGroupIndex;
  SetElementEnabledById("SepCharacterInput", gIndex == gOtherIndex);
  SetElementEnabledById("CollapseSpaces", gIndex == gSpaceIndex);
}

function onOK()
{
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
      sepCharacter = gDialog.sepCharacterInput.value.slice(0,1);
      break;
  }

  // 1 = OutputSelectionOnly, 1024 = OutputLFLineBreak
  // 256 = OutputEncodeEntities
  var str = editorShell.GetContentsAs("text/html", 1+1024);

  // Replace nbsp with spaces:
  str = str.replace(/\u00a0/g, " ");

  // Strip out </p> completely
  str = str.replace(/\s*<\/p>\s*/g, "");

  // Trim whitespace adjacent to <p> and <br> tags
  //  and replace <p> with <br> 
  //  (which will be replaced with </tr> below)
  str = str.replace(/\s*<p>\s*|\s*<br>\s*/g, "<br>");

  // Trim leading <br>s
  str = str.replace(/^(<br>)+/, "");

  // Trim trailing <br>s
  str = str.replace(/(<br>)+$/, "");

  // Reduce multiple internal <br> to just 1
  // TODO: Maybe add a checkbox to let user decide
  //str = str.replace(/(<br>)+/g, "<br>");

  // Trim leading and trailing spaces
  str = str.replace(/^\s+|\s+$/, "");

  // Remove all tag contents so we don't replace
  //   separator character within tags
  // Also converts lists to something usefull
  var stack = [];
  var start;
  var end;
  var searchStart = 0;
  var listSeparator = "";
  var listItemSeparator = "";
  var endList = false;

  do {
    start = str.indexOf("<", searchStart);

    if (start >= 0)
    {
      var end = str.indexOf(">", start+1);
      if (end > start)
      {
        var tagContent = TrimString(str.slice(start+1, end));

        if ( tagContent.match(/^ol|^ul|^dl/) )
        {
          //  Replace list tag with <BR> to start new row 
          //   at begining of second or greater list tag
          str = str.slice(0, start) + listSeparator + str.slice(end+1);
          if (listSeparator == "")
            listSeparator = "<br>";
          
          // Reset for list item separation into cells
          listItemSeparator = "";
        }
        else if ( tagContent.match(/^li|^dt|^dd/) )
        {
          // Start a new row if this is first item after the ending the last list
          if (endList)
            listItemSeparator = "<br>";

          // Start new cell at begining of second or greater list items
          str = str.slice(0, start) + listItemSeparator + str.slice(end+1);

          if (endList || listItemSeparator == "")
            listItemSeparator = sepCharacter;

          endList = false;
        }
        else 
        {
          // Find end tags
          endList = tagContent.match(/^\/ol|^\/ul|^\/dl/);
          if ( endList || tagContent.match(/^\/li|^\/dt|^\/dd/) )
          {
            // Strip out tag
            str = str.slice(0, start) + str.slice(end+1);
          }
          else
          {
            // Not a list-related tag: Store tag contents in an array
            stack.push(tagContent);
           
            // Keep the "<" and ">" while removing from source string
            start++;
            str = str.slice(0, start) + str.slice(end);
          }
        }
      }
      searchStart = start + 1;
    }
  } while (start >= 0);

  // Replace separator characters with table cells
  var replaceString;
  if (gDialog.deleteSepCharacter.checked)
  {
    replaceString = "";
  }  
  else
  {
    // Don't delete separator character,
    //  so include it at start of string to replace
    replaceString = sepCharacter;
  }

  replaceString += "<td>"; 

  if (sepCharacter.length > 0)
  {
    var tempStr = sepCharacter;
    var regExpChars = ".!@#$%^&*-+[]{}()\|\\\/";
    if (regExpChars.indexOf(sepCharacter) >= 0)
      tempStr = "\\" + sepCharacter;

    if (gIndex == gSpaceIndex)
    {
      // If checkbox is checked, 
      //   one or more adjacent spaces are one separator
      if (gDialog.collapseSpaces.checked)
          tempStr = "\\s+"
        else
          tempStr = "\\s";
    }
    var pattern = new RegExp(tempStr, "g");
    str = str.replace(pattern, replaceString);
  }

  // Put back tag contents that we removed above
  searchStart = 0;
  var stackIndex = 0;
  do {
    start = str.indexOf("<", searchStart);
    end = start + 1;
    if (start >= 0 && str.charAt(end) == ">")
    {
      // We really need a FIFO stack!
      str = str.slice(0, end) + stack[stackIndex++] + str.slice(end);
    }
    searchStart = end;

  } while (start >= 0);

  // End table row and start another for each br or p
  str = str.replace(/\s*<br>\s*/g, "</tr>\n<tr><td>");

  // Add the table tags and the opening and closing tr/td tags
  // Default table attributes should be same as those used in nsHTMLEditor::CreateElementWithDefaults()
  // (Default width="100%" is used in EdInsertTable.js)
  str = "<table border=\"1\" width=\"100%\" cellpadding=\"2\" cellspacing=\"2\">\n<tr><td>" + str + "</tr>\n</table>\n";

  editorShell.BeginBatchChanges();
  
  // Delete the selection -- makes it easier to find where table will insert
  editorShell.DeleteSelection(0);

  var anchorNodeBeforeInsert = editorShell.editorSelection.anchorNode;
  var offset = editorShell.editorSelection.anchorOffset;
  var nodeBeforeTable = null;
  var nodeAfterTable = null;
  if (anchorNodeBeforeInsert.nodeType == Node.TEXT_NODE)
  {
    // Text was split. Table should be right after the first or before 
    nodeBeforeTable = anchorNodeBeforeInsert.previousSibling;
    nodeAfterTable = anchorNodeBeforeInsert;
  }
  else
  {
    // Table should be inserted right after node pointed to by selection
    if (offset > 0)
      nodeBeforeTable = anchorNodeBeforeInsert.childNodes.item(offset - 1);

    nodeAfterTable = anchorNodeBeforeInsert.childNodes.item(offset);
  }
  
  editorShell.InsertSource(str);

  var table = null;
  if (nodeAfterTable)
  {
    var previous = nodeAfterTable.previousSibling;
    if (previous && previous.nodeName.toLowerCase() == "table")
      table = previous;
  }
  if (!table && nodeBeforeTable)
  {
    var next = nodeBeforeTable.nextSibling;
    if (next && next.nodeName.toLowerCase() == "table")
      table = next;
  }

  if (table)
  {
    // Fixup table only if pref is set
    var prefs = GetPrefs();
    try {
      if (prefs && prefs.GetBoolPref("editor.table.maintain_structure") )
        editorShell.NormalizeTable(table);
    } catch(ex) {
      dump(ex);
    }

    // Put caret in first cell
    var firstRow = editorShell.GetFirstRow(table);
    if (firstRow)
    {
      var node2 = firstRow.firstChild;
      do {
        if (node2.nodeName.toLowerCase() == "td" ||
            node2.nodeName.toLowerCase() == "th")
        {
          editorShell.editorSelection.collapse(node2, 0);
          break;
        }
        node2 = node.nextSibling;
      } while (node2);
    }
  }

  editorShell.EndBatchChanges();

  // Save persisted attributes
  gDialog.sepRadioGroup.setAttribute("index", gIndex);
  if (gIndex == gOtherIndex)
    gDialog.sepRadioGroup.setAttribute("character", sepCharacter);

  SaveWindowLocation();
  return true;
}
