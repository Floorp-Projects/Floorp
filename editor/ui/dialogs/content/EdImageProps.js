/* 
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Pete Collins
 *   Brian King
 *   Ben Goodger
 */

var insertNew     = true;
var SeeMore       = true;
var wasEnableAll  = false;
var oldSourceInt  = 0;
var imageElement;

// dialog initialization code

function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, null);

  // Create dialog object to store controls for easy access
  dialog = new Object;

  dialog.srcInput          = document.getElementById( "srcInput" );
  dialog.altTextInput      = document.getElementById( "altTextInput" );
  dialog.MoreFewerButton   = document.getElementById( "MoreFewerButton" );
  dialog.MoreSection       = document.getElementById( "MoreSection" );
  dialog.customsizeRadio   = document.getElementById( "customsizeRadio" );
  dialog.constrainCheckbox = document.getElementById( "constrainCheckbox" );
  dialog.widthInput        = document.getElementById( "widthInput" );
  dialog.heightInput       = document.getElementById( "heightInput" );
  dialog.widthUnitsSelect  = document.getElementById( "widthUnitsSelect" );
  dialog.heightUnitsSelect = document.getElementById( "heightUnitsSelect" );
  dialog.imagelrInput      = document.getElementById( "imageleftrightInput" );
  dialog.imagetbInput      = document.getElementById( "imagetopbottomInput" );
  dialog.border            = document.getElementById( "border" );
  dialog.alignTypeSelect   = document.getElementById( "alignTypeSelect" );

  // Set SeeMore bool to the OPPOSITE of the current state,
  //   which is automatically saved by using the 'persist="more"' 
  //   attribute on the MoreFewerButton button
  //   onMoreFewer will toggle the state and redraw the dialog
  SeeMore = (dialog.MoreFewerButton.getAttribute("more") != "1");
  onMoreFewer();

  // Get a single selected image element
  var tagName = "img"
  imageElement = editorShell.GetSelectedElement(tagName);

  if (imageElement) 
  {
    // We found an element and don't need to insert one
    insertNew = false;
  } 
  else
  {
    insertNew = true;

    // We don't have an element selected, 
    //  so create one with default attributes

    imageElement = editorShell.CreateElementWithDefaults(tagName);
    if( !imageElement )
    {
      dump("Failed to get selected element or create a new one!\n");
      window.close();
    }  
  }

  // Make a copy to use for AdvancedEdit
  globalElement = imageElement.cloneNode(false);
  
  // Initialize all widgets with image attributes
  InitDialog();
  
  dialog.srcInput.focus();
}

// Set dialog widgets with attribute data
// We get them from globalElement copy so this can be used
//   by AdvancedEdit(), which is shared by all property dialogs
function InitDialog()
{
  // Set the controls to the image's attributes

  str = globalElement.getAttribute("src");
  if (str)
    dialog.srcInput.value = str;
  
  str = globalElement.getAttribute("alt");
  if (str)
    dialog.altTextInput.value = str;
  
  // setup the height and width widgets
  dialog.widthInput.value = InitPixelOrPercentCombobox(globalElement, "width", "widthUnitsSelect");
  dialog.heightInput.value = InitPixelOrPercentCombobox(globalElement, "height", "heightUnitsSelect");
  
  // TODO: We need to get the actual image dimensions.
  //       If different from attribute dimensions, then "custom" is checked.
  // For now, always check custom, so we don't trash existing values
  dialog.customsizeRadio.checked = true;
  
  dump(dialog.customsizeRadio.checked+" dialog.customsizeRadio.checked\n");

  // set spacing editfields
  dialog.imagelrInput.value = globalElement.getAttribute("hspace");
  dialog.imagetbInput.value = globalElement.getAttribute("vspace");
  dialog.border.value       = globalElement.getAttribute("border");    

  // Get alignment setting  
  var align = globalElement.getAttribute("align");
  if (align) {
    align.toLowerCase();
dump("Image Align exists = "+align+"\n");
  }

dump("Image Align="+align+"\n");
  switch ( align )
  {
    case "top":
      dialog.alignTypeSelect.selectedIndex = 0;
      break;
    case "center":
      dialog.alignTypeSelect.selectedIndex = 1;
      break;
    case "left":
      dialog.alignTypeSelect.selectedIndex = 3;
      break;
    case "right":
      dialog.alignTypeSelect.selectedIndex = 4;
      break;
    default:  // Default or "bottom"
      dialog.alignTypeSelect.selectedIndex = 2;
      break;
  }

dump( "Image Align Select Index after setting="+dialog.alignTypeSelect.selectedIndex+"\n");

  imageTypeExtension = checkForImage();
  // we want to force an update so initialize "wasEnableAll" to be the opposite of what the actual state is
  wasEnableAll = !imageTypeExtension;
  doOverallEnabling();
}

function chooseFile()
{
  // Get a local file, converted into URL format

  fileName = editorShell.GetLocalFileURL(window, "img");
  if (fileName && fileName != "") {
    dialog.srcInput.value = fileName;
//  imageTypeExtension  = checkForImage();
    doValueChanged();
  }
  
  checkForImage( "srcInput" );
  
  // Put focus into the input field
  dialog.srcInput.focus();
}

function onMoreFewer()
{
  if (SeeMore)
  {
    dialog.MoreSection.setAttribute("style","display: none");
    window.sizeToContent();
    dialog.MoreFewerButton.setAttribute("more","0");
    dialog.MoreFewerButton.setAttribute("value",GetString("MoreProperties"));
    SeeMore = false;
  }
  else
  {
    dialog.MoreSection.setAttribute("style","display: inherit");
    window.sizeToContent();
    dialog.MoreFewerButton.setAttribute("more","1");
    dialog.MoreFewerButton.setAttribute("value",GetString("FewerProperties"));
    SeeMore = true;
  }
}

function doValueChanged()
{
    doOverallEnabling();
}

function OnChangeSrc()
{
  doValueChanged();
}

function doDimensionEnabling( doEnable )
{
  // Make associated text labels look disabled or not
  SetElementEnabledByID( "originalsizeLabel", doEnable );
  SetElementEnabledByID( "customsizeLabel", doEnable );
  SetElementEnabledByID( "dimensionsLabel", doEnable );

  SetElementEnabledByID( "originalsizeRadio", doEnable );
  SetElementEnabledByID( "customsizeRadio", doEnable );

  // Rest are enabled only if "Custom" is checked
  var enable = (doEnable && dialog.customsizeRadio.checked);

  SetElementEnabledByID( "widthInput", enable );
  SetElementEnabledByID( "widthLabel", enable);
  SetElementEnabledByID( "widthUnitsSelect", enable );

  SetElementEnabledByID( "heightInput", enable );
  SetElementEnabledByID( "heightLabel", enable );
  SetElementEnabledByID( "heightUnitsSelect", enable );


  SetElementEnabledByID( "constrainCheckbox", enable );
  SetElementEnabledByID( "constrainLabel", enable );
}

function doOverallEnabling()
{
  var imageTypeExtension = checkForImage();
  var canEnableAll       = imageTypeExtension != 0;
  if ( wasEnableAll == canEnableAll )
    return;
  
  wasEnableAll = canEnableAll;

  SetElementEnabledByID("ok", canEnableAll );
  SetElementEnabledByID( "altTextLabel", canEnableAll );

  // Do widgets for sizing
  doDimensionEnabling( canEnableAll );
  
  SetElementEnabledByID("alignLabel", canEnableAll );
  SetElementEnabledByID("alignTypeSelect", canEnableAll );

  // spacing fieldset
  SetElementEnabledByID( "spacingLabel", canEnableAll );
  SetElementEnabledByID( "imageleftrightInput", canEnableAll );
  SetElementEnabledByID( "leftrightLabel", canEnableAll );
  SetElementEnabledByID( "leftrighttypeLabel", canEnableAll );

  SetElementEnabledByID( "imagetopbottomInput", canEnableAll );
  SetElementEnabledByID( "topbottomLabel", canEnableAll );
  SetElementEnabledByID( "topbottomtypeLabel", canEnableAll );

  SetElementEnabledByID( "border", canEnableAll );
  SetElementEnabledByID( "borderLabel", canEnableAll );
  SetElementEnabledByID( "bordertypeLabel", canEnableAll );

  SetElementEnabledByID( "AdvancedEdit", canEnableAll );
}

// an API to validate and image by sniffing out the extension
/* assumes that the element id is "srcInput" */
/* returns lower-case extension or 0 */
function checkForImage()
{
  image = dialog.srcInput.value.trimString();
  if ( !image )
    return 0;
  
  /* look for an extension */
  var tailindex = image.lastIndexOf("."); 
  if ( tailindex == 0 || tailindex == -1 ) /* -1 is not found */
    return 0; 
  
  /* move past period, get the substring from the first character after the '.' to the last character (length) */
  tailindex = tailindex + 1;
  var type = image.substring(tailindex,image.length);
  
  /* convert extension to lower case */
  if (type)
    type = type.toLowerCase();
  
  // TODO: Will we convert .BMPs to a web format?
  switch( type )  {
    case "gif":
    case "jpg":
    case "jpeg":
    case "png":
      return type;
      break;
    default :
     return 0;
  }
}


// constrainProportions contribution by pete@postpagan.com
// needs to handle pixels/percent

function constrainProportions( srcID, destID )
{
  srcElement = document.getElementById ( srcID );
  if ( !srcElement )
    return;
  
  forceInteger( srcID );
  
  // now find out if we should be constraining or not

  var constrainChecked = (dialog.constrainCheckbox.checked);
  if ( !constrainChecked )
    return;
  
  destElement = document.getElementById( destID );
  if ( !destElement )
    return;
  
  // set new value in the other edit field
  // src / dest ratio mantained
  // newDest = (newSrc * oldDest / oldSrc)

  if ( oldSourceInt == 0 )
    destElement.value = srcElement.value;
  else
    destElement.value = Math.round( srcElement.value * destElement.value / oldSourceInt );
  
  oldSourceInt = srcElement.value;
}

// Get data from widgets, validate, and set for the global element
//   accessible to AdvancedEdit() [in EdDialogCommon.js]
function ValidateData()
{
  var imageTypeExtension  = checkForImage();
  if ( !imageTypeExtension ) {
    ShowInputErrorMessage(GetString("MissingImageError"));
    return false;
  }

  //TODO: WE NEED TO DO SOME URL VALIDATION HERE, E.G.:
  // We must convert to "file:///" or "http://" format else image doesn't load!
  var src = dialog.srcInput.value.trimString();
  globalElement.setAttribute("src", src);
  
  // TODO: Should we confirm with user if no alt tag? Or just set to empty string?
  var alt = dialog.altTextInput.value.trimString();
  globalElement.setAttribute("alt", alt);
  
  var isPercentWidth = (dialog.widthUnitsSelect.selectedIndex == 1);
  var maxLimitWidth = isPercentWidth ? 100 : maxPixels;  // Defined in EdDialogCommon.js
  var isPercentHeight = (dialog.heightUnitsSelect.selectedIndex == 1);
  var maxLimitHeight = isPercentHeight ? 100 : maxPixels;  // Defined in EdDialogCommon.js

  //TODO: WE SHOULD ALWAYS SET WIDTH AND HEIGHT FOR FASTER IMAGE LAYOUT
  //  IF USER DOESN'T SET IT, WE NEED TO GET VALUE FROM ORIGINAL IMAGE 
  var width = "";
  var height = "";
  if ( dialog.customsizeRadio.checked )
  {
    width = ValidateNumberString(dialog.widthInput.value, 1, maxLimitWidth);
    if (width == "") {
      dump("Image Width is empty\n");
      dialog.widthInput.focus();
      return false;
    }
    if (isPercentWidth)
      width = width + "%";

    height = ValidateNumberString(dialog.heightInput.value, 1, maxLimitHeight);
    if (height == "") {
      dump("Image Height is empty\n");
      dialog.heightInput.focus();
      return false;
    }
    if (isPercentHeight)
      height = height + "%";
  }

  dump("Image width,heigth: "+width+","+height+"\n");
  if (width.length > 0)
    globalElement.setAttribute("width", width);
  if (height.length > 0)
    globalElement.setAttribute("width", width);
  
  // spacing attributes
  // All of these should use ValidateNumberString() to 
  //  ensure value is within acceptable range
  if ( dialog.imagelrInput.value.length > 0 )
  {
    var hspace = ValidateNumberString(dialog.imagelrInput.value, 0, maxPixels);
    if (hspace == "")
      return false;
    globalElement.setAttribute( "hspace", hspace );
  }
  else
    globalElement.removeAttribute( "hspace" );
  
  if ( dialog.imagetbInput.value.length > 0 )
  {
    var vspace = ValidateNumberString(dialog.imagetbInput.value, 0, maxPixels);
    if (vspace == "")
      return false;
    globalElement.setAttribute( "vspace", vspace );
  }
  else
    globalElement.removeAttribute( "vspace" );
  
  // note this is deprecated and should be converted to stylesheets

  if ( dialog.border.value.length > 0 )
  {
    var border = ValidateNumberString(dialog.border.value, 0, maxPixels);
    if (border == "")
      return false;
    globalElement.setAttribute( "border", border );
  }
  else
    globalElement.removeAttribute( "border" );

  // Default or setting "bottom" means don't set the attribute
  var align = "";
  switch ( dialog.alignTypeSelect.selectedIndex )
  {
    case 0:
      align = "top";
      break;
    case 1:
      align = "center";
      break;
    case 3:
      break;
    case 4:
      break;
  }
  if (align == "")
    globalElement.removeAttribute( "align" );
  else
    globalElement.setAttribute( "align", align );

  return true;
}

function onOK()
{
  // handle insertion of new image
  if (ValidateData())
  {
    // All values are valid - copy to actual element in doc or 
    //   element created to insert
    editorShell.CloneAttributes(imageElement, globalElement);
    if (insertNew)
    {
      try {
        // 'true' means delete the selection before inserting
        editorShell.InsertElementAtSelection(imageElement, true);
      } catch (e) {
        dump("Exception occured in InsertElementAtSelection\n");
      }
    }
    return true;
  }
  return false;
}

