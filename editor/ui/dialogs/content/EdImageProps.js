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

var insertNew                           = true;
var imageElement;
var doSeeAll                            = true;
var wasEnableAll                        = false;
var hasAnyChanged                       = false;
var oldSourceInt                        = 0;

// dialog initialization code

function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, null);

  // Create dialog object to store controls for easy access

  dialog                                = new Object;

  // This is the "combined" widget:

  dialog.srcInput                       = document.getElementById("image.srcInput");
  dialog.altTextInput                   = document.getElementById("image.altTextInput");

  dialog.MoreFewerButton                = document.getElementById("MoreFewerButton");
  dialog.MoreRow                        = document.getElementById("MoreRow");

  dialog.customsizeRadio                = document.getElementById( "customsizeRadio" );
  dialog.imagewidthInput                = document.getElementById( "imagewidthInput" );
  dialog.imageheightInput               = document.getElementById( "imageheightInput" );
  
  dialog.imagelrInput                   = document.getElementById( "imageleftrightInput" );
  dialog.imagetbInput                   = document.getElementById( "imagetopbottomInput" );
  dialog.imageborderInput               = document.getElementById( "imageborderInput" );

  // Start in the mode initialized in the "doSeeAll" var above
  // THIS IS NOT WORKING NOW - After switching to "basic" mode,
  // then back to 

  if (doSeeAll) {
    dialog.MoreRow.style.visibility     = "inherit"; // visible
  } else {
    dialog.MoreRow.style.visibility     = "collapse"; // use "hidden" if still too many problems
  }

  if (null == dialog.srcInput || 
      null == dialog.altTextInput )
  {
    dump("Not all dialog controls were found!!!\n");
  }
      
  // Get a single selected image element

  var tagName                           = "img"
  imageElement                          = editorShell.GetSelectedElement(tagName);

  if (imageElement) 
  {
    // We found an element and don't need to insert one

    insertNew                           = false;
  } 
  else
  {
    insertNew                           = true;

    // We don't have an element selected, 
    //  so create one with default attributes

    dump("Element not selected - calling createElementWithDefaults\n");
    imageElement                        = editorShell.CreateElementWithDefaults(tagName);
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
function InitDialog() {

  // Set the controls to the image's attributes

  str                                   = globalElement.getAttribute("src");
  if ( str == "null" )
  {
    str                                 = "";
  }

  dialog.srcInput.value                 = str;
  
  str                                   = globalElement.getAttribute("alt");

  if ( str == "null" )
  {
    str                                 = "";
  }
  dialog.altTextInput.value = str;
  
  // set height and width
  // note: need to set actual image size if no attributes

  dimvalue = globalElement.getAttribute("width");

  if ( dimvalue == "null" )
  {
    dimvalue = "";
  }
  dialog.imagewidthInput.value          = dimvalue;
  
  dimvalue                              = globalElement.getAttribute("height");
  if ( dimvalue == "null" )
  {
    dimvalue                            = "";
  }
  dialog.imageheightInput.value         = dimvalue;
  
  // Mods Brian King XML Workshop
  // Set H & W pop-up on start-up
  if (insertNew == false)
  {
    
    var wdh                             = globalElement.getAttribute("width");
    var hgt                             = globalElement.getAttribute("height");
    ispercentw                          = wdh.substr(wdh.length-1, 1);
    ispercenth                          = hgt.substr(hgt.length-1, 1);

  if (ispercentw == "%")
      setPopup("w");

    if (ispercenth == "%")
      setPopup("h");

  }
  // End Mods BK
  
  // this is not the correct way to determine if custom or original

  if ( dimvalue != "" )
  {
    dialog.customsizeRadio.checked      = true;
  }

  alignpopup                            = document.getElementById("image.alignType");

  // set spacing editfields

  sizevalue                             = globalElement.getAttribute("hspace");
  dialog.imagelrInput.value             = sizevalue;
  
  sizevalue                             = globalElement.getAttribute("vspace");
  dialog.imagetbInput.value             = sizevalue;
  
  sizevalue                             = globalElement.getAttribute("border");
  dialog.imageborderInput.value         = sizevalue;    

  
  // we want to force an update so initialize "wasEnableAll" to be the opposite of what the actual state is
  imageTypeExtension                    = checkForImage();
  wasEnableAll                          = !imageTypeExtension;
  doOverallEnabling();
}

function chooseFile()
{
  // Get a local file, converted into URL format

  fileName                              = editorShell.GetLocalFileURL(window, "img");
  if (fileName && fileName != "") {
    dialog.srcInput.value               = fileName;
//  imageTypeExtension                  = checkForImage();
    doValueChanged();
  }

  // Put focus into the input field

  dialog.srcInput.focus();
}

function onMoreFewer()
{
  if (doSeeAll)
  {
    void(null);    
    doSeeAll                            = false;
    dialog.MoreRow.style.visibility     = "collapse"; // use "hidden" if still too many problems
  }
  else
  {
    doSeeAll                            = true;
    dialog.MoreRow.style.visibility     = "inherit"; // was visible; show doesn't seem to work
  }
  // When visibility = "collapse" works,
  // Use this to resize dialog:
  window.sizeToContent();
}

function doValueChanged()
{

  if ( !hasAnyChanged )
  {
    hasAnyChanged                       = true;
    doOverallEnabling();
    hasAnyChanged                       = false;
  }
}

function SelectWidthUnits()
{
   list                                 = document.getElementById("WidthUnits");
   value                                = list.options[list.selectedIndex].value;

   dump("Selected item: "+value+"\n");

   doValueChanged();
}

function OnChangeSrc()
{
  doValueChanged();
}

function doDimensionEnabling( doEnable )
{

  SetClassEnabledByID( "originalsizeLabel", doEnable );
  SetClassEnabledByID( "customsizeLabel", doEnable );

  SetClassEnabledByID( "dimensionsLegend", doEnable );
  SetClassEnabledByID( "spacingLegend", doEnable );

  SetClassEnabledByID( "AdvancedButton", doEnable );
  SetClassEnabledByID( "MoreFewerButton", doEnable );


  customradio                           = document.getElementById( "customsizeRadio" );


  if ( customradio && customradio.checked )
  {
      // disable or enable custom setting controls

    SetElementEnabledByID( "imagewidthInput", doEnable && customradio.checked );

////////////////// this is currently the only way i can get it to work /////////////////

    // SetElementEnabledByID( "widthunitSelect", doEnable && customradio.checked );

    element                           = document.getElementById( "widthunitSelect" );
    element.setAttribute( "disabled", false );

    SetElementEnabledByID( "imageheightInput", doEnable && customradio.checked );

    element                           = document.getElementById( "heightunitSelect" );
    element.setAttribute( "disabled", false );

    //SetElementEnabledByID( "heightunitSelect", doEnable && customradio.checked );

/////////////////////////////////////////////////////////////////////////////////////////

    SetElementEnabledByID( "constrainCheckbox", doEnable && customradio.checked );

    SetClassEnabledByID( "imagewidthLabel", doEnable && customradio.checked );
    SetClassEnabledByID( "imageheightLabel", doEnable && customradio.checked );
    SetClassEnabledByID( "constrainLabel", doEnable && customradio.checked );
  }

}

function doOverallEnabling()
{

  var imageTypeExtension                = checkForImage();
  var canEnableAll;
  canEnableAll                          = imageTypeExtension;

  if ( wasEnableAll == canEnableAll )
    return;
  
  wasEnableAll                          = canEnableAll;

  btn                                   = document.getElementById("ok");
  if ( btn )
  {
    dump("ok button found!\n")
    btn.disabled                        = (!canEnableAll && hasAnyChanged);
  }
  else dump("ok button not found\n");
  
  fieldset                              = document.getElementById("imagedimensionsFieldset");
  if ( fieldset )
  {
    SetElementEnabledByID("imagedimensionsFieldset", canEnableAll );
    doDimensionEnabling( canEnableAll );
  }
  
  // handle altText and MoreFewer button

  SetClassEnabledByID( "image.altTextLabel", canEnableAll );
  SetElementEnabledByID("image.altTextInput", canEnableAll );
  SetElementEnabledByID("MoreFewerButton", canEnableAll );
  SetElementEnabledByID("AdvancedButton", canEnableAll );

  // alignment

  SetClassEnabledByID( "imagealignmentLabel", canEnableAll );
  SetElementEnabledByID("image.alignType", canEnableAll );

/* this shouldn't be needed here; doDimensionEnabling should do this */
  customradio                           = document.getElementById( "customsizeRadio" );
  if(customradio.checked){
  SetElementEnabledByID("heightunitSelect", canEnableAll );
  SetElementEnabledByID("widthunitSelect", canEnableAll );
                        }

    // spacing fieldset

  SetElementEnabledByID("spacing.fieldset", canEnableAll );
  SetElementEnabledByID("imageleftrightInput", canEnableAll );
  SetElementEnabledByID("imagetopbottomInput", canEnableAll );
  SetElementEnabledByID("imageborderInput", canEnableAll );

    // do spacing labels

  SetClassEnabledByID( "leftrightLabel", canEnableAll );
  SetClassEnabledByID( "leftrighttypeLabel", canEnableAll );
  SetClassEnabledByID( "topbottomLabel", canEnableAll );
  SetClassEnabledByID( "topbottomtypeLabel", canEnableAll );
  SetClassEnabledByID( "borderLabel", canEnableAll );
  SetClassEnabledByID( "bordertypeLabel", canEnableAll );
}

function SetImageAlignment(align)
{
// do stuff

//  contentWindow.focus();
}

// an API to validate and image by sniffing out the extension
/* assumes that the element id is "image.srcInput" */
/* returns lower-case extension or 0 */
function checkForImage() {

  image                                 = document.getElementById( "image.srcInput" ).value;

  if ( !image )
  return 0;
  
  /* look for an extension */
  var tailindex                         = image.lastIndexOf("."); 
  if ( tailindex == 0 )
  return 0; 
  
  /* move past period, get the substring from the first character after the '.' to the last character (length) */
  tail = tail + 1;
  var type                              = image.substring(tail,image.length);
  
  /* convert extension to lower case */
  if (type)
    type = type.toLowerCase();

  switch( type )  {

    case "gif":
    case "jpg":
    case "jpeg":
    case "png":
              return type;
    break;

    default : return 0;

    }

}


// constrainProportions contribution by pete@postpagan.com
// needs to handle pixels/percent

function constrainProportions( srcID, destID )
{
  srcElement                            = document.getElementById ( srcID );
  if ( !srcElement )
    return;
  
  forceInteger( srcID );
  
  // now find out if we should be constraining or not

  checkbox                              = document.getElementById( "constrainCheckbox" );
  if ( !checkbox)
    return;
  if ( !checkbox.checked )
    return;
  
  destElement                           = document.getElementById( destID );
  if ( !destElement )
    return;
  
  // set new value in the other edit field
  // src / dest ratio mantained
  // newDest = (newSrc * oldDest / oldSrc)

  if ( oldSourceInt == 0 )
    destElement.value                   = srcElement.value;
  else
    destElement.value                   = Math.round( srcElement.value * destElement.value / oldSourceInt );
  
  oldSourceInt                          = srcElement.value;
}

// Get data from widgets, validate, and set for the global element
//   accessible to AdvancedEdit() [in EdDialogCommon.js]
function ValidateData()
{
  var imageTypeExtension                = checkForImage();
  if ( !imageTypeExtension ) {
    ShowInputErrorMessage(GetString("MissingImageError"));
    return false;
  }

  //TODO: WE NEED TO DO SOME URL VALIDATION HERE, E.G.:
  // We must convert to "file:///" or "http://" format else image doesn't load!
  globalElement.setAttribute("src",dialog.srcInput.value);
  
  // TODO: Should we confirm with user if no alt tag? Or just set to empty string?
  globalElement.setAttribute("alt", dialog.altTextInput.value);
  
  // set width if custom size and width is greater than 0
  // Note: This accepts and empty string as meaning "don't set
  // BUT IT ALSO ACCEPTS 0. Should use ValidateNumberString() to tell user proper range
  if ( dialog.customsizeRadio.checked 
     && ( dialog.imagewidthInput.value.length > 0 )
     && ( dialog.imageheightInput.value.length > 0 ) )
  {
    setDimensions();  // width and height
  }
  else
  {
    //TODO: WE SHOULD ALWAYS SET WIDTH AND HEIGHT FOR FASTER IMAGE LAYOUT
    //  IF USER DOESN'T SET IT, WE NEED TO GET VALUE FROM ORIGINAL IMAGE 
    globalElement.removeAttribute( "width" );
    globalElement.removeAttribute( "height" );
  }
  
  // spacing attributes
  // All of these should use ValidateNumberString() to 
  //  ensure value is within acceptable range
  if ( dialog.imagelrInput.value.length > 0 )
    globalElement.setAttribute( "hspace", dialog.imagelrInput.value );
  else
    globalElement.removeAttribute( "hspace" );
  
  if ( dialog.imagetbInput.value.length > 0 )
    globalElement.setAttribute( "vspace", dialog.imagetbInput.value );
  else
    globalElement.removeAttribute( "vspace" );
  
  // note this is deprecated and should be converted to stylesheets

  if ( dialog.imageborderInput.value.length > 0 )
    globalElement.setAttribute( "border", dialog.imageborderInput.value );
  else
    globalElement.removeAttribute( "border" );

// This currently triggers a "Not implemented" assertion, preventing inserting an image
// TODO: FIX THIS!
/*
  alignpopup = document.getElementById("image.alignType");
  if ( alignpopup )
  {
    var alignurl;
    alignpopup.getAttribute( "src", alignurl );
    dump( "popup value = " + alignurl + "\n" );
    if ( alignurl == "&bottomIcon.url;" )
      globalElement.removeAttribute("align");
//    else
//      globalElement.setAttribute("align", alignurl );
  }
*/
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
      // 'true' means delete the selection before inserting
      editorShell.InsertElement(imageElement, true);
    }
    return true;
  }
  return false;
}


// setDimensions()
// sets height and width attributes to inserted image
// Brian King - XML Workshop
function setDimensions()
{

  var wtype                             = dialog.imagewidthSelect.getAttribute("value");
  var htype                             = dialog.imageheightSelect.getAttribute("value");

    // width
    // NO! this is not the way to do it! Depends on english strings
    //  Instead, store which index is selected when popup "pixel" or "percent of..." is used
    if (wtype.substr(0,4) == "% of")
    {
      //var Iwidth = eval("dialog.imagewidthInput.value + '%';");
      globalElement.setAttribute("width",  dialog.imagewidthInput.value + "%");
    }
    else
      globalElement.setAttribute("width", dialog.imagewidthInput.value);

    //height
    if (htype.substr(0,4) == "% of")
    {
      //var Iheight = eval("dialog.imageheightInput.value + '%';");
      globalElement.setAttribute("height", dialog.imageheightInput.value + "%");
    }
    else
      globalElement.setAttribute("height", dialog.imageheightInput.value);

}

// setPopup()
// sets height and width popups on during initialisation
// Brian King - XML Workshop

function setPopup(dim)
{

    select                              = getContainer();
    if (select.nodeName == "TD")
    { 
      if (dim == "w")
        dialog.imagewidthSelect.setAttribute("value", "% of cell");
      else
        dialog.imageheightSelect.setAttribute("value", "% of cell");
    }
    else
    {
      if (dim == "w")
        dialog.imagewidthSelect.setAttribute("value", "% of window");
      else
        dialog.imageheightSelect.setAttribute("value", "% of window");
    }

}

// This function moves the selected item into view

function popupSelectedImage( item, elementID, node ){

selectedItem                            = document.getElementById(elementID);
selectedItem.setAttribute(node, item);


}

function SetPixelOrPercentByID(fu, bar){

dump("comming soon . . . SetPixelOrPercentByID\n\n");




}
