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
var insertNewIMap = true;
var wasEnableAll  = false;
var constrainOn = false;
// Note used in current version, but these are set correctly
//  and could be used to reset width and height used for constrain ratio
var constrainWidth  = 0;
var constrainHeight = 0;
var imageElement;
var imageMap = 0;
var canRemoveImageMap = false;
var imageMapDisabled = false;
var dialog;
var globalMap;
var doAltTextError = true;
var actualWidth = "";
var actualHeight = "";
var previewImage;
var timeoutId = -1;
// msec between attempts to load image
var interval = 200;
var intervalSum = 0;
// After this many msec, give up trying to load image
var intervalLimit = 60000;

// These must correspond to values in EditorDialog.css for each theme
// (unfortunately, setting "style" attribute here doesn't work!)
var previewImageWidth = 80;
var previewImageHeight = 50;
var StartupCalled = false;

// dialog initialization code

function Startup()
{
  //XXX Very weird! When calling this with an existing image,
  //    we get called twice. That causes dialog layout 
  //    to explode to fullscreen!
  if (StartupCalled)
  {
    dump("*** CALLING IMAGE DIALOG Startup() AGAIN! ***\n");
    return;
  }
  StartupCalled = true;

  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, onCancel);

  // Create dialog object to store controls for easy access
  dialog = new Object;

  dialog.srcInput          = document.getElementById( "srcInput" );
  dialog.altTextInput      = document.getElementById( "altTextInput" );
  dialog.MoreFewerButton   = document.getElementById( "MoreFewerButton" );
  dialog.AdvancedEditButton = document.getElementById( "AdvancedEditButton" );
  dialog.AdvancedEditButton2 = document.getElementById( "AdvancedEditButton2" );
  dialog.MoreSection       = document.getElementById( "MoreSection" );
  dialog.customSizeRadio   = document.getElementById( "customSizeRadio" );
  dialog.actualSizeRadio = document.getElementById( "actualSizeRadio" );
  dialog.constrainCheckbox = document.getElementById( "constrainCheckbox" );
  dialog.widthInput        = document.getElementById( "widthInput" );
  dialog.heightInput       = document.getElementById( "heightInput" );
  dialog.widthUnitsMenulist   = document.getElementById( "widthUnitsMenulist" );
  dialog.heightUnitsMenulist  = document.getElementById( "heightUnitsMenulist" );
  dialog.imagelrInput      = document.getElementById( "imageleftrightInput" );
  dialog.imagetbInput      = document.getElementById( "imagetopbottomInput" );
  dialog.border            = document.getElementById( "border" );
  dialog.alignTypeSelect   = document.getElementById( "alignTypeSelect" );
  dialog.alignImage        = document.getElementById( "alignImage" );
  dialog.alignText         = document.getElementById( "alignText" );
  dialog.editImageMap      = document.getElementById( "editImageMap" );
  dialog.removeImageMap    = document.getElementById( "removeImageMap" );
  dialog.ImageHolder       = document.getElementById( "preview-image-holder" );
  dialog.PreviewBox        = document.getElementById( "preview-image-box" );
  dialog.PreviewWidth      = document.getElementById( "PreviewWidth" );
  dialog.PreviewHeight     = document.getElementById( "PreviewHeight" );
  dialog.PreviewSize       = document.getElementById( "PreviewSize" );
  
  // Get a single selected image element
  var tagName = "img"
  imageElement = editorShell.GetSelectedElement(tagName);

  if (imageElement) 
  {
    // We found an element and don't need to insert one
    insertNew = false;
    actualWidth  = imageElement.naturalWidth;
    actualHeight = imageElement.naturalHeight;
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
  

  // Get image map for image
  var usemap = globalElement.getAttribute("usemap");
  if (usemap)
  {
    mapname = usemap.substring(1, usemap.length);
    mapCollection = editorShell.editorDocument.getElementsByName(mapname);
    if (mapCollection[0] != null)
    {
      imageMap = mapCollection[0];
      globalMap = imageMap;
      canRemoveImageMap = true;
      insertNewIMap = false;
      dump(imageMap.childNodes.length+"\n");
    }
    else
    {
      insertNewIMap = true;
      globalMap = null;
    }
  }
  else
  {
    insertNewIMap = true;
    globalMap = null;
  }
  InitDialog();

  // By default turn constrain on, but both width and height must be in pixels
  dialog.constrainCheckbox.checked =
    dialog.widthUnitsMenulist.selectedIndex == 0 &&
    dialog.heightUnitsMenulist.selectedIndex == 0;    

  // Set SeeMore bool to the OPPOSITE of the current state,
  //   which is automatically saved by using the 'persist="more"' 
  //   attribute on the MoreFewerButton button
  //   onMoreFewerImage will toggle the state and redraw the dialog
  SeeMore = (dialog.MoreFewerButton.getAttribute("more") != "1");

  // Initialize widgets with image attributes in the case where the entire dialog isn't visible
  onMoreFewerImage();  // this call will initialize all widgets if entire dialog is visible

  SetTextfieldFocus(dialog.srcInput);

  SetWindowLocation();
}

// Set dialog widgets with attribute data
// We get them from globalElement copy so this can be used
//   by AdvancedEdit(), which is shared by all property dialogs
function InitDialog()
{
  // Set the controls to the image's attributes

  var str = globalElement.getAttribute("src");
  if (str)
  {
    dialog.srcInput.value = str;
    GetImageFromURL();
  }
  
  str = globalElement.getAttribute("alt");
  if (str)
    dialog.altTextInput.value = str;
  
  // setup the height and width widgets
  var width = InitPixelOrPercentMenulist(globalElement, 
                    insertNew ? null : imageElement, 
                    "width", "widthUnitsMenulist", gPixel);
  var height = InitPixelOrPercentMenulist(globalElement, 
                    insertNew ? null : imageElement, 
                    "height", "heightUnitsMenulist", gPixel);
  
  // Set actual radio button if both set values are the same as actual
  if (actualWidth && actualHeight)
    dialog.actualSizeRadio.checked = (width == actualWidth) && (height == actualHeight);
  else if ( !(width || height) )
    dialog.actualSizeRadio.checked = true;

  if (!dialog.actualSizeRadio.checked)
    dialog.customSizeRadio.checked = true;
  
  dialog.widthInput.value  = constrainWidth = width ? width : (actualWidth ? actualWidth : "");
  dialog.heightInput.value = constrainHeight = height ? height : (actualHeight ? actualHeight : "");

  // set spacing editfields
  dialog.imagelrInput.value = globalElement.getAttribute("hspace");
  dialog.imagetbInput.value = globalElement.getAttribute("vspace");
  dialog.border.value       = globalElement.getAttribute("border");    

  // Get alignment setting  
  var align = globalElement.getAttribute("align");
  if (align) {
	  align = align.toLowerCase();
  }
  var imgClass;
  var textID;
  switch ( align )
  {
	  case "top":
	    dialog.alignTypeSelect.selectedIndex = 0;
      imgClass = "img-align-top";
      textID   = "topText";
	    break;
	  case "center":
	    dialog.alignTypeSelect.selectedIndex = 1;
      imgClass = "img-align-middle";
      textID   = "middleText";
	    break;
	  case "right":
      // Note: this means the image is on the right
	    dialog.alignTypeSelect.selectedIndex = 3;
      imgClass = "img-align-right";
      textID   = "rightText";
	    break;
	  case "left":
      // Note: this means the image is on the left
	    dialog.alignTypeSelect.selectedIndex = 4;
      imgClass = "img-align-left";
      textID   = "leftText";
	    break;
	  default:  // Default or "bottom"
	    dialog.alignTypeSelect.selectedIndex = 2;
      imgClass = "img-align-bottom";
      textID   = "bottomText";
	    break;
  }
  // Set the same image and text as used in the selected menuitem in the menulist    
  // Image url is CSS-driven based on class
  dialog.alignImage.setAttribute("class", imgClass);
  dialog.alignText.setAttribute("value", document.getElementById(textID).getAttribute("value"));

  // we want to force an update so initialize "wasEnableAll" to be the opposite of what the actual state is
  wasEnableAll = !IsValidImage(dialog.srcInput.value);
  doOverallEnabling();
  doDimensionEnabling();
}

function chooseFile()
{
  // Get a local file, converted into URL format
  var fileName = GetLocalFileURL("img");
  if (fileName) {
    dialog.srcInput.value = fileName;
    doOverallEnabling();
  }
  GetImageFromURL();
  // Put focus into the input field
  SetTextfieldFocus(dialog.srcInput);
}

function GetImageFromURL()
{
  dialog.PreviewSize.setAttribute("collapsed", "true");

  // Remove existing preview image
  // (other attempts to just change "src" fail;
  //  once we fail to load, further setting of src fail)
  if (dialog.ImageHolder.firstChild)
  {
    //previewImage.setAttribute("width", 0);
    //previewImage.setAttribute("height", 0);
    dialog.ImageHolder.removeChild(dialog.ImageHolder.firstChild);
  }
  
  var imageSrc = dialog.srcInput.value;
  if (imageSrc) imageSrc = imageSrc.trimString();
  if (!imageSrc) return;
  if (IsValidImage(imageSrc))
  {
    if (!dialog.ImageHolder.firstChild)
    {
      // Append an image to the dialog to trigger image loading
      //   and also serves as a preview
      previewImage = editorShell.CreateElementWithDefaults("img");
      if (!previewImage) return;
      dialog.ImageHolder.appendChild(previewImage);
    }
    previewImage.src = imageSrc;

    // Get the origin width from the image or setup timer to get later
    if (previewImage.complete)
      GetActualSize();
    else
    {
      // Start timer to poll until image is loaded
      //dump("*** Starting timer to get natural image size...\n");
      timeoutId = window.setInterval("GetActualSize()", interval);
      intervalSum = 0;
    }
  }
}

function GetActualSize()
{
  if (intervalSum > intervalLimit)
  {
    dump(" Timeout trying to load preview image\n");
    CancelTimer();
    return;
  }

  if (!previewImage)
  {
    CancelTimer();
  }
  else
  {
    if (previewImage.complete)
    {
      // Image loading has completed -- we can get actual width
      CancelTimer();
      actualWidth  = previewImage.naturalWidth;
      actualHeight = previewImage.naturalHeight;
      if (actualWidth && actualHeight)
      {
        // Use actual size or scale to fit preview if either dimension is too large
        var width = actualWidth;
        var height = actualHeight;
        if (actualWidth > previewImageWidth)
        {
            width = previewImageWidth;
            height = actualHeight * (previewImageWidth / actualWidth);
        }
        if (height > previewImageHeight)
        {
          height = previewImageHeight;
          width = actualWidth * (previewImageHeight / actualHeight);
        }
        if (actualWidth > previewImageWidth || actualHeight > previewImageHeight)
        {
          // Resize image to fit preview frame
          previewImage.setAttribute("width", width);
          previewImage.setAttribute("height", height);
        }

        dialog.PreviewWidth.setAttribute("value", actualWidth);
        dialog.PreviewHeight.setAttribute("value", actualHeight);

        dialog.PreviewSize.setAttribute("collapsed", "false");
        dialog.ImageHolder.setAttribute("collapsed", "false");

        // Use values as start for constrain proportions
      }

      if (dialog.actualSizeRadio.checked)
        SetActualSize();
    }
    else 
    {
      //dump("*** Waiting for image loading...\n");
      // Accumulate time so we don't do try this forever
      intervalSum += interval;
    }
  }
}

function SetActualSize()
{
  dialog.widthInput.value = actualWidth ? actualWidth : "";
  dialog.heightInput.value = actualHeight ? actualHeight : "";
  doDimensionEnabling();
}

function CancelTimer()
{
  if (timeoutId != -1)
  {
    //dump("*** CancelTimer\n");
    window.clearInterval(timeoutId);
    timeoutId = -1;
  }
  intervalSum = 0;
}

function ChangeImageSrc()
{
  GetImageFromURL();
  doOverallEnabling();
}

function onMoreFewerImage()
{
  if (SeeMore)
  {
    dialog.MoreSection.setAttribute("collapsed","true");
    dialog.MoreFewerButton.setAttribute("value", GetString("MoreProperties"));
    dialog.MoreFewerButton.setAttribute("more","0");
    SeeMore = false;
    // Show the "Advanced Edit" button on same line as "More Properties"
    dialog.AdvancedEditButton.setAttribute("collapsed","false");
    dialog.AdvancedEditButton2.setAttribute("collapsed","true");
    // Weird caret appearing when we collapse, so force focus to URL textfield
    dialog.srcInput.focus();
  }
  else
  {
    dialog.MoreSection.setAttribute("collapsed","false");
    dialog.MoreFewerButton.setAttribute("value", "");
    dialog.MoreFewerButton.setAttribute("value", GetString("FewerProperties"));
    dialog.MoreFewerButton.setAttribute("more","1");
    SeeMore = true;

    // Show the "Advanced Edit" button at bottom
    dialog.AdvancedEditButton.setAttribute("collapsed","true");
    dialog.AdvancedEditButton2.setAttribute("collapsed","false");
  }
  window.sizeToContent();
}

function doDimensionEnabling()
{
  // Enabled only if "Custom" is checked
  var enable = (dialog.customSizeRadio.checked);
  
  if ( !dialog.customSizeRadio.checked && !dialog.actualSizeRadio.checked)
    dump("BUG!  neither radio button is checked!!!! \n");

  SetElementEnabledById( "widthInput", enable );
  SetElementEnabledById( "widthLabel", enable);
  SetElementEnabledById( "widthUnitsMenulist", enable );

  SetElementEnabledById( "heightInput", enable );
  SetElementEnabledById( "heightLabel", enable );
  SetElementEnabledById( "heightUnitsMenulist", enable );

  var constrainEnable = enable 
         && ( dialog.widthUnitsMenulist.selectedIndex == 0 )
         && ( dialog.heightUnitsMenulist.selectedIndex == 0 );

  SetElementEnabledById( "constrainCheckbox", constrainEnable );

  // Counteracting another weird caret 
  //  (appears in Height input, but not really focused!)
  if (enable)
    dialog.widthInput.focus();
}

function doOverallEnabling()
{
  var canEnableOk = IsValidImage(dialog.srcInput.value);
  if ( wasEnableAll == canEnableOk )
    return;
  
  wasEnableAll = canEnableOk;

  SetElementEnabledById("ok", canEnableOk );

  SetElementEnabledById( "imagemapLabel",  canEnableOk );
  SetElementEnabledById( "editImageMap",   canEnableOk );
  SetElementEnabledById( "removeImageMap", canRemoveImageMap);
}

function ToggleConstrain()
{
  // If just turned on, save the current width and height as basis for constrian ratio
  // Thus clicking on/off lets user say "Use these values as aspect ration"
  if (dialog.constrainCheckbox.checked && !dialog.constrainCheckbox.disabled)
  {
    constrainWidth = Number(dialog.widthInput.value.trimString());
    constrainHeight = Number(dialog.heightInput.value.trimString());
  }
  document.getElementById('widthInput').focus()

}

function constrainProportions( srcID, destID )
{
  // Return if we don't have proper data or checkbox isn't checked and enabled
//  if (!constrainWidth || !constrainHeight ||
  if (!actualWidth || !actualHeight ||
      !(dialog.constrainCheckbox.checked && !dialog.constrainCheckbox.disabled))
    return;
  
  var srcElement = document.getElementById ( srcID );
  if ( !srcElement )
    return;
  
  var destElement = document.getElementById( destID );
  if ( !destElement )
    return;

  forceInteger( srcID );

  // This always uses the actual width and height ratios
  if (srcID == "widthInput")
    destElement.value = Math.round( srcElement.value * actualHeight / actualWidth );
  else
    destElement.value = Math.round( srcElement.value * actualWidth / actualHeight );

/*  
  // With this strategy, the width and height ratio 
  //   can be reset to whatever the user entered.
  if (srcID == "widthInput")
    destElement.value = Math.round( srcElement.value * constrainHeight / constrainWidth );
  else
    destElement.value = Math.round( srcElement.value * constrainWidth / constrainHeight );
*/
}

function editImageMap()
{
  // Make a copy to use for image map editor
  if (insertNewIMap)
  {
    imageMap = editorShell.CreateElementWithDefaults("map");
    globalMap = imageMap.cloneNode(true);
  }
  else
    globalMap = imageMap;  
  
  window.openDialog("chrome://editor/content/EdImageMap.xul", "_blank", "chrome,close,titlebar,modal", globalElement, globalMap);
}

function removeImageMap()
{
  globalElement.removeAttribute("usemap");
  if (imageMap){
    canRemoveImageMap = false;
    SetElementEnabledByID( "removeImageMap", false);
    editorShell.DeleteElement(imageMap);
    insertNewIMap = true;
    globalMap = null;
  }
}

function changeAlignment(menuItem)
{
  // Item index assume structure in XUL
  // (current item has spring, image, spring, text)
  var image = menuItem.childNodes.item(1);
  var text =  menuItem.childNodes.item(3);
  dialog.alignImage.setAttribute("class", image.getAttribute("class"));
  dialog.alignText.setAttribute("value", text.getAttribute("value"));
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
  
  // Note: allow typing spaces, 
  // Warn user if empty string just once per dialog session
  //   but don't consider this a failure
  var alt = dialog.altTextInput.value;
  if (doAltTextError && !alt)
  {
    ShowInputErrorMessage(GetString("NoAltText"));
    SetTextfieldFocus(dialog.altTextInput);
    doAltTextError = false;
    return false;
  }
  globalElement.setAttribute("alt", alt);
  
  var width = "";
  var height = "";
  var isPercentWidth, isPercentHeight;
  var maxLimitWidth, maxLimitHeight;

  if (dialog.actualSizeRadio.checked)
  {
    width = actualWidth ? actualWidth : "";
    height = actualHeight ? actualHeight : "";
  }
  else
  {
	  isPercentWidth = (dialog.widthUnitsMenulist.selectedIndex == 1);
	  isPercentHeight = (dialog.heightUnitsMenulist.selectedIndex == 1);

	  maxLimitWidth = isPercentWidth ? 100 : maxPixels;  // Defined in EdDialogCommon.js
    width = ValidateNumberString(dialog.widthInput.value, 1, maxLimitWidth);
    if (width == "")
    {
      if ( !SeeMore )
        onMoreFewerImage();
      SetTextfieldFocus(dialog.widthInput);
      return false;
    }
    if (isPercentWidth)
      width = width + "%";

	  maxLimitHeight = isPercentHeight ? 100 : maxPixels;  // Defined in EdDialogCommon.js
    height = ValidateNumberString(dialog.heightInput.value, 1, maxLimitHeight);
    if (height == "")
    {
      if ( !SeeMore )
        onMoreFewerImage();
      SetTextfieldFocus(dialog.heightInput);
      return false;
    }
    if (isPercentHeight)
      height = height + "%";
  }
  // We always set the width and height attributes, even if same as actual.
  //  This speeds up layout of pages since sizes are known before image is downloaded
  // (But don't set if we couldn't obtain actual dimensions)
  if (width)
    globalElement.setAttribute("width", width);
  else
    globalElement.removeAttribute("width");

  if (height)
    globalElement.setAttribute("height", height);
  else
    globalElement.removeAttribute("height");

  // spacing attributes
  var amount;
	if ( dialog.imagelrInput.value )
	{
	  amount = ValidateNumberString(dialog.imagelrInput.value, 0, maxPixels);
	  if (amount == "")
	    return false;
    globalElement.setAttribute( "hspace", amount );
  }
  else
    globalElement.removeAttribute( "hspace" );

	if ( dialog.imagetbInput.value )
	{
	  var vspace = ValidateNumberString(dialog.imagetbInput.value, 0, maxPixels);
	  if (vspace == "")
	    return false;
	  globalElement.setAttribute( "vspace", vspace );
	}
	else
	  globalElement.removeAttribute( "vspace" );

  // note this is deprecated and should be converted to stylesheets
	if ( dialog.border.value )
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

  return true;
}

function onOK()
{
  // handle insertion of new image
  if (ValidateData())
  {
	  // Assign to map if there is one
	  if ( globalMap )
	  {
      mapName = globalMap.getAttribute("name");
      dump("mapName = "+mapName+"\n");
      if (mapName != "")
      {
        globalElement.setAttribute("usemap", ("#"+mapName));
        if (globalElement.getAttribute("border") == "")
          globalElement.setAttribute("border", 0);
      }

      // Copy or insert image map
      imageMap = globalMap.cloneNode(true);
      if (insertNewIMap)
      {
        try
        {
          editorShell.editorDocument.body.appendChild(imageMap);
        //editorShell.InsertElementAtSelection(imageMap, false);
        }
        catch (e)
        {
          dump("Exception occured in InsertElementAtSelection\n");
        }
      }
    }

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

    // un-comment to see that inserting image maps does not work!
    /*test = editorShell.CreateElementWithDefaults("map");
    test.setAttribute("name", "testing");
    testArea = editorShell.CreateElementWithDefaults("area");
    testArea.setAttribute("shape", "circle");
    testArea.setAttribute("coords", "86,102,52");
    testArea.setAttribute("href", "test");
    test.appendChild(testArea);
    editorShell.InsertElementAtSelection(test, false);*/

    SaveWindowLocation();
    CancelTimer();
    return true;
  }
  return false;
}

function onImageCancel()
{
  CancelTimer();
  onCancel();
}