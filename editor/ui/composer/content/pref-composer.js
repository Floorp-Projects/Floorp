// This is mostly a modified version of code in EdColorProps.xul
// TODO: Factor out common code to reduce code size
//       and make a new utility.js to eliminate duplication
//       in editor.js and EdDialogCommon.js

// Initialize in case we can't get them from prefs???
var defaultTextColor="#000000";
var defaultLinkColor="#000099";
var defaultActiveColor="#000099";
var defaultVisitedColor="#990099";
var defaultBackgroundColor="#FFFFFF";

var customTextColor;
var customLinkColor;
var customActiveColor;
var customVisitedColor;
var customBackgroundColor;
var previewBGColor;
var backgroundImage;

// Strings we use often
var styleStr =       "style";
var textStr =        "text";
var linkStr =        "link";
var vlinkStr =       "vlink";
var alinkStr =       "alink";
var bgcolorStr =     "bgcolor";
var backgroundStr =  "background";
var colorStyle =     "color: ";
var backColorStyle = "background-color: ";
var backImageStyle = "; background-image: url(";

var browserColors;
var dialog;

function Startup()
{
  dialog = new Object;
  if (!dialog)
    return false;

  dialog.ColorPreview = document.getElementById("ColorPreview");
  dialog.NormalText = document.getElementById("NormalText");
  dialog.LinkText = document.getElementById("LinkText");
  dialog.ActiveLinkText = document.getElementById("ActiveLinkText");
  dialog.VisitedLinkText = document.getElementById("VisitedLinkText");
  dialog.DefaultColorsRadio = document.getElementById("DefaultColorsRadio");
  dialog.CustomColorsRadio = document.getElementById("CustomColorsRadio");
  dialog.BackgroundImageInput = document.getElementById("BackgroundImageInput");

  // The data elements that hold the pref values
  dialog.NormalData = document.getElementById("textData");
  dialog.LinkData = document.getElementById("linkData");
  dialog.ActiveLinkData = document.getElementById("aLinkData");
  dialog.VisitedLinkData = document.getElementById("fLinkData");
  dialog.BackgroundColorData = document.getElementById("backgroundColorData");
  dialog.BackgroundImageData = document.getElementById("backgroundImageData");

  browserColors = GetDefaultBrowserColors();
  // XXX TODO: This is a problem! If this is false, it overrides ability to display colors set in document!!!
//    var useDocumentColors =    prefs.GetBoolPref("browser.display.use_document_colors");

  // Use author's browser pref colors passed into dialog
  defaultTextColor = browserColors.TextColor;
  defaultLinkColor = browserColors.LinkColor;
  // Note: Browser doesn't store a value for ActiveLinkColor
  defaultActiveColor = defaultLinkColor;
  defaultVisitedColor =  browserColors.VisitedLinkColor;
  defaultBackgroundColor=  browserColors.BackgroundColor;

  // Get the colors and image set by prefs init code 
  customTextColor = dialog.NormalData.getAttribute("value"); 
  customLinkColor = dialog.LinkData.getAttribute("value");
  customActiveColor = dialog.ActiveLinkData.getAttribute("value");
  customVisitedColor = dialog.VisitedLinkData.getAttribute("value");
  customBackgroundColor = dialog.BackgroundColorData.getAttribute("value");
  backgroundImage = dialog.BackgroundImageData.getAttribute("value");
dump(" *** customTextColor="+customTextColor+",customLinkColor="+customLinkColor+",  customActiveColor="+customActiveColor+", customVisitedColor="+customVisitedColor+", customBackgroundColor="+customBackgroundColor+"\n");
  if (backgroundImage)
    dialog.BackgroundImageInput.value = backgroundImage;

  if( document.getElementById( "useCustomColors" ).data == 0 )
    UseDefaultColors();
  else
    UseCustomColors();

  return true;
}                   

function GetColorAndUpdate(ColorWellID)
{
  // Only allow selecting when in custom mode
  if (!dialog.CustomColorsRadio.checked) return;

  var colorObj = new Object;
  var colorWell = document.getElementById(ColorWellID);
  if (!colorWell) return;

  // Don't allow a blank color, i.e., using the "default"
  colorObj.NoDefault = true;

  switch( ColorWellID )
  {
    case "textCW":
      colorObj.Type = "Text";
      colorObj.TextColor = customTextColor;
      break;
    case "linkCW":
      colorObj.Type = "Link";
      colorObj.TextColor = customLinkColor;
      break;
    case "activeCW":
      colorObj.Type = "ActiveLink";
      colorObj.TextColor = customActiveColor;
      break;
    case "visitedCW":
      colorObj.Type = "VisitedLink";
      colorObj.TextColor = customVisitedColor;
      break;
    case "backgroundCW":
      colorObj.Type = "Page";
      colorObj.PageColor = customBackgroundColor;
      break;
  }

  window.openDialog("chrome://editor/content/EdColorPicker.xul", "_blank", "chrome,close,titlebar,modal", "", colorObj);

  // User canceled the dialog
  if (colorObj.Cancel)
    return;

  var color = "";
  switch( ColorWellID )
  {
    case "textCW":
      color = customTextColor = colorObj.TextColor;
      dialog.NormalData.setAttribute("value", color); 
      break;
    case "linkCW":
      color = customLinkColor = colorObj.TextColor;
      dialog.LinkData.setAttribute("value", color);
      break;
    case "activeCW":
      color = customActiveColor = colorObj.TextColor;
      dialog.ActiveLinkData.setAttribute("value", color);
      break;
    case "visitedCW":
      color = customVisitedColor = colorObj.TextColor;
      dialog.VisitedLinkData.setAttribute("value", color);
      break;
    case "backgroundCW":
      color = customBackgroundColor = colorObj.BackgroundColor;
      dialog.BackgroundColorData.setAttribute("value", color);
      break;
  }
dump(" *** customTextColor="+customTextColor+",customLinkColor="+customLinkColor+",  customActiveColor="+customActiveColor+", customVisitedColor="+customVisitedColor+", customBackgroundColor="+customBackgroundColor+"\n");

  setColorWell(ColorWellID, color); 
  SetColorPreview(ColorWellID, color);
}

function SetColorPreview(ColorWellID, color)
{
  switch( ColorWellID )
  {
    case "textCW":
      dialog.NormalText.setAttribute(styleStr,colorStyle+color);
      break;
    case "linkCW":
      dialog.LinkText.setAttribute(styleStr,colorStyle+color);
      break;
    case "activeCW":
      dialog.ActiveLinkText.setAttribute(styleStr,colorStyle+color);
      break;
    case "visitedCW":
      dialog.VisitedLinkText.setAttribute(styleStr,colorStyle+color);
      break;
    case "backgroundCW":
      // Must combine background color and image style values
      var styleValue = backColorStyle+color;
      if (backgroundImage)
        styleValue += ";"+backImageStyle+backgroundImage+");";

      dialog.ColorPreview.setAttribute(styleStr,styleValue);
      previewBGColor = color;
      break;
  }
}

function UseCustomColors()
{
  SetElementEnabledById("TextButton", true);
  SetElementEnabledById("LinkButton", true);
  SetElementEnabledById("ActiveLinkButton", true);
  SetElementEnabledById("VisitedLinkButton", true);
  SetElementEnabledById("BackgroundButton", true);
  SetElementEnabledById("Text", true);
  SetElementEnabledById("Link", true);
  SetElementEnabledById("Active", true);
  SetElementEnabledById("Visited", true);
  SetElementEnabledById("Background", true);

  SetColorPreview("textCW",       customTextColor);
  SetColorPreview("linkCW",       customLinkColor);
  SetColorPreview("activeCW",     customActiveColor);
  SetColorPreview("visitedCW",    customVisitedColor);
  SetColorPreview("backgroundCW", customBackgroundColor);

  setColorWell("textCW",          customTextColor);
  setColorWell("linkCW",          customLinkColor);
  setColorWell("activeCW",        customActiveColor);
  setColorWell("visitedCW",       customVisitedColor);
  setColorWell("backgroundCW",    customBackgroundColor);

dump(" *** customTextColor="+customTextColor+",customLinkColor="+customLinkColor+",  customActiveColor="+customActiveColor+", customVisitedColor="+customVisitedColor+", customBackgroundColor="+customBackgroundColor+"\n");
  dialog.NormalData.setAttribute("value",          customTextColor); 
  dialog.LinkData.setAttribute("value",            customLinkColor);
  dialog.ActiveLinkData.setAttribute("value",      customActiveColor);
  dialog.VisitedLinkData.setAttribute("value",     customVisitedColor);
  dialog.BackgroundColorData.setAttribute("value", customBackgroundColor);
}

function UseDefaultColors()
{
  SetColorPreview("textCW",       defaultTextColor);
  SetColorPreview("linkCW",       defaultLinkColor);
  SetColorPreview("activeCW",     defaultActiveColor);
  SetColorPreview("visitedCW",    defaultVisitedColor);
  SetColorPreview("backgroundCW", defaultBackgroundColor);

  // Setting to blank color will remove color from buttons,
  setColorWell("textCW",       "");
  setColorWell("linkCW",       "");
  setColorWell("activeCW",     "");
  setColorWell("visitedCW",    "");
  setColorWell("backgroundCW", "");

  // Disable color buttons and labels
  SetElementEnabledById("TextButton", false);
  SetElementEnabledById("LinkButton", false);
  SetElementEnabledById("ActiveLinkButton", false);
  SetElementEnabledById("VisitedLinkButton", false);
  SetElementEnabledById("BackgroundButton", false);
  SetElementEnabledById("Text", false);
  SetElementEnabledById("Link", false);
  SetElementEnabledById("Active", false);
  SetElementEnabledById("Visited", false);
  SetElementEnabledById("Background", false);
  
  // Note that we leave custom colors set even if 
  //  custom colors pref is false (we just ignore the colors)
}
  
function ChooseImageFile()
{
  // Get a local image file, converted into URL format
  var fileName = GetLocalFileURL("img");
  if (fileName)
  {
    dialog.BackgroundImageInput.value = fileName;
    ValidateAndPreviewImage(true);
  }
  SetTextfieldFocus(dialog.BackgroundImageInput);
}

function ChangeBackgroundImage()
{
  // Don't show error message for image while user is typing
  ValidateAndPreviewImage(false);
}

function ValidateAndPreviewImage(ShowErrorMessage)
{
  // First make a string with just background color
  var styleValue = backColorStyle+previewBGColor+";";

  var image = dialog.BackgroundImageInput.value.trimString();
  if (image)
  {
    if (IsValidImage(image))
    {
      backgroundImage = image;
      // Append image style
      styleValue += backImageStyle+backgroundImage+");";
    }
    else
    {
      backgroundImage = null;
      if (ShowErrorMessage)
      {
        SetTextfieldFocus(dialog.BackgroundImageInput);
        // Tell user about bad image
        ShowInputErrorMessage(GetString("MissingImageError"));
      }
    }
  }
  else
    backgroundImage = null;

  // Set style on preview (removes image if not valid)
  dialog.ColorPreview.setAttribute(styleStr, styleValue);
  
  // Set the pref data so pref code saves it 
  document.getElementById("backgroundImageData").setAttribute("value", backgroundImage);
}

