var insertNew = true;
var tagName = "anchor"
var anchorElement = null;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;
  dump("EditoreditorShell found for NamedAnchor Properties dialog\n");

  // Create dialog object to store controls for easy access
  dialog = new Object;
  // GET EACH CONTROL -- E.G.:
  dialog.nameInput = document.getElementById("name");

  // Get a single selected element of the desired type
  anchorElement = editorShell.GetSelectedElement(tagName);

  if (anchorElement) {
    // We found an element and don't need to insert one
    insertNew = false;
    dump("Found existing anchor\n");
    dialog.nameInput.value = anchorElement.getAttribute("name");
  } else {
    insertNew = true;
    // We don't have an element selected, 
    //  so create one with default attributes
    dump("Element not selected - calling createElementWithDefaults\n");
    anchorElement = editorShell.CreateElementWithDefaults(tagName);
    // Use the current selection as suggested name
    name = editorShell.selectionAsText;
    // Get 40 characters of the selected text and don't add "..."
    name = TruncateStringAtWordEnd(name, 40, false);
    // Replace whitespace with "_"
    name = ReplaceWhitespace(name, "_");
    dialog.nameInput.value = name;
  }

  if(!anchorElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  }
  
  dialog.nameInput.focus();
}

function onOK()
{
  name = dialog.nameInput.value;
  name = TrimString(name);
  if (name.length == 0) {
    dump("EMPTY ANCHOR STRING\n");
    //TODO: POPUP ERROR DIALOG HERE
  } else {
    // Replace spaces with "_" else it causes trouble in URL parsing
    name = ReplaceWhitespace(name, "_");
    anchorElement.setAttribute("name",name);
    if (insertNew) {
      // Don't delete selected text when inserting
      editorShell.InsertElement(anchorElement, false);
    }
    window.close();
  }
}
