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
var wasEnableAll  = false;
var oldSourceInt  = 0;
var imageElement;
var canRemoveImageMap = false;
var imageMapDisabled = false;

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
  dialog.originalsizeRadio = document.getElementById( "originalsizeRadio" );
  dialog.constrainCheckbox = document.getElementById( "constrainCheckbox" );
  dialog.widthInput        = document.getElementById( "widthInput" );
  dialog.heightInput       = document.getElementById( "heightInput" );
  dialog.widthUnitsSelect  = document.getElementById( "widthUnitsSelect" );
  dialog.heightUnitsSelect = document.getElementById( "heightUnitsSelect" );
  dialog.imagelrInput      = document.getElementById( "imageleftrightInput" );
  dialog.imagetbInput      = document.getElementById( "imagetopbottomInput" );
  dialog.border            = document.getElementById( "border" );
  dialog.alignTypeSelect   = document.getElementById( "alignTypeSelect" );
  dialog.editImageMap      = document.getElementById( "editImageMap" );
  dialog.removeImageMap    = document.getElementById( "removeImageMap" );
  dialog.doConstrain = false;
  
  // Another version of button just for this dialog -- on same line as "More Properties"
  dialog.AdvancedEditButton2 = document.getElementById( "AdvancedEditButton2" );

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
  
  // Set SeeMore bool to the OPPOSITE of the current state,
  //   which is automatically saved by using the 'persist="more"' 
  //   attribute on the MoreFewerButton button
  //   onMoreFewerImage will toggle the state and redraw the dialog
  SeeMore = (dialog.MoreFewerButton.getAttribute("more") != "1");

  // Initialize widgets with image attributes in the case where the entire dialog isn't visible
  if ( SeeMore ) // this is actually in the opposite state until onMoreFewerImage is called below
    InitDialog();
  
  onMoreFewerImage();  // this call will initialize all widgets if entire dialog is visible
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
  
  if ( SeeMore )
  {
	  // setup the height and width widgets
	  dialog.widthInput.value = InitPixelOrPercentCombobox(globalElement, "width", "widthUnitsSelect");
	  dialog.heightInput.value = InitPixelOrPercentCombobox(globalElement, "height", "heightUnitsSelect");
	  
	  // TODO: We need to get the actual image dimensions.
	  //       If different from attribute dimensions, then "custom" is checked.
	  // For now, always check custom, so we don't trash existing values
	  if ( dialog.widthInput.value.length && dialog.heightInput.value.length 
	    && dialog.widthInput.value != "0" && dialog.heightInput.value != "0")
	  {
	    dialog.isCustomSize = true; 
	    dialog.customsizeRadio.checked = true;
	  }
	  else
	  {
	    dialog.isCustomSize = false;
	    dialog.originalsizeRadio.checked = true;
	  }

    dump("custom size is: "+dialog.isCustomSize+"\n");
    
	  // set spacing editfields
	  dialog.imagelrInput.value = globalElement.getAttribute("hspace");
	  dialog.imagetbInput.value = globalElement.getAttribute("vspace");
	  dialog.border.value       = globalElement.getAttribute("border");    

	  // Get alignment setting  
	  var align = globalElement.getAttribute("align");
	  if (align) {
	    align = align.toLowerCase();
	  }

	  switch ( align )
	  {
	    case "top":
	      dialog.alignTypeSelect.selectedIndex = 0;
	      break;
	    case "center":
	      dialog.alignTypeSelect.selectedIndex = 1;
	      break;
	    case "right":
	      dialog.alignTypeSelect.selectedIndex = 3;
	      break;
	    case "left":
	      dialog.alignTypeSelect.selectedIndex = 4;
	      break;
	    default:  // Default or "bottom"
	      dialog.alignTypeSelect.selectedIndex = 2;
	      break;
	  }
  }

  // we want to force an update so initialize "wasEnableAll" to be the opposite of what the actual state is
  wasEnableAll = !IsValidImage(dialog.srcInput.value);
  doOverallEnabling();
}

function chooseImageFile()
{
  // Get a local file, converted into URL format
  fileName = GetLocalFileURL("img");
  if (fileName && fileName != "") {
    dialog.srcInput.value = fileName;
    doOverallEnabling();
  }
  
  // Put focus into the input field
  dialog.srcInput.focus();
}

function SetGlobalElementToCurrentDialogSettings()
{
  // src
  var str = dialog.srcInput.value.trimString();
  globalElement.setAttribute("src", str);

  // alt
  str = dialog.altTextInput.value.trimString();
  globalElement.setAttribute("alt", str);

  var alignment;
  //Note that the attributes "left" and "right" are opposite
  //  of what we use in the UI, which describes where the TEXT wraps,
  //  not the image location (which is what the HTML describes)
  switch ( dialog.alignTypeSelect.selectedIndex )
  {
    case 0:
      alignment = "top";
      break;
    case 1:
      alignment = "center";
      break;
    case 3:
      alignment = "right";
      break;
    case 4:
      alignment = "left";
      break;
    default:  // Default or "bottom" (2)
      alignment = "";
      break;
  }
dump("alignment ="+alignment+"\n");

  if ( alignment == "" )
    globalElement.removeAttribute( "align" );
  else
    globalElement.setAttribute( "align", alignment );

  if ( dialog.imagelrInput.value.length > 0 )
    globalElement.setAttribute("hspace", dialog.imagelrInput.value);
  else
    globalElement.removeAttribute("hspace");
  
  if ( dialog.imagetbInput.value.length > 0 )
    globalElement.setAttribute("vspace", dialog.imagetbInput.value);
  else
    globalElement.removeAttribute("vspace");

  if ( dialog.border.value.length > 0 )
    globalElement.setAttribute("border", dialog.border.value);
  else
    globalElement.removeAttribute("border");
  
  // width
  str = dialog.widthInput.value;
  if (dialog.widthUnitsSelect.selectedIndex == 1)
    str = str + "%";
  globalElement.setAttribute("width", str);

  // height
  str = dialog.heightInput.value;
  if (dialog.heightUnitsSelect.selectedIndex == 1)
    str = str + "%";
  globalElement.setAttribute("height", str);
}

function onMoreFewerImage()
{
  if (SeeMore)
  {
    dialog.isCustomSize = dialog.customsizeRadio.checked;
    dialog.doConstrain = dialog.constrainCheckbox.checked;
    SetGlobalElementToCurrentDialogSettings();
    
    dialog.MoreSection.setAttribute("style","display: none");
    // Show the "Advanced Edit" button on same line as "More Properties"
    dialog.AdvancedEditButton2.setAttribute("style","display: inherit");
    window.sizeToContent();
    dialog.MoreFewerButton.setAttribute("more","0");
    dialog.MoreFewerButton.setAttribute("value",GetString("MoreProperties"));
    SeeMore = false;
  }
  else
  {
    dialog.MoreSection.setAttribute("style","display: inherit");
    // Hide the "Advanced Edit" next to "More..." Use button at bottom right of dialog
    dialog.AdvancedEditButton2.setAttribute("style","display: none");
    window.sizeToContent();

    dialog.MoreFewerButton.setAttribute("more","1");
    dialog.MoreFewerButton.setAttribute("value",GetString("FewerProperties"));
    SeeMore = true;
    
    InitDialog();
    
    if (dialog.isCustomSize)
    {
      dialog.customsizeRadio.checked = true;
      dialog.originalsizeRadio.checked = false;
    }
    else
    {
      dialog.customsizeRadio.checked = false;
      dialog.originalsizeRadio.checked = true;
    }

    if (dialog.doConstrain)
      dialog.constrainCheckbox.checked = true;
  }
}

function doDimensionEnabling( doEnable )
{
  // Enabled only if "Custom" is checked
  var enable = (doEnable && dialog.customsizeRadio.checked);

  SetElementEnabledByID( "widthInput", enable );
  SetElementEnabledByID( "widthLabel", enable);
  SetElementEnabledByID( "widthUnitsSelect", enable );

  SetElementEnabledByID( "heightInput", enable );
  SetElementEnabledByID( "heightLabel", enable );
  SetElementEnabledByID( "heightUnitsSelect", enable );

  var constrainEnable = enable 
         && ( dialog.widthUnitsSelect.selectedIndex == 0 )
         && ( dialog.heightUnitsSelect.selectedIndex == 0 );
  SetElementEnabledByID( "constrainCheckbox", constrainEnable );
}

function doOverallEnabling()
{
  var canEnableAll = IsValidImage(dialog.srcInput.value);
  if ( wasEnableAll == canEnableAll )
    return;
  
  wasEnableAll = canEnableAll;

  SetElementEnabledByID("ok", canEnableAll );
  SetElementEnabledByID( "altTextLabel", canEnableAll );

  // Do widgets for sizing
  SetElementEnabledByID( "dimensionsLabel", canEnableAll );
  doDimensionEnabling( canEnableAll );
  
  SetElementEnabledByID("alignLabel", canEnableAll );
  SetElementEnabledByID("alignTypeSelect", canEnableAll );

  // spacing Box
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

  // This shouldn't find button, but it does!
  SetElementEnabledByID( "AdvancedEditButton2", canEnableAll );
  SetElementEnabledByID( "AdvancedEditButton3", canEnableAll );

  SetElementEnabledByID( "editImageMap", canEnableAll );
  // TODO: ADD APPROPRIATE DISABLING BASED ON EXISTENCE OF IMAGE MAP
  SetElementEnabledByID( "removeImageMap", canEnableAll && canRemoveImageMap);
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
  constrainChecked = constrainChecked && !dialog.constrainCheckbox.disabled;
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

function editImageMap()
{
  if (editorShell){
    var tagName = "img";
    image = editorShell.GetSelectedElement(tagName);

    //Test selected element to see if it's an image
    if (image){
      //If it is, launch image map dialog
      window.openDialog("chrome://editor/content/EdImageMap.xul", "_blank", "chrome,close", "");
    }
  }
}

function removeImageMap()
{
  dump("removeImageMap -- WRITE ME!\n");
}

// Get data from widgets, validate, and set for the global element
//   accessible to AdvancedEdit() [in EdDialogCommon.js]
function ValidateData()
{
  if ( !IsValidImage(dialog.srcInput.value )) {
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
  
  //TODO: WE SHOULD ALWAYS SET WIDTH AND HEIGHT FOR FASTER IMAGE LAYOUT
  //  IF USER DOESN'T SET IT, WE NEED TO GET VALUE FROM ORIGINAL IMAGE 
  
  var width = "";
  var height = "";
  var isPercentWidth, isPercentHeight;
  var maxLimitWidth, maxLimitHeight;

  if ( SeeMore )
  {
    dialog.isCustomSize = dialog.customsizeRadio.checked;
		isPercentWidth = (dialog.widthUnitsSelect.selectedIndex == 1);
		isPercentHeight = (dialog.heightUnitsSelect.selectedIndex == 1);
		width = dialog.widthInput.value;
		height = dialog.heightInput.value;
  }
  else /* can't SeeMore */
  {
    var tailindex;
    
    width = globalElement.getAttribute( "width" );
    
    tailindex = width.lastIndexOf("%"); 
    isPercentWidth = ( tailindex > 0 );
    if ( isPercentWidth )
      width = width.substring(0, tailindex);
    
    height = globalElement.getAttribute( "height" );
    tailindex = height.lastIndexOf("%"); 
    isPercentHeight = ( tailindex > 0 );
    if ( isPercentHeight )
      height = height.substring(0, tailindex);
  }

  if ( dialog.isCustomSize )
  {
	  maxLimitWidth = isPercentWidth ? 100 : maxPixels;  // Defined in EdDialogCommon.js
    width = ValidateNumberString(width, 1, maxLimitWidth);
    if (width == "") {
      if ( !SeeMore )
        onMoreFewerImage();
      dialog.widthInput.focus();
      return false;
    }
    if (isPercentWidth)
      width = width + "%";

	  maxLimitHeight = isPercentHeight ? 100 : maxPixels;  // Defined in EdDialogCommon.js
    height = ValidateNumberString(height, 1, maxLimitHeight);
    if (height == "") {
      if ( !SeeMore )
        onMoreFewerImage();
      dialog.heightInput.focus();
      return false;
    }
    if (isPercentHeight)
      height = height + "%";
  }

  if ( !dialog.isCustomSize )
  {
  // for now (until we can get the actual image dimensions), clear the height/width attributes if not set
    globalElement.removeAttribute( "width" );
    globalElement.removeAttribute( "height" );
  }
  else
  {
	  if (width.length > 0)
	    globalElement.setAttribute("width", width);
	  if (height.length > 0)
	    globalElement.setAttribute("height", height);
  }

  // spacing attributes
  // All of these should use ValidateNumberString() to 
  //  ensure value is within acceptable range
  var amount;
  if ( SeeMore )
  {
	  if ( dialog.imagelrInput.value.length > 0 )
	  {
	    amount = ValidateNumberString(dialog.imagelrInput.value, 0, maxPixels);
	    if (amount == "")
	      return false;
      globalElement.setAttribute( "hspace", amount );
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
    // Note that the attributes "left" and "right" are opposite
    //  of what we use in the UI, which describes where the TEXT wraps,
    //  not the image location (which is what the HTML describes)
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
	      align = "right";
	      break;
	    case 4:
	      align = "left";
	      break;
	  }
	  if (align == "")
	    globalElement.removeAttribute( "align" );
	  else
	    globalElement.setAttribute( "align", align );
  }

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

