/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
 
/* Main Composer window debug menu functions */

// --------------------------- Output ---------------------------


function EditorGetText()
{
  if (editorShell) {
    dump("Getting text\n");
    var  outputText = editorShell.GetContentsAs("text/plain", 2);
    dump("<<" + outputText + ">>\n");
  }
}

function EditorGetHTML()
{
  if (editorShell) {
    dump("Getting HTML\n");
    var  outputHTML = editorShell.GetContentsAs("text/html", 256);
    dump(outputHTML + "\n");
  }
}

function EditorDumpContent()
{
  if (window.editorShell) {
    dump("==============  Content Tree: ================\n");
    window.editorShell.DumpContentTree();
  }
}

function EditorInsertText(textToInsert)
{
  editorShell.InsertText(textToInsert);
}

function EditorTestSelection()
{
  dump("Testing selection\n");
  var selection = editorShell.editorSelection;
  if (!selection)
  {
    dump("No selection!\n");
    return;
  }

  dump("Selection contains:\n");
  // 3rd param = column to wrap
  dump(selection.toStringWithFormat("text/plain",
                                    gOutputFormatted & gOutputSelectionOnly,
                                    0) + "\n");

  var output;

  dump("====== Selection as unformatted text ==========\n");
  output = editorShell.GetContentsAs("text/plain", 1);
  dump(output + "\n\n");

  dump("====== Selection as formatted text ============\n");
  output = editorShell.GetContentsAs("text/plain", 3);
  dump(output + "\n\n");

  dump("====== Selection as HTML ======================\n");
  output = editorShell.GetContentsAs("text/html", 1);
  dump(output + "\n\n");  

  dump("====== Selection as prettyprinted HTML ========\n");
  output = editorShell.GetContentsAs("text/html", 3);
  dump(output + "\n\n");  

  dump("====== Length and status =====================\n");
  output = "Document is ";
  if (editorShell.documentIsEmpty)
    output += "empty\n";
  else
    output += "not empty\n";
  output += "Document length is " + editorShell.documentLength + " characters";
  dump(output + "\n\n");
}

function EditorTestTableLayout()
{
  dump("\n\n\n************ Dump Selection Ranges ************\n");
  var selection = editorShell.editorSelection;
  for (i = 0; i < selection.rangeCount; i++)
  {
    var range = selection.getRangeAt(i);
    if (range)
    {
      dump("Range "+i+": StartParent="+range.startParent+", offset="+range.startOffset+"\n");
    }
  }
  dump("\n\n");

  var table = editorShell.GetElementOrParentByTagName("table", null);
  if (!table) {
    dump("Enclosing Table not found: Place caret in a table cell to do this test\n\n");
    return;
  }
    
  var cell;
  var startRowIndexObj = new Object();
  var startColIndexObj = new Object();
  var rowSpanObj = new Object();
  var colSpanObj = new Object();
  var actualRowSpanObj = new Object();
  var actualColSpanObj = new Object();
  var isSelectedObj = new Object();
  var startRowIndex = 0;
  var startColIndex = 0;
  var rowSpan;
  var colSpan;
  var actualRowSpan;
  var actualColSpan;
  var isSelected;
  var col = 0;
  var row = 0;
  var rowCount = 0;
  var maxColCount = 0;
  var doneWithRow = false;
  var doneWithCol = false;

  dump("\n\n\n************ Starting Table Layout test ************\n");

  // Note: We could also get the number of rows, cols and use for loops,
  //   but this tests using out-of-bounds offsets to detect end of row or column

  while (!doneWithRow)  // Iterate through rows
  {  
    dump("* Data for ROW="+row+":\n");
    while(!doneWithCol)  // Iterate through cells in the row
    {
      try {
        cell = editorShell.GetCellDataAt(table, row, col, startRowIndexObj, startColIndexObj,
                                         rowSpanObj, colSpanObj, actualRowSpanObj, actualColSpanObj, 
                                         isSelectedObj);

        if (cell)
        {
          rowSpan = rowSpanObj.value;
          colSpan = colSpanObj.value;
          actualRowSpan = actualRowSpanObj.value;
          actualColSpan = actualColSpanObj.value;
          isSelected = isSelectedObj.value;
          
          dump(" Row="+row+", Col="+col+"  StartRow="+startRowIndexObj.value+", StartCol="+startColIndexObj.value+"\n");
          dump("  RowSpan="+rowSpan+", ColSpan="+colSpan+"  ActualRowSpan="+actualRowSpan+", ActualColSpan="+actualColSpan);
          if (isSelected)
            dump("  Cell is selected\n");
          else
            dump("  Cell is NOT selected\n");

          // Save the indexes of a cell that will span across the cellmap grid
          if (rowSpan > 1)
            startRowIndex = startRowIndexObj.value;
          if (colSpan > 1)
            startColIndex = startColIndexObj.value;

          // Initialize these for efficient spanned-cell search
          startRowIndexObj.value = startRowIndex;
          startColIndexObj.value = startColIndex;

          col++;
        } else {
          doneWithCol = true;
          // Get maximum number of cells in any row
          if (col > maxColCount)
            maxColCount = col;
          dump("  End of row found\n\n");
        }
      }
      catch (e) {
        dump("  *** GetCellDataAt failed at Row="+row+", Col="+col+" ***\n\n");
        return;
      }
    }
    if (col == 0) {
      // Didn't find a cell in the first col of a row,
      // thus no more rows in table
      doneWithRow = true;
      rowCount = row;
      dump("No more rows in table\n\n");
    } else {
      // Setup for next row
      col = 0;
      row++;
      doneWithCol = false;
    }      
  }
  dump("Counted during scan: Number of rows="+rowCount+" Number of Columns="+maxColCount+"\n");
  rowCount = editorShell.GetTableRowCount(table);
  maxColCount = editorShell.GetTableColumnCount(table);
  dump("From nsITableLayout: Number of rows="+rowCount+" Number of Columns="+maxColCount+"\n****** End of Table Layout Test *****\n\n");
}

function EditorShowEmbeddedObjects()
{
  dump("\nEmbedded Objects:\n");
  var objectArray = editorShell.GetEmbeddedObjects();
  dump(objectArray.Count() + " embedded objects\n");
  for (var i=0; i < objectArray.Count(); ++i)
    dump(objectArray.GetElementAt(i) + "\n");
}

function EditorUnitTests()
{
  dump("Running Unit Tests\n");
  editorShell.RunUnitTests();
}

function EditorTestDocument()
{
  dump("Getting document\n");
  var theDoc = editorShell.editorDocument;
  if (theDoc)
  {
    dump("Got the doc\n");
    dump("Document name:" + theDoc.nodeName + "\n");
    dump("Document type:" + theDoc.doctype + "\n");
  }
  else
  {
    dump("Failed to get the doc\n");
  }
}

// --------------------------- Logging stuff ---------------------------

function EditorExecuteScript(theFile)
{
  var inputStream = Components.classes["@mozilla.org/network/file-input-stream;1"].createInstance();
  inputStream = inputStream.QueryInterface(Components.interfaces.nsIFileInputStream);

  inputStream.init(theFile, 1, 0);    // open read only
  
  var scriptableInputStream = Components.classes["@mozilla.org/scriptableinputstream;1"].createInstance();
  scriptableInputStream = scriptableInputStream.QueryInterface(Components.interfaces.nsIScriptableInputStream);
  
  scriptableInputStream.init(inputStream);    // open read only
  
  var buf         = { value:null };
  var tmpBuf      = { value:null };
  var didTruncate = { value:false };
  var lineNum     = 0;
  var ex;

/*
  // Log files can be quite huge, so read in a line
  // at a time and execute it:

  while (!inputStream.eof())
  {
    buf.value         = "";
    didTruncate.value = true;

    // Keep looping until we get a complete line of
    // text, or we hit the end of file:

    while (didTruncate.value && !inputStream.eof())
    {
      didTruncate.value = false;
      fileSpec.readLine(tmpBuf, 1024, didTruncate);
      buf.value += tmpBuf.value;

      // XXX Need to null out tmpBuf.value to avoid crashing
      // XXX in some JavaScript string allocation method.
      // XXX This is probably leaking the buffer allocated
      // XXX by the readLine() implementation.

      tmpBuf.value = null;
    }

    ++lineNum;
*/
  {
    // suck in the entire file
    var fileSize = scriptableInputStream.available();
    var fileContents = scriptableInputStream.read(fileSize);
    
    dump(fileContents);
    
    try       { eval(fileContents); }
    catch(ex) { dump("Playback ERROR: Line " + lineNum + "  " + ex + "\n"); return; }
  }

  buf.value = null;
}

function EditorGetScriptFileSpec()
{
  var dirServ = Components.classes['@mozilla.org/file/directory_service;1'].createInstance();
  dirServ = dirServ.QueryInterface(Components.interfaces.nsIProperties);
  var processDir = dirServ.get("Home", Components.interfaces.nsIFile);
  processDir.append("journal.js");
  return processDir;
}

function EditorStartLog()
{
  var fs;

  fs = EditorGetScriptFileSpec();
  editorShell.StartLogging(fs);
  window._content.focus();

  fs = null;
}

function EditorStopLog()
{
  editorShell.StopLogging();
  window._content.focus();
}

function EditorRunLog()
{
  var fs;
  fs = EditorGetScriptFileSpec();
  EditorExecuteScript(fs);
  window._content.focus();
}

// --------------------------- TransactionManager ---------------------------


function DumpUndoStack()
{
  try {
    var txmgr = editorShell.transactionManager;

    if (!txmgr)
    {
      dump("**** Editor has no TransactionManager!\n");
      return;
    }

    dump("---------------------- BEGIN UNDO STACK DUMP\n");
    dump("<!-- Bottom of Stack -->\n");
    PrintTxnList(txmgr.getUndoList(), "");
    dump("<!--  Top of Stack  -->\n");
    dump("Num Undo Items: " + txmgr.numberOfUndoItems + "\n");
    dump("---------------------- END   UNDO STACK DUMP\n");
  } catch (e) {
    dump("ERROR: DumpUndoStack() failed: " + e);
  }
}

function DumpRedoStack()
{
  try {
    var txmgr = editorShell.transactionManager;

    if (!txmgr)
    {
      dump("**** Editor has no TransactionManager!\n");
      return;
    }

    dump("---------------------- BEGIN REDO STACK DUMP\n");
    dump("<!-- Bottom of Stack -->\n");
    PrintTxnList(txmgr.getRedoList(), "");
    dump("<!--  Top of Stack  -->\n");
    dump("Num Redo Items: " + txmgr.numberOfRedoItems + "\n");
    dump("---------------------- END   REDO STACK DUMP\n");
  } catch (e) {
    dump("ERROR: DumpUndoStack() failed: " + e);
  }
}

function PrintTxnList(txnList, prefixStr)
{
  var i;

  for (i=0 ; i < txnList.numItems; i++)
  {
    var txn = txnList.getItem(i);
    var desc = "TXMgr Batch";

    if (txn)
    {
      txn = txn.QueryInterface(Components.interfaces.nsPIEditorTransaction);
      desc = txn.txnDescription;
    }
    dump(prefixStr + "+ " + desc + "\n");
    PrintTxnList(txnList.getChildListForItem(i), prefixStr + "|    ");
  }
}

