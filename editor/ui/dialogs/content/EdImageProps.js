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
 */

var insertNew                           = true;
var imageElement;
var tagName                             = "img"
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
    dialog.MoreRow.style.visibility     = "hidden"; // collapse
  }

  if (null == dialog.srcInput || 
      null == dialog.altTextInput )
  {
    dump("Not all dialog controls were found!!!\n");
  }
      
  // Get a single selected image element

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
  
  // Initialize all widgets with image attributes
  InitDialog();
  
  dialog.srcInput.focus();
}

function InitDialog() {

  // Set the controls to the image's attributes

  str                                   = imageElement.getAttribute("src");
  if ( str == "null" )
  {
    str                                 = "";
  }

  dialog.srcInput.value                 = str;
  
  str                                   = imageElement.getAttribute("alt");

  if ( str == "null" )
  {
    str                                 = "";
  }
  dialog.altTextInput.value = str;
  
  // we want to force an update so initialize "wasEnabledAll" to be the opposite of what the actual state is

  wasEnabledAll = !((dialog.srcInput.value.length > 0) && (dialog.altTextInput.value.length > 0));
  
  // set height and width
  // note: need to set actual image size if no attributes

  dimvalue = imageElement.getAttribute("width");

  if ( dimvalue == "null" )
  {
    dimvalue = "";
  }
  dialog.imagewidthInput.value          = dimvalue;
  
  dimvalue                              = imageElement.getAttribute("height");
  if ( dimvalue == "null" )
  {
    dimvalue                            = "";
  }
  dialog.imageheightInput.value         = dimvalue;
  
  // Mods Brian King XML Workshop
  // Set H & W pop-up on start-up
  if (insertNew == false)
  {
    
    var wdh                             = imageElement.getAttribute("width");
    var hgt                             = imageElement.getAttribute("height");
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

/********************* removed, some things are better said without words ******************

  if ( alignpopup )
  {
    alignvalue                          = imageElement.getAttribute("align");

    if ( alignvalue == "" )
    {
      alignvalue                        = "at the bottom";
    }

    dump( "popup value = " + alignvalue + "\n" );

    alignpopup.setAttribute( "value", alignvalue );

  }

********************* removed, some things are better said without words ******************/

  // set spacing editfields

  sizevalue                             = imageElement.getAttribute("hspace");
  dialog.imagelrInput.value             = sizevalue;
  
  sizevalue                             = imageElement.getAttribute("vspace");
  dialog.imagetbInput.value             = sizevalue;
  
  sizevalue                             = imageElement.getAttribute("border");
  dialog.imageborderInput.value         = sizevalue;    

  // force wasEnableAll to be different so everything gets updated

  wasEnableAll                          = !(dialog.srcInput.value.length > 0);
  doOverallEnabling();

  checkForImage( "image.srcInput" );
}

function chooseFile()
{
  // Get a local file, converted into URL format

  fileName                              = editorShell.GetLocalFileURL(window, "img");
  if (fileName && fileName != "") {
    dialog.srcInput.value               = fileName;
    checkForImage( "image.srcInput" );
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
    dialog.MoreRow.style.visibility     = "hidden"; // collapse is a little funky
  }
  else
  {
    doSeeAll                            = true;
    dialog.MoreRow.style.visibility     = "inherit"; // was visible; show doesn't seem to work
  }
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

  var canEnableAll;
  canEnableAll                          = imageType;

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

var imageType                           = false;

function checkForImage( elementID ){

  image                                 = document.getElementById( elementID ).value;

  if ( !image )
  return;
  
  var length                            = image.length;

  var tail                              = image.length - 4; 
  var type                              = image.substring(tail,length);

  if ( tail == 0 )  { 
  return; 
  }
  else  {

  switch( type )  {

    case ".gif":
    imageType    = type;
    break;

    case ".GIF":
    imageType    = type;
    break;

    case ".jpg":
    imageType    = type;
    break;

    case ".JPG":
    imageType    = type;
    break;

    case "JPEG":
    imageType    = type;
    break;

    case "jpeg":
    imageType    = type;
    break;

    case ".png":
    imageType    = type;
    break;

    case ".PNG":
    imageType    = type;
    break;

    default : imageType   = false;


      }

    }

  if( imageType ){ dump("Image is of type "+imageType+"\n\n"); }


return(imageType);

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

function onAdvancedEdit()
{
  dump("\n\n Need to write onAdvancedEdit for Image dialog\n\n");
}

function onOK()
{
  if ( !imageType )   {
  dump("alert *** please choose an image of typ gif, jpg or png.\n\n");
  return false;
  }

  imageElement.setAttribute("src",dialog.srcInput.value);

  // We must convert to "file:///" format else image doesn't load!
  
  // TODO: we should confirm with user if no alt tag

  imageElement.setAttribute("alt", dialog.altTextInput.value);
  
  // set width if custom size and width is greater than 0

  if ( dialog.customsizeRadio.checked 
     && ( dialog.imagewidthInput.value.length > 0 )
     && ( dialog.imageheightInput.value.length > 0 ) )
  {
    setDimensions();  // width and height
  }
  else
  {
    imageElement.removeAttribute( "width" );
    imageElement.removeAttribute( "height" );
  }
  
  // spacing attributes

  if ( dialog.imagelrInput.value.length > 0 )
    imageElement.setAttribute( "hspace", dialog.imagelrInput.value );
  else
    imageElement.removeAttribute( "hspace" );
  
  if ( dialog.imagetbInput.value.length > 0 )
    imageElement.setAttribute( "vspace", dialog.imagetbInput.value );
  else
    imageElement.removeAttribute( "vspace" );
  
  // note this is deprecated and should be converted to stylesheets

  if ( dialog.imageborderInput.value.length > 0 )
    imageElement.setAttribute( "border", dialog.imageborderInput.value );
  else
    imageElement.removeAttribute( "border" );
/*
  alignpopup = document.getElementById("image.alignType");
  if ( alignpopup )
  {
    alignpopup.getAttribute( "value", alignvalue );
    dump( "popup value = " + alignvalue + "\n" );
    if ( alignvalue == "at the bottom" )
      imageElement.removeAttribute("align");
    else
      imageElement.setAttribute("align", alignvalue );
  }
*/
  // handle insertion of new image

  if (insertNew)
  {
    // 'true' means delete the selection before inserting

    editorShell.InsertElement(imageElement, true);
  }

  return true;
}


// setDimensions()
// sets height and width attributes to inserted image
// Brian King - XML Workshop
// TODO: THIS NEEDS TO BE MODIFIED TO USE LOCALIZED STRING BUNDLE,
//  e.g., this assumes "% of" 
//  Use editorShell.GetString("name") to get a string and 
//  define those strings in editor\ui\dialogs\content\editor.properties
//  Note that using localized strings will break assumption about location of "% of"

function setDimensions()
{

  var wtype                             = dialog.imagewidthSelect.getAttribute("value");
  var htype                             = dialog.imageheightSelect.getAttribute("value");

    // width
    if (wtype.substr(0,4) == "% of")
    {
      //var Iwidth = eval("dialog.imagewidthInput.value + '%';");
      imageElement.setAttribute("width",  dialog.imagewidthInput.value + "%");
    }
    else
      imageElement.setAttribute("width", dialog.imagewidthInput.value);

    //height
    if (htype.substr(0,4) == "% of")
    {
      //var Iheight = eval("dialog.imageheightInput.value + '%';");
      imageElement.setAttribute("height", dialog.imageheightInput.value + "%");
    }
    else
      imageElement.setAttribute("height", dialog.imageheightInput.value);

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
