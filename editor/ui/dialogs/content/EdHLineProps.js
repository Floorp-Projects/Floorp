var toolkitCore;
var tagName = "hr";
var hLineElement;
var tempLineElement;
var percentChar = "";
var shading = true;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  // Get the selected horizontal line
  hLineElement = editorShell.GetSelectedElement(tagName);

  if (!hLineElement) {
    // We should never be here if not editing an existing HLine
    dump("HLine is not selected! Shouldn't be here!\n");
    window.close();
    return;
  }
  // Create a temporary element to use with Save Settings as default
  tempLineElement = editorShell.editorDocument.createElement("HR");
  if (!hLineElement) {
    dump("Temporary HLine element was not created!\n");
    window.close();
    return;
  }

  // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.heightInput = document.getElementById("height");
  dialog.widthInput = document.getElementById("width");
  dialog.leftAlign = document.getElementById("leftAlign");
  dialog.centerAlign = document.getElementById("centerAlign");
  dialog.rightAlign = document.getElementById("rightAlign");
  dialog.shading = document.getElementById("3dShading");

 
  // Initialize control values based on existing attributes

    // Just to be confusing, "size" is used instead of height
    // We will use "height" here and in UI
  dialog.heightInput.value = hLineElement.getAttribute("size");

  // Get the width attribute of the element, stripping out "%"
  // This sets contents of button text and "percentChar" variable
  dialog.widthInput.value = InitPixelOrPercentPopupButton(hLineElement, "width", "pixelOrPercentButton");

  align = hLineElement.getAttribute("align");
  if (align == "center") {
    dialog.centerAlign.checked = true;
  } else if (align == "right") {
    dialog.rightAlign.checked = true;
  } else {
    dialog.leftAlign.checked = true;
  }
  noshade = hLineElement.getAttribute("noshade");
  dialog.shading.checked = (noshade == "");

  // SET FOCUS TO FIRST CONTROL
  dialog.heightInput.focus();
}

function onSaveDefault()
{
  // "false" means set attributes on the tempLineElement,
  //   not the real element being edited
  if (ValidateData(false)) {
    editorShell.SaveHLineSettings(tempLineElement);
    dump("Saving HLine settings to preferences\n");
  }
}

function onAdvanced()
{
  //TODO: Call the generic attribute editor ("extra HTML")
}

function ValidateData(setAttributes)
{
  // Height is always pixels
  height = ValidateNumberString(dialog.heightInput.value, 1, maxPixels);
  if (height == "") {
    // Set focus to the offending control
    dump("Height is empty\n");
    dialog.heightInput.focus();
    return false;
  }
  dump("Setting height="+height+"\n");
  if (setAttributes) {
    hLineElement.setAttribute("size", height);
  } else {
    tempLineElement.setAttribute("size", height);
  }

  var maxLimit;
  dump("Validate width. PercentChar="+percentChar+"\n");
  if (percentChar == "%") {
    maxLimit = 100;
  } else {
    // Upper limit when using pixels
    maxLimit = maxPixels;
  }

  width = ValidateNumberString(dialog.widthInput.value, 1, maxLimit);
  if (width == "") {
    dump("Width is empty\n");
    dialog.widthInput.focus();
    return false;
  }
  width = width + percentChar;
  dump("Height="+height+" Width="+width+"\n");
  if (setAttributes) {
    hLineElement.setAttribute("width", width);
  } else {
    tempLineElement.setAttribute("width", width);
  }


  align = "left";
  if (dialog.centerAlign.checked) {
    align = "center";
  } else if (dialog.rightAlign.checked) {
    align = "right";
  }
  if (setAttributes) {
    hLineElement.setAttribute("align", align);
  } else {
    tempLineElement.setAttribute("align", align);
  }

  if (dialog.shading.checked) {
    if (setAttributes) {
      hLineElement.removeAttribute("noshade");
    } else {
      tempLineElement.removeAttribute("noshade");
    }
  } else {
    if (setAttributes) {
      hLineElement.setAttribute("noshade", "");
    } else {
      tempLineElement.setAttribute("noshade", "");
    }
  }
  return true;
}

function onOK()
{
  dump("************* onOK for HLine props\n");
  // Since we only edit existing HLines, 
  //  ValidateData will set the new attributes
  //   so there's nothing else to do
  if (ValidateData(true)) {
    window.close();
  }  
}
