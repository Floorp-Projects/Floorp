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
 * The Original Code is CaScadeS, a stylesheet editor for Composer.
 *
 * The Initial Developer of the Original Code is
 * Daniel Glazman.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original author: Daniel Glazman <daniel@glazman.org>
 *   Charley Manske <cmanske@netscape.com>, for local fonts access
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var gTimerID;
var textColor, backgroundColor, borderColor;
var gLocalFonts = null;
const kUrlString = "url(";


// * if |value| is not empty, sets the CSS property |property| to |value|
//   in |elt| element's inline styles ; otherwise removes the property
//   param XULElement elt
//   param String property
//   param String value
function AddStyleToElement(elt, property, value)
{
  if (!elt || !elt.style) return;
  if (value == "") {
    elt.style.removeProperty(property);
  }
  else {
    try {
      elt.style.setProperty(property, value,
                            elt.style.getPropertyPriority(property));
    }
    catch (ex) {  }
  }
}

// * Callback from font-family selection XUL elts
//   param String fontFamily
function onFontFamilySelect(fontFamily)
{
  if (!gDialog.selectedObject) return;
  var enableCustomInput = false;
  var enablePredefMenu  = false;
  if (fontFamily == "") {
    // nothing to do here
  }
  else if (fontFamily == "-user-defined") {
    // user wants to select a font that is not pre-defined
    // so let's get that user defined name from the textarea
    enableCustomInput = true;
    fontFamily = gDialog.customFontFamilyInput.value;
  }
  else if (fontFamily == "-predefined"){
    fontFamily = "";
    enablePredefMenu = true;
    // user wants a predefined font family
    var valueElt = gDialog.predefFontFamilyMenulist.selectedItem;
    if (valueElt) {
      fontFamily = valueElt.value;
    }
  }
  else {
    // all other cases are user defined fonts
    enablePredefMenu = true;
  }

  EnableUI(gDialog.predefFontFamilyMenulist, enablePredefMenu);
  EnableUI(gDialog.customFontFamilyInput, enableCustomInput);

  AddStyleToElement(gDialog.brownFoxLabel,  "font-family", fontFamily);
  AddStyleToElement(gDialog.selectedObject, "font-family", fontFamily);
  
  SetModifiedFlagOnStylesheet();
}

// * Callback when the color textarea's value is changed
//   it sets the correspondinf property/value on the element elt
//   and sets the color of the corresponding UI button
//   param XULElement elt
//   param String property
//   param String id
//   param String ColorWellID
function onColorZoneInput(elt, property, id, ColorWellID)
{
  ChangeValueOnInput(elt, property, id);
  setColorWell(ColorWellID, elt.value);
}

// * Opens a color picker for the requested property and handles
//   the result
//   param String ColorWellID
function GetColorAndUpdate(ColorWellID)
{
  var colorObj = new Object;
  var colorWell = document.getElementById(ColorWellID);
  if (!colorWell) return;

  // Don't allow a blank color, i.e., using the "default"
  colorObj.NoDefault = true;

  switch( ColorWellID )
  {
    case "textCW":
      colorObj.Type = "Text";
      colorObj.TextColor = textColor;
      break;
    case "backgroundCW":
      colorObj.Type = "Page";
      colorObj.PageColor = backgroundColor;
      break;
    case "bordertopCW":
    case "borderleftCW":
    case "borderrightCW":
    case "borderbottomCW":
      colorObj.Type = "Text";
      colorObj.TextColor = borderColor;
      break;
  }

  window.openDialog("chrome://editor/content/EdColorPicker.xul", "_blank", "chrome,close,titlebar,modal", "", colorObj);

  // User canceled the dialog
  if (colorObj.Cancel)
    return;

  var color = "";
  var property = "";
  var textZone = null;
  switch( ColorWellID )
  {
    case "textCW":
      color = textColor = colorObj.TextColor;
      property = "color";
      AddStyleToElement(gDialog.brownFoxLabel, "color", color);
      textZone = gDialog.textColorInput;
      break;
    case "backgroundCW":
      color = backgroundColor = colorObj.BackgroundColor;
      property = "background-color";
      textZone = gDialog.backgroundColorInput;
      break;
    case "bordertopCW":
      color = textColor = colorObj.TextColor;
      property = "border-top-color";
      AddStyleToElement(gDialog.borderPreview, "border-top-color", color);
      textZone = gDialog.topBorderColorInput;
      break;
    case "borderleftCW":
      color = textColor = colorObj.TextColor;
      property = "border-left-color";
      AddStyleToElement(gDialog.borderPreview, "border-left-color", color);
      textZone = gDialog.leftBorderColorInput;
      break;
    case "borderrightCW":
      color = textColor = colorObj.TextColor;
      property = "border-right-color";
      AddStyleToElement(gDialog.borderPreview, "border-right-color", color);
      textZone = gDialog.rightBorderColorInput;
      break;
    case "borderbottomCW":
      color = textColor = colorObj.TextColor;
      property = "border-bottom-color";
      AddStyleToElement(gDialog.borderPreview, "border-bottom-color", color);
      textZone = gDialog.bottomBorderColorInput;
      break;
  }

  setColorWell(ColorWellID, color); 
  if (textZone != null) {
    textZone.value = color;
    if (ColorWellID == "bordertopCW")
      IfFourSidesSameStyle('border', 'color', color, 'allFourBordersSame', 'borderPreview', null);
    else if (ColorWellID == "backgroundCW")
      AddStyleToElement(gDialog.backgroundPreview, "background-color", color);
  }
  try {
    gDialog.selectedObject.style.setProperty(property,
                              color,
                              gDialog.selectedObject.style.getPropertyPriority(property));
  }
  catch (ex) {}
  SetModifiedFlagOnStylesheet();
}

// * Callback when a textarea's value is changed
//   it sets the corresponding property/value on the element elt
//   param XULElement elt
//   param String property
//   param String id
function ChangeValueOnInput(elt, property, id)
{
  if (!gDialog.selectedObject) return;
  var value = elt.value;
  if (id != null) {
    elt = document.getElementById(id);
    if (elt) {
      AddStyleToElement(elt, property, value);
    }
  }
  AddStyleToElement(gDialog.selectedObject, property, value);
  SetModifiedFlagOnStylesheet();
}

// * Callback when a pulldown menu entry is selected
//   param String property
//   param String value
//   param String id
function onPullDownChanged(property, value, id)
{
  if (!gDialog.selectedObject) return;
  if (id != null) {
    var elt = document.getElementById(id);
    if (elt) {
      AddStyleToElement(elt, property, value);
    }
  }
  AddStyleToElement(gDialog.selectedObject, property, value);
  SetModifiedFlagOnStylesheet();
}

// * Callback when the 'none' value is selected for the
//   'text-decoration' property; it unchecks all other checkboxes
//   if the 'none' checkbox is checked
function onNoneTextDecorationChange()
{
  if (!gDialog.selectedObject) return;

  var textDecoration = "";
  if (gDialog.noDecorationCheckbox.checked) {
    // uncheck other possible values for the property if 'none' is checked
    gDialog.textUnderlineCheckbox.checked   = false;
    gDialog.textOverlineCheckbox.checked    = false;
    gDialog.textLinethroughCheckbox.checked = false;
    gDialog.textBlinkCheckbox.checked       = false;
    textDecoration = "none";
  }
  AddStyleToElement(gDialog.selectedObject, "text-decoration", textDecoration);
  AddStyleToElement(gDialog.brownFoxLabel, "text-decoration", textDecoration);
  SetModifiedFlagOnStylesheet();
}
    
// * Callback when one of the chekboxes but for 'none' for
//   'text-decoration' property sees his state changed ; it
//   recomputes the value of the property appending the various
//   individual values
function onTextDecorationChange()
{
  if (!gDialog.selectedObject) return;

  var textDecoration = "";
  if (gDialog.textUnderlineCheckbox.checked)     textDecoration += "underline ";
  if (gDialog.textOverlineCheckbox.checked)      textDecoration += "overline ";
  if (gDialog.textLinethroughCheckbox.checked)   textDecoration += "line-through ";
  if (gDialog.textBlinkCheckbox.checked)         textDecoration += "blink ";
  if (textDecoration != "") {
    gDialog.noDecorationCheckbox.checked = false;
  }
  AddStyleToElement(gDialog.selectedObject, "text-decoration", textDecoration);
  AddStyleToElement(gDialog.brownFoxLabel, "text-decoration", textDecoration);
  SetModifiedFlagOnStylesheet();
}

// * This function sets a timer when the user changes an image URL
//   in a textarea. This leaves the user enough time to complete the
//   URL and avoids Gecko trying to load an incomplete URL
function ChangeImageSrc()
{
  if (gTimerID)
    clearTimeout(gTimerID);

  gTimerID = setTimeout("onBgImageUrlChanged()", 800);
}

// * This is the callback from ChangeImageSrc(). It loads an image and uses
//   the cache.
function onBgImageUrlChanged()
{
  var property = "background-image";
  var id = "backgroundPreview";

  if (!gDialog.selectedObject) return;
  var value = gDialog.backgroundImageInput.value;
  if (value != "" && value != "none") {
    try {
      // Remove the image URL from image cache so it loads fresh
      //  (if we don't do this, loads after the first will always use image cache
      //   and we won't see image edit changes or be able to get actual width and height)
      
      var IOService = GetIOService();
      if (IOService)
      {
        // We must have an absolute URL to preview it or remove it from the cache
        value = MakeAbsoluteUrl(value);

        if (GetScheme(value))
        {
          var uri = IOService.newURI(value, null, null);
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
    value = kUrlString + value + ")";
  }
  if (id != null) {
    var xulElt = document.getElementById(id);
    if (xulElt) {
      AddStyleToElement(xulElt, property, value);
    }
  }
  AddStyleToElement(gDialog.selectedObject, property, value);
  SetModifiedFlagOnStylesheet();

  if (property == "background-image") {
    enableBackgroundProps((value != ""));
  }
}

// * This function enable some of the background-* properties that
//   only make sense if there is a background image specified
//   param boolean enable
function enableBackgroundProps(enable)
{
  var idArray = [ "backgroundRepeatLabel", "backgroundRepeatMenulist", "backgroundAttachmentCheckbox",
                   "backgroundPositionLabel", "xBackgroundPositionRadiogroup",
                   "yBackgroundPositionRadiogroup" ];
  var i, elt, lookAtNextSibling;

  for (i = 0; i < idArray.length; i++) {
    elt = document.getElementById(idArray[i]);
    lookAtNextSibling = false;
    if (elt.nodeName.toLowerCase() == "radiogroup") {
      elt = elt.firstChild;
      lookAtNextSibling = true;
    }
    while (elt) {
      EnableUI(elt, enable);
      if (lookAtNextSibling) {
        elt = elt.nextSibling;
      }
      else {
        elt = null;
      }
    }
  }
}

// * open a file picker dialog for the requested file filter and
//   returns the corresponding file URL into the element of ID id
//   param String filterType
//   param String id
function chooseFile(filterType, id)
{
  // Get a local image file, converted into URL format
  var fileName = GetLocalFileURL(filterType);
  var elt = document.getElementById(id);
  if (fileName)
  {
    // Always try to relativize local file URLs
    if (gHaveDocumentUrl)
      fileName = MakeRelativeUrl(fileName);

    elt.value = fileName;
    onBgImageUrlChanged();
  }
  SetTextboxFocus(elt);
}

// * Callback when the user changes the value for background-attachment
function onBackgroundAttachmentChange()
{
  if (!gDialog.selectedObject) return;
  // the checkbox is checked if the background scrolls with the page
  var value = gDialog.backgroundAttachmentCheckbox.checked ? "" : "fixed";
  AddStyleToElement(gDialog.selectedObject, "background-attachment", value);
  SetModifiedFlagOnStylesheet();
}

// * Callback when one of the background positions (x or y) is changed
function onBackGroundPositionSelect()
{
  var xPosition = gDialog.xBackgroundPositionRadiogroup.selectedItem.value;
  var yPosition = gDialog.yBackgroundPositionRadiogroup.selectedItem.value;
  var value = xPosition + " " + yPosition;
  AddStyleToElement(gDialog.backgroundPreview, "background-position", value);
  AddStyleToElement(gDialog.selectedObject, "background-position", value);
  SetModifiedFlagOnStylesheet();
}

// * Inits the values shown in the Text tab panel
function InitTextTabPanel()
{
  gDialog.selectedTab = TEXT_TAB;
  if (!gDialog.selectedObject || !gDialog.selectedObject.style)
    return;

  /* INIT FONT-FAMILY */
  var fontFamily       = getSpecifiedStyle("font-family");
  AddStyleToElement(gDialog.brownFoxLabel, "font-family", fontFamily);
  var fontFamilyChoice = "noFontFamilyRadio";
  var predefFontFamilyChoice = "";
  var enableCustomInput = false;
  var enablePredefMenu  = false;


  if (fontFamily == "arial,helvetica,sans-serif") {
    fontFamilyChoice = "predefFontFamilyRadio";
    predefFontFamilyChoice = "sansSerifFontFamilyMenuitem";
    enablePredefMenu = true;
  }
  else if (fontFamily == "times new roman,times,serif") {
    fontFamilyChoice = "predefFontFamilyRadio";
    predefFontFamilyChoice = "serifFontFamilyMenuitem";
    enablePredefMenu = true;
  }
  else if (fontFamily == "courier new,courier,monospace") {
    fontFamilyChoice = "predefFontFamilyRadio";
    predefFontFamilyChoice = "monospaceFontFamilyMenuitem";
    enablePredefMenu = true;
  }
  else if (fontFamily != "") {
    fontFamilyChoice = "customFontFamilyRadio";
    gDialog.customFontFamilyInput.value = fontFamily;
    enableCustomInput = true;
  }
  /* selects the radio button */
  var fontFamilyItem = document.getElementById(fontFamilyChoice);
  gDialog.fontFamilyRadiogroup.selectedItem = fontFamilyItem;
  /* enablers/disablers */
  if (enablePredefMenu) {
    gDialog.predefFontFamilyMenulist.removeAttribute("disabled");
  }
  else {
    gDialog.predefFontFamilyMenulist.setAttribute("disabled", "true");
  }
  if (enableCustomInput) {
    gDialog.customFontFamilyInput.removeAttribute("disabled");
  }
  else {
    gDialog.customFontFamilyInput.setAttribute("disabled", "true");
  }

  if (predefFontFamilyChoice != "") {
    var predefFontFamilyMenuitem = document.getElementById(predefFontFamilyChoice);
    gDialog.predefFontFamilyMenulist.selectedItem = predefFontFamilyMenuitem;
  }

  /* INIT COLOR */
  textColor = ConvertRGBColorIntoHEXColor(getSpecifiedStyle("color"));
  backgroundColor = ConvertRGBColorIntoHEXColor(getSpecifiedStyle("background-color"));
  gDialog.textColorInput.setAttribute("value", textColor);
  AddStyleToElement(gDialog.brownFoxLabel, "color", textColor);
  if (backgroundColor == "")
    backgroundColor = "white";
  AddStyleToElement(gDialog.brownFoxLabel, "background-color", backgroundColor);
  setColorWell("textCW", textColor); 

  /* INIT FONT-STYLE */
  SelectsOneChoice("font-style", "fontStyleMenulist", "FontStyleMenu", gDialog.brownFoxLabel)

  /* INIT FONT-WEIGHT */
  SelectsOneChoice("font-weight", "fontWeightMenulist", "FontWeightMenu", gDialog.brownFoxLabel)

  /* INIT TEXT-TRANSFORM */
  SelectsOneChoice("text-transform", "textTransformMenulist", "TextTransformMenu", gDialog.brownFoxLabel)

  /* INIT TEXT-ALIGN */
  SelectsOneChoice("text-align", "textAlignMenulist", "TextAlignMenu", gDialog.brownFoxLabel)

  /* INIT TEXT-DECORATION */
  var textDecoration = getSpecifiedStyle("text-decoration");
  AddStyleToElement(gDialog.brownFoxLabel, "text-decoration", textDecoration);
  SelectsCheckboxIfValueContains(textDecoration, "underline", "underlineTextDecorationCheckbox");
  SelectsCheckboxIfValueContains(textDecoration, "overline", "overlineTextDecorationCheckbox");
  SelectsCheckboxIfValueContains(textDecoration, "line-through", "linethroughTextDecorationCheckbox");
  SelectsCheckboxIfValueContains(textDecoration, "blink", "blinkTextDecorationCheckbox");
  SelectsCheckboxIfValueContains(textDecoration, "none", "noneTextDecorationCheckbox");

  /* INIT FONT SIZE */
  var fontSize = getSpecifiedStyle("font-size");
  if (!fontSize || fontSize == "")
    gDialog.fontSizeInput.value = "";
  else
    gDialog.fontSizeInput.value = fontSize;
  AddStyleToElement(gDialog.brownFoxLabel, "font-size", fontSize);

  /* INIT LINE-HEIGHT */
  var lineHeight = getSpecifiedStyle("line-height");
  if (!lineHeight || lineHeight == "")
    gDialog.lineHeightInput.value = "";
  else
    gDialog.lineHeightInput.value = lineHeight;
  
}

// * Utility function to check if the four side values of a property
//   are equal so we an show a shorthand in the dialog
//   param String top
//   param String left
//   param String right
//   param String bottom
//   return boolean
function AreAllFourValuesEqual(top, left, right, bottom)
{
  return (top == left && top == right && top == bottom);
}

// * Retrieves the specified style for the property |prop| and
//   makes it become the value of the XUL element of ID |id|
//   param String id
//   param String prop
function SetTextzoneValue(id, prop)
{
  var elt = document.getElementById(id);
  if (elt) {
    var value = getSpecifiedStyle(prop);
    if (!value)
      elt.value = "";
    else
      elt.value = value;
  }
}

// * Inits the values shown in the Aural tab panel
function InitAuralTabPanel()
{
  gDialog.selectedTab = AURAL_TAB;
  if (!gDialog.selectedObject || !gDialog.selectedObject.style)
    return;

  // observe the scrollbar and call onScrollbarAttrModified when an attribute is modified
  gDialog.volumeScrollbar.removeEventListener("DOMAttrModified", onVolumeScrollbarAttrModified, false);
  var volume = getSpecifiedStyle("volume");
  if (volume == "silent") {
    gDialog.muteVolumeCheckbox.checked = true;
    gDialog.volumeScrollbar.setAttribute("disabled", "true");
  }
  else {
    gDialog.muteVolumeCheckbox.checked = false;
    gDialog.volumeScrollbar.removeAttribute("disabled");
  }
  // if no volume specified, display medium value
  if (!volume)
    volume = "50";
  gDialog.volumeMenulist.value = volume;
  UpdateScrollbar(gDialog.volumeMenulist, 'volumeScrollbar');

  gDialog.volumeScrollbar.addEventListener("DOMAttrModified", onVolumeScrollbarAttrModified, false);

  SelectsOneChoice("speak", "speakMenulist", "SpeakMenu", null);
  SelectsOneChoice("speech-rate", "speechRateMenulist", "SpeechRateMenu", null);
}

// * this is a callback of a listener set on the scrollbar for volume setting
//   param DOMMutationEvent aEvent
function onVolumeScrollbarAttrModified(aEvent)
{
  if (aEvent.attrName == "curpos") {
    var e = gDialog.volumeMenulist;
    if (e.value == "silent") return;
    var v = Math.floor(aEvent.newValue / 10);
    e.value = v;
    AddStyleToElement(gDialog.selectedObject, "volume", v);
    SetModifiedFlagOnStylesheet();
  }
}

function onOpacityScrollbarAttrModified(aEvent)
{
  if (aEvent.attrName == "curpos") {
    var v = aEvent.newValue / 1000;
    if (v == 1)
      gDialog.opacityLabel.setAttribute("value", "transparent");
    else
      gDialog.opacityLabel.setAttribute("value", v);    
    AddStyleToElement(gDialog.selectedObject, "-moz-opacity", v);
    SetModifiedFlagOnStylesheet();
  }
}

// * when the user checks the "mute" checkbox, 'silent' should become
//   the new value of the 'volume' property. If the user unchecks it
//   the value should be computed from the volume scrollbar's position
//   param XULElement elt
function MuteVolume(elt)
{
  var v;
  if (elt.checked) {
    v = "silent";
    gDialog.volumeScrollbar.setAttribute("disable", "true");
  }
  else {
    v = Math.floor(gDialog.volumeScrollbar.getAttribute("curpos") / 10);
    gDialog.volumeScrollbar.removeAttribute("disable");
  }
  gDialog.volumeMenulist.value = v;
  AddStyleToElement(gDialog.selectedObject, "volume", v);
  SetModifiedFlagOnStylesheet();
}

// * updates the volume's scrollbar position using the value of the
//   'volume' property stored in the value of the XUL element |elt|.
//   It also converts pre-defined volume names to the equivalent
//   numerical value (defined in CSS 2 spec)
//   param XULElement elt
//   param String id
function UpdateScrollbar(elt, id)
{
  var v = elt.value, x = 0;
  if (v == "silent") return;
  var e = document.getElementById(id);
  switch (v) {
    case "x-soft": x=0; break;
    case "soft":   x=250; break;
    case "medium": x=500; break;
    case "loud":   x=750; break;
    case "x-loud": x=1000; break;
    default:
      x = parseInt(v) * 10;
      // if we don't have a number, it must be a pre-defined keyword
      // and it can't be one because we checked that above
      if (isNaN(x)) x = 0;
      break;
  }
  // let's update the scrollbar's position
  e.setAttribute("curpos",  x);
}

// * Inits the values shown in the Box model tab panel
function InitBoxTabPanel()
{
  gDialog.selectedTab = BOX_TAB;
  if (!gDialog.selectedObject || !gDialog.selectedObject.style)
    return;

  SelectsOneChoice("display",    "displayMenulist",    "DisplayMenuitem", null);
  SelectsOneChoice("visibility", "visibilityMenulist", "VisibilityMenuitem", null);
  SelectsOneChoice("float",      "floatMenulist",      "FloatMenuitem", null);
  SelectsOneChoice("clear",      "clearMenulist",      "ClearMenuitem", null);
  SelectsOneChoice("position",   "positionMenulist",   "PositionMenuitem", null);
  SelectsOneChoice("overflow",   "overflowMenulist",   "OverflowMenuitem", null);

  SetTextzoneValue("zindexInput",                  "z-index");
  SetTextzoneValue("margintopEditableMenulist",    "margin-top");
  SetTextzoneValue("paddingtopEditableMenulist",   "padding-top");
  SetTextzoneValue("topEditableMenulist",          "top");
  SetTextzoneValue("marginleftEditableMenulist",   "margin-left");
  SetTextzoneValue("paddingleftEditableMenulist",  "padding-left");
  SetTextzoneValue("leftEditableMenulist",         "left");
  SetTextzoneValue("marginrightEditableMenulist",  "margin-right");
  SetTextzoneValue("paddingrightEditableMenulist", "padding-right");
  SetTextzoneValue("rightEditableMenulist",        "right");
  SetTextzoneValue("marginbottomEditableMenulist", "margin-bottom");
  SetTextzoneValue("paddingbottomEditableMenulist","padding-bottom");
  SetTextzoneValue("bottomEditableMenulist",       "bottom");

  SetTextzoneValue("widthEditableMenulist",        "width");
  SetTextzoneValue("minwidthEditableMenulist",     "min-width");
  SetTextzoneValue("maxwidthEditableMenulist",     "max-width");
  SetTextzoneValue("heightEditableMenulist",       "height");
  SetTextzoneValue("minheightEditableMenulist",    "min-height");
  SetTextzoneValue("maxheightEditableMenulist",    "max-height");

}

// * Inits the values shown in the Border tab panel
function InitBorderTabPanel()
{
  gDialog.selectedTab = BORDER_TAB;
  if (!gDialog.selectedObject || !gDialog.selectedObject.style)
    return;

  /* INIT */
  var style = [ getSpecifiedStyle("border-top-style"),
                getSpecifiedStyle("border-left-style"),
                getSpecifiedStyle("border-right-style"),
                getSpecifiedStyle("border-bottom-style")
              ];
  var width = [ getSpecifiedStyle("border-top-width"),
                getSpecifiedStyle("border-left-width"),
                getSpecifiedStyle("border-right-width"),
                getSpecifiedStyle("border-bottom-width")
              ];
  var color = [ ConvertRGBColorIntoHEXColor(getSpecifiedStyle("border-top-color")),
                ConvertRGBColorIntoHEXColor(getSpecifiedStyle("border-left-color")),
                ConvertRGBColorIntoHEXColor(getSpecifiedStyle("border-right-color")),
                ConvertRGBColorIntoHEXColor(getSpecifiedStyle("border-bottom-color"))
              ];

  var sameFourSides = false;
  // let's see if we have same style/color/width for all four borders
  if (AreAllFourValuesEqual(style[0], style[1], style[2], style[3]) &&
      AreAllFourValuesEqual(width[0], width[1], width[2], width[3]) &&
      AreAllFourValuesEqual(getHexColorFromColorName(color[0]),
                            getHexColorFromColorName(color[1]),
                            getHexColorFromColorName(color[2]),
                            getHexColorFromColorName(color[3]))) {
    sameFourSides = true;
  }

  var sideArray = [ "top", "left", "right", "bottom" ];

  for (var i=0; i<sideArray.length; i++) {
    var styleMenu = document.getElementById( sideArray[i]+"BorderStyleMenulist" );
    var colorInput = document.getElementById( sideArray[i]+"BorderColorInput" );
    var widthInput = document.getElementById( sideArray[i]+"BorderWidthInput" );
    if (!i || !sameFourSides) {
      EnableUI(styleMenu, true);
      EnableUI(colorInput, true);
      EnableUI(widthInput, true);

      SelectsOneChoice("border-"+sideArray[i]+"-style",
                       sideArray[i]+"BorderStyleMenulist",
                       sideArray[i]+"BorderStyle", gDialog.borderPreview)

      colorInput.value =  color[i];
      setColorWell("border"+sideArray[i]+"CW", color[i]);

      AddStyleToElement(gDialog.borderPreview, "border-"+sideArray[i]+"-color", color[i]);

      widthInput.value =  width[i];

      AddStyleToElement(gDialog.borderPreview, "border-"+sideArray[i]+"-width", width[i]);
    }
    else {
      EnableUI(styleMenu, false);
      EnableUI(colorInput, false);
      EnableUI(widthInput, false);
      colorInput.value = "";
      setColorWell("border"+sideArray[i]+"CW", "");
      widthInput.value = "";
      styleMenu.selectedItem = document.getElementById("unspecified" + sideArray[i] + "BorderStyle");
      AddStyleToElement(gDialog.borderPreview, "border-"+sideArray[i]+"-color", color[0]);
      AddStyleToElement(gDialog.borderPreview, "border-"+sideArray[i]+"-style", style[0]);
      AddStyleToElement(gDialog.borderPreview, "border-"+sideArray[i]+"-width", width[0]);

    }
  }
  if (sameFourSides)
    gDialog.allFourBordersSame.setAttribute("checked", "true");
  else
    gDialog.allFourBordersSame.removeAttribute("checked");
}
 
// * Inits the values shown in the Background tab panel
function InitBackgroundTabPanel()
{
  gDialog.selectedTab = BACKGROUND_TAB;
  if (!gDialog.selectedObject || !gDialog.selectedObject.style)
    return;

  gDialog.opacityScrollbar.removeEventListener("DOMAttrModified", onOpacityScrollbarAttrModified, false);
  var opacity = getSpecifiedStyle("-moz-opacity");
  if (opacity == "")
    opacity = 1;
  gDialog.opacityScrollbar.setAttribute("curpos", opacity*1000);
  gDialog.opacityScrollbar.addEventListener("DOMAttrModified", onOpacityScrollbarAttrModified, false);
  
  /* INIT BACKGROUND-COLOR */
  backgroundColor = ConvertRGBColorIntoHEXColor(getSpecifiedStyle("background-color"));
  gDialog.backgroundColorInput.setAttribute("value", backgroundColor);
  if (backgroundColor == "")
    backgroundColor = "white";
  setColorWell("backgroundCW", backgroundColor); 
  AddStyleToElement(gDialog.backgroundPreview, "background-color", backgroundColor);

  /* INIT BACKGROUND-REPEAT */
  var backgroundRepeat = getSpecifiedStyle("background-repeat");
  var choice = "repeatBackgroundRepeatMenu";
  if (backgroundRepeat == "no-repeat") {
    choice = "norepeatBackgroundRepeatMenu";
  }
  else if (backgroundRepeat == "repeat-x") {
    choice = "horizontallyBackgroundRepeatMenu";
  }
  else if (backgroundRepeat == "repeat-y") {
    choice = "verticallyBackgroundRepeatMenu";
  }
  else if (backgroundRepeat == "repeat") {
    choice = "repeatBackgroundRepeatMenu";
  }
  AddStyleToElement(gDialog.backgroundPreview, "background-repeat", backgroundRepeat);
  var choiceItem = document.getElementById(choice);
  gDialog.backgroundRepeatMenulist.selectedItem = choiceItem;

  /* INIT BACKGROUND-IMAGE */
  var backgroundImage = getSpecifiedStyle("background-image");
  gDialog.backgroundImageInput.setAttribute("value", backgroundImage);
  enableBackgroundProps( (backgroundImage != "none" && backgroundImage != "") );
  AddStyleToElement(gDialog.backgroundPreview, "background-image", backgroundImage);
  
  /* INIT BACKGROUND-ATTACHMENT */
  var backgroundAttachment = getSpecifiedStyle("background-attachment");
  gDialog.backgroundAttachmentCheckbox.checked = ( (backgroundAttachment == "fixed") ? false : true );
  AddStyleToElement(gDialog.backgroundPreview, "background-attachment", backgroundAttachment);

  /* INIT BACKGROUND-POSITION */
  var xBackgroundPosition = getSpecifiedStyle("-x-background-x-position");
  var yBackgroundPosition = getSpecifiedStyle("-x-background-y-position");
  var xRadio, yRadio;
  if (xBackgroundPosition == "100%" || xBackgroundPosition == "right")
    xRadio = "rightXBackgroundPositionRadio";
  else if (xBackgroundPosition == "50%" || xBackgroundPosition == "center")
    xRadio = "centerXBackgroundPositionRadio";
  else
    xRadio = "leftXBackgroundPositionRadio";
  if (yBackgroundPosition == "100%" || yBackgroundPosition == "bottom")
    yRadio = "bottomYBackgroundPositionRadio";
  else if (xBackgroundPosition == "50%" || yBackgroundPosition == "center")
    yRadio = "centerYBackgroundPositionRadio";
  else
    yRadio = "topYBackgroundPositionRadio";

  gDialog.xBackgroundPositionRadiogroup.selectedItem = document.getElementById(xRadio);
  gDialog.yBackgroundPositionRadiogroup.selectedItem = document.getElementById(yRadio);
  AddStyleToElement(gDialog.backgroundPreview, "background-attachment",
                    xBackgroundPosition + " " + yBackgroundPosition);
}

// * if the string |value| contains the substring |searchedString|, the
//   checkbox of id |id| is checked ; it's unchecked otherwise.
//   param String value
//   param String searchedString
//   param String id
function SelectsCheckboxIfValueContains(value, searchedString, id)
{
  if (checkbox) {
    var state = (value.indexOf(searchedString) != -1);
    var checkbox = document.getElementById(id);
    checkbox.checked = state;
  }
}

// * First, if |xulElt| is not null, this function copies the
//   specified style for the CSS property |property| to the inline
//   styles carried by |xulElt|. Then it retrieves the property's
//   value, remove possible dashes, and append |idSuffix| to it.
//   It forms the ID of a menu item that will be selected in the
//   menulist |idMenuList|.
//   param String property
//   param String idMenuList
//   param String idSuffix
//   param XULElement xulElt
function SelectsOneChoice(property, idMenulist, idSuffix, xulElt)
{
  var propertyValue = getSpecifiedStyle(property);
  if (xulElt) {
    AddStyleToElement(xulElt, property, propertyValue);
  }
  propertyValue = TrimDashes(propertyValue);
  var menulist = document.getElementById(idMenulist);
  var choice = "unspecified" + idSuffix;
  if (propertyValue && propertyValue != "") {
    choice = propertyValue + idSuffix;
  }
  var choiceItem = document.getElementById(choice);
  menulist.selectedItem = choiceItem;
}

// * utility function to convert predefined HTML4 color names
//   into their #rrggbb equivalent
//   param String color
//   return String
function getHexColorFromColorName(color)
{
  var namedColorsArray = [
    { name:"aqua",     value:"#00ffff" },
    { name:"black",    value:"#000000" },
    { name:"blue",     value:"#0000ff" },
    { name:"fuchsia",  value:"#ff00ff" },
    { name:"gray",     value:"#808080" },
    { name:"green",    value:"#008000" },
    { name:"lime",     value:"#00ff00" },
    { name:"maroon",   value:"#800000" },
    { name:"navy",     value:"#000080" },
    { name:"olive",    value:"#808000" },
    { name:"purple",   value:"#800080" },
    { name:"red",      value:"#ff0000" },
    { name:"silver",   value:"#c0c0c0" },
    { name:"teal",     value:"#008080" },
    { name:"white",    value:"#ffffff" },
    { name:"yellow",   value:"#ffff00" }
    ];
  for (var i=0; i< namedColorsArray.length; i++) {
    if (color == namedColorsArray[i].name) {
      color = namedColorsArray[i].value;
      break;
    }
  }
  return color;
}

// * retrieve the specified value for the CSS property |property|.
//   The beauty of the CSS OM is that the same exact function can be
//   used for both inline styles carried by an element, and rules in a
//   stylesheet
//   param String property
//   return String
function getSpecifiedStyle(property)
{
  if (gDialog.selectedObject && gDialog.selectedObject.style) {
    return gDialog.selectedObject.style.getPropertyValue(property).toLowerCase();
  }

  return null;
}

// * This recreates the textual content of an embedded stylesheet
//   param DOMCSSStyleSheet sheet
function SerializeEmbeddedSheet(sheet)
{
  var ownerNode = sheet.ownerNode;
  var cssText = "\n";
  var l = sheet.cssRules.length;
  var tmp;
  for (var j = 0; j< l; j++) {
    tmp = sheet.cssRules.item(j).cssText;
    var re = /;\s*/;
    var cssTextSplitted = tmp.split(re);
    cssText += "  ";
    for (var k=0; k<cssTextSplitted.length; k++) {
      cssText += cssTextSplitted[k];
      if (k != cssTextSplitted.length-1)
        cssText += ";\n    ";
    }
    if (l > 1) cssText += "\n\n";
  }
  var child = ownerNode.lastChild;
  while (child) {
    tmp = child.previousSibling;
    ownerNode.removeChild(child);
    child = tmp;
  }
  var textNode = document.createTextNode(cssText + "\n");
  ownerNode.appendChild(textNode);
}

// * This recreates the textual content of an external stylesheet
//   param DOMCSSStyleSheet sheet
//   param String href
function SerializeExternalSheet(sheet, href)
{
  const classes             = Components.classes;
  const interfaces          = Components.interfaces;
  const nsILocalFile        = interfaces.nsILocalFile;
  const nsIFileOutputStream = interfaces.nsIFileOutputStream;
  const FILEOUT_CTRID       = '@mozilla.org/network/file-output-stream;1';
  
  var fileHandler = GetFileProtocolHandler();
  var localFile;
  if (href)
    localFile = fileHandler.getFileFromURLSpec(href).QueryInterface(nsILocalFile);
  else
    localFile = fileHandler.getFileFromURLSpec(sheet.href).QueryInterface(nsILocalFile);

  var fileOuputStream = classes[FILEOUT_CTRID].createInstance(nsIFileOutputStream);
  try {
    fileOuputStream.init(localFile, -1, -1, 0);

    var cssText = "/* Generated by CaScadeS, a stylesheet editor for Mozilla Composer */\n\n";
    var l = sheet.cssRules.length, tmp;
    var re = /;\s*/;
    var cssTextSplitted, ruleIndex, singleDeclIndex;
    
    for (ruleIndex = 0; ruleIndex< l; ruleIndex++) {
      tmp = sheet.cssRules.item(ruleIndex).cssText;
      cssTextSplitted = tmp.split(re);
      cssText += "  ";
      for (singleDeclIndex=0; singleDeclIndex<cssTextSplitted.length; singleDeclIndex++) {
        cssText += cssTextSplitted[singleDeclIndex];
        if (singleDeclIndex != cssTextSplitted.length-1)
          cssText += ";\n    ";
      }
      if (l > 1) cssText += "\n\n";
    }
    var textNode = document.createTextNode(cssText + "\n");
    fileOuputStream.write(cssText, cssText.length);
    fileOuputStream.close();
  }
  catch (ex) {}
}

// * if the checkbox of ID |checkboxID| is checked, copies the value |value|,
//   supposed to be the one for the top side, to the other three sides, makes
//   the XUL Element |elt| have that value. |propertyBase| and |propertySuffix|
//   are used to create side properties like in border-*-style.
//   The property and value are also added to the styles of the element of ID
//   |previewID| if it not null.
//   param String propertyBase
//   param String propertySuffix
//   param String value
//   param String checkboxID
//   param String previewID
//   param XULElement elt
function IfFourSidesSameStyle(propertyBase, propertySuffix, value, checkboxID, previewID, elt)
{
  // value is the value of the '*-top' property
  var checkbox = document.getElementById(checkboxID);
  if (!checkbox || !checkbox.checked) return;

  if (!value) {
    if (elt) {
      value = elt.value;
    }
  }
  var xulElt = document.getElementById(previewID);
  var sideArray = [ "left", "right", "bottom" ];
  for (var i=0; i<sideArray.length; i++) {
    var property = propertyBase + "-" + sideArray[i] + "-" + propertySuffix;
    if (xulElt) {
      AddStyleToElement(xulElt, property, value);
    }
    AddStyleToElement(gDialog.selectedObject, property, value);
  }
}

// * Callback when the user clicks on the "use same style for
//   the four borders" checkbox. |elt| is that checkbox.
//   param XULElement elt
function ToggleFourBorderSidesSameStyle(elt)
{
  var sameStyle = elt.checked;
  var sideArray = [ "left", "right", "bottom" ];

  var style = getSpecifiedStyle("border-top-style");
  if (!sameStyle && !style)
    style = "unspecified";
  var color = gDialog.topBorderColorInput.value;
  var width = getSpecifiedStyle("border-top-width");

  for (var i=0; i<sideArray.length; i++) {
    var styleMenulist = document.getElementById( sideArray[i] + "BorderStyleMenulist" );
    var widthInput    = document.getElementById( sideArray[i] + "BorderWidthInput" );
    var colorInput    = document.getElementById( sideArray[i] + "BorderColorInput" );
    var colorButton   = document.getElementById( sideArray[i] + "BorderColorButton" );
    if (sameStyle) {
      styleMenulist.selectedItem = document.getElementById("unspecified" + sideArray[i] + "BorderStyle");

      widthInput.value = "";

      colorInput.value = "";
      colorInput.setAttribute("disabled", "true");
      setColorWell("border"+sideArray[i]+"CW", "");

      widthInput.setAttribute("disabled", "true");
      styleMenulist.setAttribute("disabled", "true");
      colorButton.setAttribute("disabled", "true");

      AddStyleToElement(gDialog.borderPreview, "border-"+sideArray[i]+"-color", color);
      AddStyleToElement(gDialog.borderPreview, "border-"+sideArray[i]+"-style", style);
      AddStyleToElement(gDialog.borderPreview, "border-"+sideArray[i]+"-width", width);

      AddStyleToElement(gDialog.selectedObject, "border-"+sideArray[i]+"-color", color);
      AddStyleToElement(gDialog.selectedObject, "border-"+sideArray[i]+"-style", style);
      AddStyleToElement(gDialog.selectedObject, "border-"+sideArray[i]+"-width", width);
    }
    else {
      styleMenulist.removeAttribute("disabled");
      widthInput.removeAttribute("disabled");
      colorInput.removeAttribute("disabled");
      colorButton.removeAttribute("disabled");

      colorInput.value = color;
      setColorWell("border"+sideArray[i]+"CW", color);
      widthInput.value = width;
      var styleMenuitem = document.getElementById( style+sideArray[i]+"BorderStyle" );
      styleMenulist.selectedItem = styleMenuitem;
    }
  }
}  

// * Enable a XUL element depending on a boolean value
//   param XULElement elt
//   param boolean enabled
function EnableUI(elt, enabled)
{
  if (enabled) {
    elt.removeAttribute("disabled");
  }
  else {
    elt.setAttribute("disabled", "true");
  }
}

// * Initializes a menupopup containing length units. It looks for
//   the current value of the property and if it is already a length,
//   build the menu keeping the same numeric part and browsing all the
//   possible units.
function InitLengthUnitMenuPopup(elt, property, id, allowsPercentages)
{
  var value = elt.parentNode.value;
  var re = /([+-]?\d*\.\d+|[+-]?\d+)\D*/ ;
  var found = value.match(re);
  var numPart = "0";
  if (found && found.length != 0)
    numPart = RegExp.$1;

  var child = elt.firstChild, tmp;
  while (child && child.nodeName.toLowerCase() != "menuseparator") {
    tmp = child.nextSibling;
    elt.removeChild(child);
    child = tmp;
  }

  // below is an array of all valid CSS length units
  var unitsArray = [ "%", "px", "pt", "cm", "in", "mm", "pc", "em", "ex" ];
  var j, newitem, start = 0;
  if (!allowsPercentages)
    start = 1;
  for (j=start; j<unitsArray.length; j++) {
    var itemValue = numPart + unitsArray[j];
    newitem = document.createElementNS(XUL_NS, "menuitem");
    newitem.setAttribute("label", itemValue);
    newitem.setAttribute("value", itemValue);
    if (child)
      elt.insertBefore(newitem, child);
    else
      elt.appendChild(newitem);
  }
}

// * Removes all dashes from a string
//   param String s
//   return String
function TrimDashes(s)
{
  var r = "", i;
  for (i=0; i<s.length; i++)
    if (s[i] != '-')
      r += s[i];
  return r;
}

// * Callback for spinbuttons. Increments of |increment| the length value of
//   the XUL element of ID |id|.
//   param integer increment
//   param String id
function Spinbutton(increment, id)
{
  var elt = document.getElementById(id);
  if (elt) {
    var value = elt.value;
    var re = /([+-]?\d*\.\d+|[+-]?\d+)(\D*)/ ;
    var found = value.match(re);
    if (found && found.length != 0) {
      var numPart = RegExp.$1;
      var unitPart = RegExp.$2;
      var unitIncrement;
      switch (unitPart) {
        case "cm":
        case "in":
          unitIncrement = 0.01;
          break;
        default:
          unitIncrement = 1;
          break;
      }
      elt.value = (Math.round(100 * (parseFloat(numPart) + (increment * unitIncrement)))/100) + unitPart;
    }
    else
      elt.value = increment + "px";
    elt.oninput();
  }
}

// * code for initLocalFontFaceMenu shamelessly stolen from
//   Charley Manske <cmanske@netscape.com>
//   param XULElement menuPopup
function initLocalFontFaceMenu(menuPopup)
{
  if (!menuPopup) return;
  var menu = menuPopup.parentNode;
  if (!gLocalFonts)
  {
    // Build list of all local fonts once per editor
    try 
    {
      var enumerator = Components.classes["@mozilla.org/gfx/fontenumerator;1"].createInstance();
      if( enumerator )
        enumerator = enumerator.QueryInterface(Components.interfaces.nsIFontEnumerator);
      var localFontCount = { value: 0 }
      var localFontsString = enumerator.EnumerateAllFonts(localFontCount);
      if (localFontsString)
        gLocalFonts = localFontsString.toString().split(",");
      if (gLocalFonts)
      {
//        faces.sort(); Array is already sorted
        for( var i = 0; i < gLocalFonts.length; i++ )
        {
          if( gLocalFonts[i] != "" )
          {
            var itemNode = document.createElement("menuitem");
            itemNode.setAttribute("label", gLocalFonts[i]);
            //itemNode.value = gLocalFonts[i];
            itemNode.setAttribute("value", gLocalFonts[i]);
            menuPopup.appendChild(itemNode);
          }
        }
      }
    }
    catch(e) { }
  }
  //SetElementEnabledById("localFontFaceMenu", gLocalFonts != 0);
}

// * Handles a command in a menulist
//   param XULElement elt
//   param String property
//   param String id
function DoMenulistCommand(elt, property, id) {
  var choice = elt.selectedItem;
  var value = choice.value;
  onPullDownChanged(property, value, id);
  if (property == "border-top-style") {
    // this one is a special case : we need to check if the checkbox
    // "all sides have same style" is checked ; if it's checked, make
    // the other 3 sides acquire the style assigned to the top border
    IfFourSidesSameStyle('border', 'style', value,
                         'allFourBordersSame', id, null);
  }
}

