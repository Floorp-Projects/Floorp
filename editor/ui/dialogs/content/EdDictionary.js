var spellChecker;

function Startup()
{
  if (!InitEditorShell())
    return;
  dump("EditoreditorShell found for dialog\n");

  // Get the spellChecker shell
  spellChecker = editorShell.QueryInterface(Components.interfaces.nsIEditorSpellCheck);
  if (!spellChecker) {
    dump("SpellChecker not found!!!\n");
    window.close();
  }

  // Create dialog object to store controls for easy access
  dialog = new Object;
  // GET EACH CONTROL -- E.G.:
  //dialog.editBox = document.getElementById("editBox");

  // SET FOCUS TO FIRST CONTROL
  //dialog.editBox.focus();
}

function SelectWord()
{
  dump("SelectWord\n");
}

function AddWord()
{
  dump("AddWord\n");
}

function ReplaceWord()
{
  dump("ReplaceWord\n");
}

function RemoveWord()
{
  dump("RemoveWord\n");
}


function onOK()
{
  window.close();
}
