var editorShell;
var toolkitCore;
var tagName = "hr";
var hLineElement;
var tempLineElement;
var percentChar = "";
var maxPixels = 10000;
var shading = true;

// dialog initialization code
function Startup()
{
  // get the editor shell from the parent window
  editorShell = window.opener.editorShell;
  editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
  if(!editorShell) {
    dump("editorShell not found!!!\n");
    window.close();
    return;
  }

  // Get the selected horizontal line
  hLineElement = window.editorShell.GetSelectedElement(tagName);

  if (!hLineElement) {
    // We should never be here if not editing an existing HLine
    dump("HLine is not selected! Shouldn't be here!\n");
    window.close();
    return;
  }
  // Create a temporary element to use with Save Settings as default
  tempLineElement = window.editorShell.editorDocument.createElement("HR");
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
  dialog.pixelOrPercentButton = document.getElementById("pixelOrPercentButton");

  // Initialize control values based on existing attributes

  dialog.heightInput.value = hLineElement.getAttribute("height");
  width = hLineElement.getAttribute("width");

  // This assumes initial button text is "percent"
  // Search for a "%" character
  percentIndex = width.search(/%/);
  if (percentIndex > 0) {
    percentChar = "%";
    // Strip out the %
    width = width.substr(0, percentIndex);
  } else {
    dialog.pixelOrPercentButton.setAttribute("value","pixels");
  }

  dialog.widthInput.value = width;

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

// Input string is "" for pixel, or "%" for percent
function SetPixelOrPercent(percentString)
{
  percentChar = percentString;
  dump("SetPixelOrPercent. PercentChar="+percentChar+"\n");

  if (percentChar == "%") {
    dialog.pixelOrPercentButton.setAttribute("value","percent");
    dump("TODO: Set button text to PERCENT\n");
  } else {
    dialog.pixelOrPercentButton.setAttribute("value","pixels");
    dump("TODO: Set button text to PIXELS\n");
  }
  
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
    dialog.heightInput.focus();
    return false;
  }
  dump("Setting height="+height+"\n");
  if (setAttributes) {
    hLineElement.setAttribute("height", height);
  } else {
    hLineElement.setAttribute("height", height);
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
  // Since we only edit existing HLines, 
  //  ValidateData will set the new attributes
  //   so there's nothing else to do
  if (ValidateData(true)) {
    window.close();
    dump("CLOSING EdHLineProps\n");
  }  
}
