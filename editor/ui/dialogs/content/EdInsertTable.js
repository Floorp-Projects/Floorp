//Cancel() is in EdDialogCommon.js
var editorShell;
var tagName = "table"
var tableElement = null;
var rowElement = null;
var cellElement = null;
var maxRows = 10000;
var maxColumns = 10000;

// dialog initialization code
function Startup()
{
  // get the editor shell from the parent window
  editorShell = window.opener.editorShell;
  editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
  if(!editorShell) {
    dump("EditoreditorShell not found!!!\n");
    window.close();
    return;
  }
  dump("EditoreditorShell found for Insert Table dialog\n");

  // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.rowsInput = document.getElementById("rows");
  dialog.columnsInput = document.getElementById("columns");

  // Set default number to 1 row, 2 columns:
  dialog.rowsInput.value = 1;
  dialog.columnsInput.value = 2;

  tableElement = editorShell.CreateElementWithDefaults(tagName);

  if(!tableElement)
  {
    dump("Failed to create a new table!\n");
    window.close();
  }
  
  dialog.rowsInput.focus();
}

function onOK()
{
  rows = ValidateNumberString(dialog.rowsInput.value, 1, maxRows);
  if (rows == "") {
    // Set focus to the offending control
    dialog.rowsInput.focus();
    return;
  }

  columns = ValidateNumberString(dialog.columnsInput.value, 1, maxColumns);
  if (columns == "") {
    // Set focus to the offending control
    dialog.columnsInput.focus();
    return;
  }
  dump("Rows = "+rows+"  Columns = "+columns+"\n");
  for (i = 0; i < rows; i++)
  {
    newRow = editorShell.CreateElementWithDefaults("tr");
    dump("New row "+i+": "+newRow+"\n");
    if (newRow)
    {
      tableElement.appendChild(newRow);
      for (j = 0; j < columns; j++)
      {
        newCell = editorShell.CreateElementWithDefaults("td");
        dump("New cell "+j+": "+newCell+"\n");
        if (newCell)
        {
          newRow.appendChild(newCell);
        }
      }
    }
  }

  // TODO: VALIDATE ROWS, COLS and BUILD TABLE
  // Don't delete selected text when inserting
  editorShell.InsertElement(tableElement, false);
  window.close();
}
