var editorShell;
var toolkitCore;
var tagName = "hr";
var hLineElement;
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

dump("HLINE PROPS 1\n");

  // Get the selected horizontal line
  hLineElement = window.editorShell.GetSelectedElement(tagName);

dump("HLINE PROPS 2\n");

  if (!hLineElement) {
    // We should never be here if not editing an existing HLine
    dump("HLine is not selected! Shouldn't be here!\n");
    window.close();
    return;
  }
dump("HLINE PROPS 3\n");

  // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.heightInput = document.getElementById("height");
  dialog.widthInput = document.getElementById("width");
  dialog.leftAlign = document.getElementById("leftAlign");
  dialog.centerAlign = document.getElementById("centerAlign");
  dialog.rightAlign = document.getElementById("rightAlign");
  dialog.shading = document.getElementById("3dShading");
  dialog.pixelOrPercentButton = document.getElementById("pixelOrPercentButton");

dump("HLINE PROPS 4\n");
  // Initialize control values based on existing attributes

  dialog.heightInput.value = hLineElement.getAttribute("height");
  width = hLineElement.getAttribute("width");
  // Use convert to number, then back to string to strip of "%"
  dialog.widthInput.value = (width - 0) + "";

dump("HLINE PROPS 5\n");

  // This assumes initial button text is "pixels"
  if (width.search(/%/) > 0) {
    dialog.pixelOrPercentButton.value = "percent";
    percentChar = "%";
  }

dump("HLINE PROPS 6\n");

  align = hLineElement.getAttribute("align");
  if (align == "center") {
    dialog.centerAlign.checked = true;
  } else if (align == "right") {
    dialog.rightAlign.checked = true;
  } else {
    dialog.leftAlign.checked = true;
  }
  noshade = hLineElement.getAttribute("noshade");
  dump("Width ="+width+" Align ="+align+" NoShade="+noshade+"\n");
  dialog.shading.checked = (noshade == "");

dump("HLINE PROPS 3\n");
  // SET FOCUS TO FIRST CONTROL
  dialog.heightInput.focus();
}

// Input string is "" for pixel, or "%" for percent
function SetPixelOrPercent(percentString)
{
  percentChar = percentString;
  dump("SetPixelOrPercent. PercentChar="+percentChar+"\n");

  if (percentChar == "%") {
//    dialog.pixelOrPercentButton.value = "percent";
    dump("TODO: Set button text to PERCENT\n");
  } else {
//    dialog.pixelOrPercentButton.value = "pixels";
    dump("TODO: Set button text to PIXELS\n");
  }
  
}

function onSaveDefault()
{
  if (ValidateData()) {
    editorShell.SaveHLineSettings(hLineElement);
    dump("Saving HLine settings to preferences\n");
  }
}

function onAdvanced()
{
  //TODO: Call the generic attribute editor ("extra HTML")
}

function ValidateData()
{
  // Height is always pixels
  height = ValidateNumberString(dialog.heightInput.value, 1, maxPixels);
  if (height == "") {
    // Set focus to the offending control
    dialog.heightInput.focus();
    return false;
  }
  hLineElement.setAttribute("height", height);

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
  hLineElement.setAttribute("width", width);

  align = "left";
  if (dialog.centerAlign.checked) {
    align = "center";
  } else if (dialog.rightAlign.checked) {
    align = "right";
  }
  hLineElement.setAttribute("align", align);

  if (dialog.shading.checked) {
    hLineElement.removeAttribute("noshade");
  } else {
    hLineElement.setAttribute("noshade", "");
  }
  return true;
}

function OnOK()
{
  if (ValidateData()) {
    window.close();
    dump("CLOSING EdHLineProps\n");
  }  
}
