/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
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

/* Main Composer window debug menu functions */

// --------------------------- Output ---------------------------


function EditorGetText()
{
  try {
    dump("Getting text\n");
    var  outputText = GetCurrentEditor().outputToString("text/plain", 2);
    dump("<<" + outputText + ">>\n");
  } catch (e) {}
}

function EditorGetHTML()
{
  try {
    dump("Getting HTML\n");
    var  outputHTML = GetCurrentEditor().outputToString("text/html", 256);
    dump(outputHTML + "\n");
  } catch (e) {}
}

function EditorDumpContent()
{
  dump("==============  Content Tree: ================\n");
  GetCurrentEditor().dumpContentTree();
}

function EditorInsertText(textToInsert)
{
  GetCurrentEditor().insertText(textToInsert);
}

function EditorTestSelection()
{
  dump("Testing selection\n");
  var selection = GetCurrentEditor().selection;
  if (!selection)
  {
    dump("No selection!\n");
    return;
  }

  dump("Selection contains:\n");
  // 3rd param = column to wrap
  dump(selection.QueryInterface(Components.interfaces.nsISelectionPrivate)
       .toStringWithFormat("text/plain",
                           3,  // OutputFormatted & gOutputSelectionOnly
                           0) + "\n");

  var output, i;

  dump("====== Selection as node and offsets==========\n");
  dump("rangeCount = " + selection.rangeCount + "\n");
  for (i = 0; i < selection.rangeCount; i++)
  {
    var range = selection.getRangeAt(i);
    if (range)
    {
      dump("Range "+i+": StartParent="+range.startContainer.nodeName+", offset="+range.startOffset+"\n");
      dump("Range "+i+":   EndParent="+range.endContainer.nodeName+", offset="+range.endOffset+"\n\n");
    }
  }

  var editor = GetCurrentEditor();

  dump("====== Selection as unformatted text ==========\n");
  output = editor.outputToString("text/plain", 1);
  dump(output + "\n\n");

  dump("====== Selection as formatted text ============\n");
  output = editor.outputToString("text/plain", 3);
  dump(output + "\n\n");

  dump("====== Selection as HTML ======================\n");
  output = editor.outputToString("text/html", 1);
  dump(output + "\n\n");

  dump("====== Selection as prettyprinted HTML ========\n");
  output = editor.outputToString("text/html", 3);
  dump(output + "\n\n");

  dump("====== Length and status =====================\n");
  output = "Document is ";
  if (editor.documentIsEmpty)
    output += "empty\n";
  else
    output += "not empty\n";
  output += "Text length is " + editor.textLength + " characters";
  dump(output + "\n\n");
}

function EditorTestTableLayout()
{
  dump("\n\n\n************ Dump Selection Ranges ************\n");
  var selection = GetCurrentEditor().selection;
  var i;
  for (i = 0; i < selection.rangeCount; i++)
  {
    var range = selection.getRangeAt(i);
    if (range)
    {
      dump("Range "+i+": StartParent="+range.startParent+", offset="+range.startOffset+"\n");
    }
  }
  dump("\n\n");

  var editor = GetCurrentEditor();
  var table = editor.getElementOrParentByTagName("table", null);
  if (!table) {
    dump("Enclosing Table not found: Place caret in a table cell to do this test\n\n");
    return;
  }

  var cell;
  var startRowIndexObj = { value: null };
  var startColIndexObj = { value: null };
  var rowSpanObj = { value: null };
  var colSpanObj = { value: null };
  var actualRowSpanObj = { value: null };
  var actualColSpanObj = { value: null };
  var isSelectedObj = { value: false };
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
        cell = editor.getCellDataAt(table, row, col,
                                    startRowIndexObj, startColIndexObj,
                                    rowSpanObj, colSpanObj,
                                    actualRowSpanObj, actualColSpanObj,
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
  rowCount = editor.getTableRowCount(table);
  maxColCount = editor.getTableColumnCount(table);
  dump("From nsITableLayout: Number of rows="+rowCount+" Number of Columns="+maxColCount+"\n****** End of Table Layout Test *****\n\n");
}

function EditorShowEmbeddedObjects()
{
  dump("\nEmbedded Objects:\n");
  try {
    var objectArray = GetCurrentEditor().getEmbeddedObjects();
    dump(objectArray.Count() + " embedded objects\n");
    for (var i=0; i < objectArray.Count(); ++i)
      dump(objectArray.GetElementAt(i) + "\n");
  } catch(e) {}
}

function EditorUnitTests()
{
  dump("Running Unit Tests\n");
  var numTests       = { value:0 };
  var numTestsFailed = { value:0 };
  GetCurrentEditor().debugUnitTests(numTests, numTestsFailed);
}

function EditorTestDocument()
{
  dump("Getting document\n");
  var theDoc = GetCurrentEditor().document;
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

  inputStream.init(theFile, 1, 0, false);    // open read only

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
  try {
    var edlog = GetCurrentEditor().QueryInterface(Components.interfaces.nsIEditorLogging);
    var fs = EditorGetScriptFileSpec();
    edlog.startLogging(fs);
    window._content.focus();

    fs = null;
  }
  catch(ex) { dump("Can't start logging!:\n" + ex + "\n"); }
}

function EditorStopLog()
{
  try {
    var edlog = GetCurrentEditor().QueryInterface(Components.interfaces.nsIEditorLogging);
    edlog.stopLogging();
    window._content.focus();
  }
  catch(ex) { dump("Can't stop logging!:\n" + ex + "\n"); }
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
    var txmgr = GetCurrentEditor().transactionManager;

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
    var txmgr = GetCurrentEditor().transactionManager;

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
      try {
        txn = txn.QueryInterface(Components.interfaces.nsPIEditorTransaction);
        desc = txn.txnDescription;
      } catch(e) {
        desc = "UnknownTxnType";
      }
    }
    dump(prefixStr + "+ " + desc + "\n");
    PrintTxnList(txnList.getChildListForItem(i), prefixStr + "|    ");
  }
}

// ------------------------ 3rd Party Transaction Test ------------------------


function sampleJSTransaction()
{
  this.wrappedJSObject = this;
}

sampleJSTransaction.prototype = {

  isTransient: false,
  mStrData:    "[Sample-JS-Transaction-Content]",
  mObject:     null,
  mContainer:  null,
  mOffset:     null,

  doTransaction: function()
  {
    if (this.mContainer.nodeName != "#text")
    {
      // We're not in a text node, so create one and
      // we'll just insert it at (mContainer, mOffset).

      this.mObject = this.mContainer.ownerDocument.createTextNode(this.mStrData);
    }

    this.redoTransaction();
  },

  undoTransaction: function()
  {
    if (!this.mObject)
      this.mContainer.deleteData(this.mOffset, this.mStrData.length);
    else
      this.mContainer.removeChild(this.mObject);
  },

  redoTransaction: function()
  {
    if (!this.mObject)
      this.mContainer.insertData(this.mOffset, this.mStrData);
    else
      this.insert_node_at_point(this.mObject, this.mContainer, this.mOffset);
  },

  merge: function(aTxn)
  {
    // We don't do any merging!

    return false;
  },

  QueryInterface: function(aUID, theResult)
  {
    if (aUID.equals(Components.interfaces.nsITransaction) ||
        aUID.equals(Components.interfaces.nsISupports))
      return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  },

  insert_node_at_point: function(node, container, offset)
  {
    var childList = container.childNodes;

    if (childList.length == 0 || offset >= childList.length)
      container.appendChild(node);
    else
      container.insertBefore(node, childList.item(offset));
  }
}

function ExecuteJSTransactionViaTxmgr()
{
  try {
    var editor = GetCurrentEditor();
    var txmgr = editor.transactionManager;
    txmgr = txmgr.QueryInterface(Components.interfaces.nsITransactionManager);

    var selection = editor.selection;
    var range =  selection.getRangeAt(0);

    var txn = new sampleJSTransaction();

    txn.mContainer = range.startContainer;
    txn.mOffset = range.startOffset;

    txmgr.doTransaction(txn);
  } catch (e) {
    dump("ExecuteJSTransactionViaTxmgr() failed!");
  }
}

function ExecuteJSTransactionViaEditor()
{
  try {
    var editor = GetCurrentEditor();

    var selection = editor.selection;
    var range =  selection.getRangeAt(0);

    var txn = new sampleJSTransaction();

    txn.mContainer = range.startContainer;
    txn.mOffset = range.startOffset;

    editor.doTransaction(txn);
  } catch (e) {
    dump("ExecuteJSTransactionViaEditor() failed!");
  }
}

