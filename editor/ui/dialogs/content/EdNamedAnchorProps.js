var insertNew = true;
var tagName = "anchor"
var anchorElement = null;
var nameInput;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;
  dump("EditorShell found for NamedAnchor Properties dialog\n");
  dump(document+"\n");
  nameInput = document.getElementById("nameInput");
  dump(nameInput+"\n");

  // Get a single selected element of the desired type
  anchorElement = editorShell.GetSelectedElement(tagName);

  if (anchorElement) {
    // We found an element and don't need to insert one
    insertNew = false;
    dump("Found existing anchor\n");
    nameInput.value = anchorElement.getAttribute("name");
  } else {
    insertNew = true;
    // We don't have an element selected, 
    //  so create one with default attributes
    dump("Element not selected - calling createElementWithDefaults\n");
    anchorElement = editorShell.CreateElementWithDefaults(tagName);
    // Use the current selection as suggested name
    name = GetSelectionAsText();
    // Get 40 characters of the selected text and don't add "..."
    name = TruncateStringAtWordEnd(name, 40, false);
    // Replace whitespace with "_"
    name = ReplaceWhitespace(name, "_");
    dump("Selection text for name: "+name+"\n");
    nameInput.value = name;
  }

  if(!anchorElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  }
  
  nameInput.focus();
}

function onOK()
{
  name = nameInput.value;
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
