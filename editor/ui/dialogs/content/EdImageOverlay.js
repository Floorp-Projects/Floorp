/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Editor Image Properties Overlay.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pete Collins
 *   Brian King
 *   Ben Goodger
 *   Neil Rashbrook <neil@parkwaycc.co.uk> (Separated from EdImageProps.js)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 Note: We encourage non-empty alt text for images inserted into a page. 
 When there's no alt text, we always write 'alt=""' as the attribute, since "alt" is a required attribute.
 We allow users to not have alt text by checking a "Don't use alterate text" radio button,
 and we don't accept spaces as valid alt text. A space used to be required to avoid the error message
 if user didn't enter alt text, but is uneccessary now that we no longer annoy the user 
 with the error dialog if alt="" is present on an img element.
 We trim all spaces at the beginning and end of user's alt text
*/

var gInsertNewImage = true;
var gInsertNewIMap = true;
var gDoAltTextError = false;
var gConstrainOn = false;
// Note used in current version, but these are set correctly
//  and could be used to reset width and height used for constrain ratio
var gConstrainWidth  = 0;
var gConstrainHeight = 0;
var imageElement;
var gImageMap = 0;
var gCanRemoveImageMap = false;
var gRemoveImageMap = false;
var gImageMapDisabled = false;
var gActualWidth = "";
var gActualHeight = "";
var gOriginalSrc = "";
var gHaveDocumentUrl = false;
var gTimerID;
var gValidateTab;

// These must correspond to values in EditorDialog.css for each theme
// (unfortunately, setting "style" attribute here doesn't work!)
var gPreviewImageWidth = 80;
var gPreviewImageHeight = 50;

// dialog initialization code

function ImageStartup()
{
  gDialog.tabBox            = document.getElementById( "TabBox" );
  gDialog.tabLocation       = document.getElementById( "imageLocationTab" );
  gDialog.tabDimensions     = document.getElementById( "imageDimensionsTab" );
  gDialog.tabBorder         = document.getElementById( "imageBorderTab" );
  gDialog.srcInput          = document.getElementById( "srcInput" );
  gDialog.titleInput        = document.getElementById( "titleInput" );
  gDialog.altTextInput      = document.getElementById( "altTextInput" );
  gDialog.altTextRadioGroup = document.getElementById( "altTextRadioGroup" );
  gDialog.altTextRadio      = document.getElementById( "altTextRadio" );
  gDialog.noAltTextRadio    = document.getElementById( "noAltTextRadio" );
  gDialog.customSizeRadio   = document.getElementById( "customSizeRadio" );
  gDialog.actualSizeRadio   = document.getElementById( "actualSizeRadio" );
  gDialog.constrainCheckbox = document.getElementById( "constrainCheckbox" );
  gDialog.widthInput        = document.getElementById( "widthInput" );
  gDialog.heightInput       = document.getElementById( "heightInput" );
  gDialog.widthUnitsMenulist   = document.getElementById( "widthUnitsMenulist" );
  gDialog.heightUnitsMenulist  = document.getElementById( "heightUnitsMenulist" );
  gDialog.imagelrInput      = document.getElementById( "imageleftrightInput" );
  gDialog.imagetbInput      = document.getElementById( "imagetopbottomInput" );
  gDialog.border            = document.getElementById( "border" );
  gDialog.alignTypeSelect   = document.getElementById( "alignTypeSelect" );
  gDialog.ImageHolder       = document.getElementById( "preview-image-holder" );
  gDialog.PreviewWidth      = document.getElementById( "PreviewWidth" );
  gDialog.PreviewHeight     = document.getElementById( "PreviewHeight" );
  gDialog.PreviewSize       = document.getElementById( "PreviewSize" );
  gDialog.PreviewImage      = null;
  gDialog.OkButton          = document.documentElement.getButton("accept");
}

// Set dialog widgets with attribute data
// We get them from globalElement copy so this can be used
//   by AdvancedEdit(), which is shared by all property dialogs
function InitImage()
{
  // Set the controls to the image's attributes
  gDialog.srcInput.value = globalElement.getAttribute("src");

  // Set "Relativize" checkbox according to current URL state
  SetRelativeCheckbox();

  // Force loading of image from its source and show preview image
  LoadPreviewImage();

  if (globalElement.hasAttribute("title"))
    gDialog.titleInput.value = globalElement.getAttribute("title");

  var hasAltText = globalElement.hasAttribute("alt");
  var altText;
  if (hasAltText)
  {
    altText = globalElement.getAttribute("alt");
    gDialog.altTextInput.value = altText;
  }

  // Initialize altText widgets during dialog startup 
  //   or if user enterred altText in Advanced Edit dialog
  //  (this preserves "Don't use alt text" radio button state)
  if (!gDialog.altTextRadioGroup.selectedItem || altText)
  {
    if (gInsertNewImage || !hasAltText || (hasAltText && gDialog.altTextInput.value))
    {
      SetAltTextDisabled(false);
      gDialog.altTextRadioGroup.selectedItem = gDialog.altTextRadio;
    }
    else
    {
      SetAltTextDisabled(true);
      gDialog.altTextRadioGroup.selectedItem = gDialog.noAltTextRadio;
    }
  }

  // setup the height and width widgets
  var width = InitPixelOrPercentMenulist(globalElement,
                    gInsertNewImage ? null : imageElement,
                    "width", "widthUnitsMenulist", gPixel);
  var height = InitPixelOrPercentMenulist(globalElement,
                    gInsertNewImage ? null : imageElement,
                    "height", "heightUnitsMenulist", gPixel);

  // Set actual radio button if both set values are the same as actual
  SetSizeWidgets(width, height);

  gDialog.widthInput.value  = gConstrainWidth = width ? width : (gActualWidth ? gActualWidth : "");
  gDialog.heightInput.value = gConstrainHeight = height ? height : (gActualHeight ? gActualHeight : "");

  // set spacing editfields
  gDialog.imagelrInput.value = globalElement.getAttribute("hspace");
  gDialog.imagetbInput.value = globalElement.getAttribute("vspace");

  // dialog.border.value       = globalElement.getAttribute("border");
  var bv = GetHTMLOrCSSStyleValue(globalElement, "border", "border-top-width");
  if (/px/.test(bv))
  {
    // Strip out the px
    bv = RegExp.leftContext;
  }
  else if (bv == "thin")
  {
    bv = "1";
  }
  else if (bv == "medium")
  {
    bv = "3";
  }
  else if (bv == "thick")
  {
    bv = "5";
  }
  gDialog.border.value = bv;

  // Get alignment setting
  var align = globalElement.getAttribute("align");
  if (align)
    align = align.toLowerCase();

  var imgClass;
  var textID;

  switch ( align )
  {
    case "top":
    case "middle":
    case "right":
    case "left":
      gDialog.alignTypeSelect.value = align;
      break;
    default:  // Default or "bottom"
      gDialog.alignTypeSelect.value = "bottom";
  }

  // Get image map for image
  gImageMap = GetImageMap();

  doOverallEnabling();
  doDimensionEnabling();
}

function  SetSizeWidgets(width, height)
{
  if (!(width || height) || (gActualWidth && gActualHeight && width == gActualWidth && height == gActualHeight))
    gDialog.actualSizeRadio.radioGroup.selectedItem = gDialog.actualSizeRadio;

  if (!gDialog.actualSizeRadio.selected)
  {
    gDialog.actualSizeRadio.radioGroup.selectedItem = gDialog.customSizeRadio;

    // Decide if user's sizes are in the same ratio as actual sizes
    if (gActualWidth && gActualHeight)
    {
      if (gActualWidth > gActualHeight)
        gDialog.constrainCheckbox.checked = (Math.round(gActualHeight * width / gActualWidth) == height);
      else
        gDialog.constrainCheckbox.checked = (Math.round(gActualWidth * height / gActualHeight) == width);
    }
  }
}

// Disable alt text input when "Don't use alt" radio is checked
function SetAltTextDisabled(disable)
{
  gDialog.altTextInput.disabled = disable;
}

function GetImageMap()
{
  var usemap = globalElement.getAttribute("usemap");
  if (usemap)
  {
    gCanRemoveImageMap = true;
    var mapname = usemap.substring(1, usemap.length);
    var mapCollection;
    try {
      mapCollection = GetCurrentEditor().document.getElementsByName(mapname);
    } catch (e) {}
    if (mapCollection && mapCollection[0] != null)
    {
      gInsertNewIMap = false;
      return mapCollection[0];
    }
  }
  else
  {
    gCanRemoveImageMap = false;
  }

  gInsertNewIMap = true;
  return null;
}

function chooseFile()
{
  if (gTimerID)
    clearTimeout(gTimerID);
  // Get a local file, converted into URL format
  var fileName = GetLocalFileURL("img");
  if (fileName)
  {
    // Always try to relativize local file URLs
    if (gHaveDocumentUrl)
      fileName = MakeRelativeUrl(fileName);

    gDialog.srcInput.value = fileName;

    SetRelativeCheckbox();
    doOverallEnabling();
  }
  LoadPreviewImage();

  // Put focus into the input field
  SetTextboxFocus(gDialog.srcInput);
}

function PreviewImageLoaded()
{
  if (gDialog.PreviewImage)
  {
    // Image loading has completed -- we can get actual width
    gActualWidth  = gDialog.PreviewImage.naturalWidth;
    gActualHeight = gDialog.PreviewImage.naturalHeight;

    if (gActualWidth && gActualHeight)
    {
      // Use actual size or scale to fit preview if either dimension is too large
      var width = gActualWidth;
      var height = gActualHeight;
      if (gActualWidth > gPreviewImageWidth)
      {
          width = gPreviewImageWidth;
          height = gActualHeight * (gPreviewImageWidth / gActualWidth);
      }
      if (height > gPreviewImageHeight)
      {
        height = gPreviewImageHeight;
        width = gActualWidth * (gPreviewImageHeight / gActualHeight);
      }
      gDialog.PreviewImage.width = width;
      gDialog.PreviewImage.height = height;

      gDialog.PreviewWidth.setAttribute("value", gActualWidth);
      gDialog.PreviewHeight.setAttribute("value", gActualHeight);

      gDialog.PreviewSize.collapsed = false;
      gDialog.ImageHolder.collapsed = false;

      SetSizeWidgets(gDialog.widthInput.value, gDialog.heightInput.value);
    }

    if (gDialog.actualSizeRadio.selected)
      SetActualSize();
  }
}

function LoadPreviewImage()
{
  gDialog.PreviewSize.collapsed = true;
  // XXXbz workaround for bug 265416 / bug 266284
  gDialog.ImageHolder.collapsed = true;

  var imageSrc = TrimString(gDialog.srcInput.value);
  if (!imageSrc)
    return;

  try {
    // Remove the image URL from image cache so it loads fresh
    //  (if we don't do this, loads after the first will always use image cache
    //   and we won't see image edit changes or be able to get actual width and height)
    
    var IOService = GetIOService();
    if (IOService)
    {
      // We must have an absolute URL to preview it or remove it from the cache
      imageSrc = MakeAbsoluteUrl(imageSrc);

      if (GetScheme(imageSrc))
      {
        var uri = IOService.newURI(imageSrc, null, null);
        if (uri)
        {
          var imgCacheService = Components.classes["@mozilla.org/image/cache;1"].getService();
          var imgCache = imgCacheService.QueryInterface(Components.interfaces.imgICache);

          // This returns error if image wasn't in the cache; ignore that
          imgCache.removeEntry(uri);
        }
      }
    }
  } catch(e) {}

  if (gDialog.PreviewImage)
    removeEventListener("load", PreviewImageLoaded, true);

  if (gDialog.ImageHolder.firstChild)
    gDialog.ImageHolder.removeChild(gDialog.ImageHolder.firstChild);
    
  gDialog.PreviewImage = document.createElementNS("http://www.w3.org/1999/xhtml", "html:img");
  if (gDialog.PreviewImage)
  {
    // set the src before appending to the document -- see bug 198435 for why
    // this is needed.
    // XXXbz that bug is long-since fixed.  Is this still needed?
    gDialog.PreviewImage.addEventListener("load", PreviewImageLoaded, true);
    gDialog.PreviewImage.src = imageSrc;
    gDialog.ImageHolder.appendChild(gDialog.PreviewImage);
  }
}

function SetActualSize()
{
  gDialog.widthInput.value = gActualWidth ? gActualWidth : "";
  gDialog.widthUnitsMenulist.selectedIndex = 0;
  gDialog.heightInput.value = gActualHeight ? gActualHeight : "";
  gDialog.heightUnitsMenulist.selectedIndex = 0;
  doDimensionEnabling();
}

function ChangeImageSrc()
{
  if (gTimerID)
    clearTimeout(gTimerID);

  gTimerID = setTimeout("LoadPreviewImage()", 800);

  SetRelativeCheckbox();
  doOverallEnabling();
}

function doDimensionEnabling()
{
  // Enabled only if "Custom" is selected
  var enable = (gDialog.customSizeRadio.selected);

  // BUG 74145: After input field is disabled,
  //   setting it enabled causes blinking caret to appear
  //   even though focus isn't set to it.
  SetElementEnabledById( "heightInput", enable );
  SetElementEnabledById( "heightLabel", enable );
  SetElementEnabledById( "heightUnitsMenulist", enable );

  SetElementEnabledById( "widthInput", enable );
  SetElementEnabledById( "widthLabel", enable);
  SetElementEnabledById( "widthUnitsMenulist", enable );

  var constrainEnable = enable
         && ( gDialog.widthUnitsMenulist.selectedIndex == 0 )
         && ( gDialog.heightUnitsMenulist.selectedIndex == 0 );

  SetElementEnabledById( "constrainCheckbox", constrainEnable );
}

function doOverallEnabling()
{
  var enabled = TrimString(gDialog.srcInput.value) != "";

  SetElementEnabled(gDialog.OkButton, enabled);
  SetElementEnabledById("AdvancedEditButton1", enabled);
  SetElementEnabledById("imagemapLabel", enabled);

  //TODO: Restore when Image Map editor is finished
  //SetElementEnabledById("editImageMap", enabled);
  SetElementEnabledById("removeImageMap", gCanRemoveImageMap);
}

function ToggleConstrain()
{
  // If just turned on, save the current width and height as basis for constrain ratio
  // Thus clicking on/off lets user say "Use these values as aspect ration"
  if (gDialog.constrainCheckbox.checked && !gDialog.constrainCheckbox.disabled
     && (gDialog.widthUnitsMenulist.selectedIndex == 0)
     && (gDialog.heightUnitsMenulist.selectedIndex == 0))
  {
    gConstrainWidth = Number(TrimString(gDialog.widthInput.value));
    gConstrainHeight = Number(TrimString(gDialog.heightInput.value));
  }
}

function constrainProportions( srcID, destID )
{
  var srcElement = document.getElementById(srcID);
  if (!srcElement)
    return;

  var destElement = document.getElementById(destID);
  if (!destElement)
    return;

  // always force an integer (whether we are constraining or not)
  forceInteger(srcID);

  if (!gActualWidth || !gActualHeight ||
      !(gDialog.constrainCheckbox.checked && !gDialog.constrainCheckbox.disabled))
    return;

  // double-check that neither width nor height is in percent mode; bail if so!
  if ( (gDialog.widthUnitsMenulist.selectedIndex != 0)
     || (gDialog.heightUnitsMenulist.selectedIndex != 0) )
    return;

  // This always uses the actual width and height ratios
  // which is kind of funky if you change one number without the constrain
  // and then turn constrain on and change a number
  // I prefer the old strategy (below) but I can see some merit to this solution
  if (srcID == "widthInput")
    destElement.value = Math.round( srcElement.value * gActualHeight / gActualWidth );
  else
    destElement.value = Math.round( srcElement.value * gActualWidth / gActualHeight );

/*
  // With this strategy, the width and height ratio
  //   can be reset to whatever the user entered.
  if (srcID == "widthInput")
    destElement.value = Math.round( srcElement.value * gConstrainHeight / gConstrainWidth );
  else
    destElement.value = Math.round( srcElement.value * gConstrainWidth / gConstrainHeight );
*/
}

function editImageMap()
{
  // Create an imagemap for image map editor
  if (gInsertNewIMap)
  {
    try {
      gImageMap = GetCurrentEditor().createElementWithDefaults("map");
    } catch (e) {}
  }

  // Note: We no longer pass in a copy of the global ImageMap. ImageMap editor should create a copy and manage onOk and onCancel behavior
  window.openDialog("chrome://editor/content/EdImageMap.xul", "_blank", "chrome,close,titlebar,modal", globalElement, gImageMap);
}

function removeImageMap()
{
  gRemoveImageMap = true;
  gCanRemoveImageMap = false;
  SetElementEnabledById("removeImageMap", false);
}

function SwitchToValidatePanel()
{
  if (gDialog.tabBox && gValidateTab && gDialog.tabBox.selectedTab != gValidateTab)
    gDialog.tabBox.selectedTab = gValidateTab;
}

// Get data from widgets, validate, and set for the global element
//   accessible to AdvancedEdit() [in EdDialogCommon.js]
function ValidateImage()
{
  var editor = GetCurrentEditor();
  if (!editor)
    return false;

  gValidateTab = gDialog.tabLocation;
  if (!gDialog.srcInput.value)
  {
    AlertWithTitle(null, GetString("MissingImageError"));
    SwitchToValidatePanel();
    gDialog.srcInput.focus();
    return false;
  }

  //TODO: WE NEED TO DO SOME URL VALIDATION HERE, E.G.:
  // We must convert to "file:///" or "http://" format else image doesn't load!
  var src = TrimString(gDialog.srcInput.value);
  globalElement.setAttribute("src", src);

  var title = TrimString(gDialog.titleInput.value);
  if (title)
    globalElement.setAttribute("title", title);
  else
    globalElement.removeAttribute("title");

  // Force user to enter Alt text only if "Alternate text" radio is checked
  // Don't allow just spaces in alt text
  var alt = "";
  var useAlt = gDialog.altTextRadioGroup.selectedItem == gDialog.altTextRadio;
  if (useAlt)
    alt = TrimString(gDialog.altTextInput.value);

  if (gDoAltTextError && useAlt && !alt)
  {
    AlertWithTitle(null, GetString("NoAltText"));
    SwitchToValidatePanel();
    gDialog.altTextInput.focus();
    return false;
  }
  globalElement.setAttribute("alt", alt);

  var width = "";
  var height = "";

  gValidateTab = gDialog.tabDimensions;
  if (!gDialog.actualSizeRadio.selected)
  {
    // Get user values for width and height
    width = ValidateNumber(gDialog.widthInput, gDialog.widthUnitsMenulist, 1, gMaxPixels, 
                           globalElement, "width", false, true);
    if (gValidationError)
      return false;

    height = ValidateNumber(gDialog.heightInput, gDialog.heightUnitsMenulist, 1, gMaxPixels, 
                            globalElement, "height", false, true);
    if (gValidationError)
      return false;
  }

  // We always set the width and height attributes, even if same as actual.
  //  This speeds up layout of pages since sizes are known before image is loaded
  if (!width)
    width = gActualWidth;
  if (!height)
    height = gActualHeight;

  // Remove existing width and height only if source changed
  //  and we couldn't obtain actual dimensions
  var srcChanged = (src != gOriginalSrc);
  if (width)
    editor.setAttributeOrEquivalent(globalElement, "width", width, true);
  else if (srcChanged)
    editor.removeAttributeOrEquivalent(globalElement, "width", true);

  if (height)
    editor.setAttributeOrEquivalent(globalElement, "height", height, true);
  else if (srcChanged) 
    editor.removeAttributeOrEquivalent(globalElement, "height", true);

  // spacing attributes
  gValidateTab = gDialog.tabBorder;
  ValidateNumber(gDialog.imagelrInput, null, 0, gMaxPixels, 
                 globalElement, "hspace", false, true, true);
  if (gValidationError)
    return false;

  ValidateNumber(gDialog.imagetbInput, null, 0, gMaxPixels, 
                 globalElement, "vspace", false, true);
  if (gValidationError)
    return false;

  // note this is deprecated and should be converted to stylesheets
  ValidateNumber(gDialog.border, null, 0, gMaxPixels, 
                 globalElement, "border", false, true);
  if (gValidationError)
    return false;

  // Default or setting "bottom" means don't set the attribute
  // Note that the attributes "left" and "right" are opposite
  //  of what we use in the UI, which describes where the TEXT wraps,
  //  not the image location (which is what the HTML describes)
  switch ( gDialog.alignTypeSelect.value )
  {
    case "top":
    case "middle":
    case "right":
    case "left":
      editor.setAttributeOrEquivalent( globalElement, "align",
                                       gDialog.alignTypeSelect.value , true);
      break;
    default:
      try {
        editor.removeAttributeOrEquivalent(globalElement, "align", true);
      } catch (e) {}
  }

  return true;
}
