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
var oldSourceInt  = 0;
var imageElement;
var imageMap = 0;
var canRemoveImageMap = false;
var imageMapDisabled = false;

// dialog initialization code

function Startup()
{
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
  dialog.customsizeRadio   = document.getElementById( "customsizeRadio" );
  dialog.originalsizeRadio = document.getElementById( "originalsizeRadio" );
  dialog.constrainCheckbox = document.getElementById( "constrainCheckbox" );
  dialog.widthInput        = document.getElementById( "widthInput" );
  dialog.heightInput       = document.getElementById( "heightInput" );
  dialog.widthUnitsMenulist   = document.getElementById( "widthUnitsMenulist" );
  dialog.heightUnitsMenulist  = document.getElementById( "heightUnitsMenulist" );
  dialog.imagelrInput      = document.getElementById( "imageleftrightInput" );
  dialog.imagetbInput      = document.getElementById( "imagetopbottomInput" );
  dialog.border            = document.getElementById( "border" );
  dialog.alignTypeSelect   = document.getElementById( "alignTypeSelect" );
  dialog.editImageMap      = document.getElementById( "editImageMap" );
  dialog.removeImageMap    = document.getElementById( "removeImageMap" );
  dialog.doConstrain = false;
  dialog.isCustomSize = false;
  

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

  // Set SeeMore bool to the OPPOSITE of the current state,
  //   which is automatically saved by using the 'persist="more"' 
  //   attribute on the MoreFewerButton button
  //   onMoreFewerImage will toggle the state and redraw the dialog
  SeeMore = (dialog.MoreFewerButton.getAttribute("more") != "1");

  // Initialize widgets with image attributes in the case where the entire dialog isn't visible
  if ( SeeMore ) // this is actually in the opposite state until onMoreFewerImage is called below
    InitDialog();
  
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

  str = globalElement.getAttribute("src");
  if (str)
    dialog.srcInput.value = str;
  
  str = globalElement.getAttribute("alt");
  if (str)
    dialog.altTextInput.value = str;
  
  // Check for image map
  
  if ( SeeMore )
  {
	  // setup the height and width widgets
	  dialog.widthInput.value = InitPixelOrPercentMenulist(globalElement, imageElement, "width", "widthUnitsMenulist", gPixel);
	  dialog.heightInput.value = InitPixelOrPercentMenulist(globalElement, imageElement, "height", "heightUnitsMenulist", gPixel);
	  
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

function chooseFile()
{
  // Get a local file, converted into URL format
  fileName = GetLocalFileURL("img");
  if (fileName && fileName != "") {
    dialog.srcInput.value = fileName;
    doOverallEnabling();
  }
  
  // Put focus into the input field
  SetTextfieldFocus(dialog.srcInput);
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
  if (dialog.widthUnitsMenulist.selectedIndex == 1)
    str = str + "%";
  globalElement.setAttribute("width", str);

  // height
  str = dialog.heightInput.value;
  if (dialog.heightUnitsMenulist.selectedIndex == 1)
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
    
    dialog.MoreSection.setAttribute("collapsed","true");
    dialog.MoreFewerButton.setAttribute("more","0");
    dialog.MoreFewerButton.setAttribute("value",GetString("MoreProperties"));
    SeeMore = false;

    // Show the "Advanced Edit" button on same line as "More Properties"
    dialog.AdvancedEditButton.removeAttribute("collapsed");
    dialog.AdvancedEditButton2.setAttribute("collapsed","true");
    window.sizeToContent();
  }
  else
  {
    dialog.MoreSection.removeAttribute("collapsed");
    dialog.MoreFewerButton.setAttribute("more","1");
    dialog.MoreFewerButton.setAttribute("value",GetString("FewerProperties"));
    SeeMore = true;

    // Show the "Advanced Edit" button at bottom
    dialog.AdvancedEditButton.setAttribute("collapsed","true");
    dialog.AdvancedEditButton2.removeAttribute("collapsed");
    window.sizeToContent();
    
    InitDialog();
    
    //TODO: We won't need to do this when we convert to using "collapsed"
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
  SetElementEnabledById( "originalsizeRadio", doEnable );
  SetElementEnabledById( "customsizeRadio", doEnable);

  // Enabled only if "Custom" is checked
  var enable = (doEnable && dialog.customsizeRadio.checked);
  
  if ( !dialog.customsizeRadio.checked && !dialog.originalsizeRadio.checked)
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
}

function doOverallEnabling()
{
  var canEnableAll = IsValidImage(dialog.srcInput.value);
  if ( wasEnableAll == canEnableAll )
    return;
  
  wasEnableAll = canEnableAll;

  SetElementEnabledById("ok", canEnableAll );
  SetElementEnabledById( "altTextLabel", canEnableAll );

  // Do widgets for sizing
  SetElementEnabledById( "dimensionsLabel", canEnableAll );
  doDimensionEnabling( canEnableAll );
  
  SetElementEnabledById("alignLabel", canEnableAll );
  SetElementEnabledById("alignTypeSelect", canEnableAll );

  // spacing Box
  SetElementEnabledById( "spacingLabel", canEnableAll );
  SetElementEnabledById( "imageleftrightInput", canEnableAll );
  SetElementEnabledById( "leftrightLabel", canEnableAll );
  SetElementEnabledById( "leftrighttypeLabel", canEnableAll );

  SetElementEnabledById( "imagetopbottomInput", canEnableAll );
  SetElementEnabledById( "topbottomLabel", canEnableAll );
  SetElementEnabledById( "topbottomtypeLabel", canEnableAll );

  SetElementEnabledById( "border", canEnableAll );
  SetElementEnabledById( "borderLabel", canEnableAll );
  SetElementEnabledById( "bordertypeLabel", canEnableAll );

  // This shouldn't find button, but it does!
  SetElementEnabledById( "AdvancedEditButton", canEnableAll );
  SetElementEnabledById( "AdvancedEditButton2", canEnableAll );

  SetElementEnabledById( "imagemapLabel", canEnableAll );
  SetElementEnabledById( "editImageMap", canEnableAll );
  SetElementEnabledById( "removeImageMap", canEnableAll && canRemoveImageMap);
}

// constrainProportions contribution by pete@postpagan.com
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
  var alt = dialog.altTextInput.value; //.trimString();
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
		isPercentWidth = (dialog.widthUnitsMenulist.selectedIndex == 1);
		isPercentHeight = (dialog.heightUnitsMenulist.selectedIndex == 1);
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

  if ( SeeMore && dialog.originalsizeRadio.checked )
  {
    // original size: don't do anything right now
  }
  else if ( (SeeMore && dialog.customsizeRadio.checked) 
             || dialog.isCustomSize )
  {
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
    dump("SeeMore spacing attribs\n");
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

    return true;
  }
  return false;
}

