var insertNew = true;
var imageElement;
var tagName = "img"
var doSeeAll = true;
var wasEnableAll = false;
var hasAnyChanged = false;
var oldSourceInt = 0;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;
  dump("EditoreditorShell found for Image Properties dialog\n");

  // Create dialog object to store controls for easy access
  dialog = new Object;
  // This is the "combined" widget:
  dialog.srcInput = document.getElementById("image.srcInput");
  dialog.altTextInput = document.getElementById("image.altTextInput");

  dialog.MoreFewerButton = document.getElementById("MoreFewerButton");
  dialog.MoreRow = document.getElementById("MoreRow");

  dialog.customsizeRadio = document.getElementById( "customsizeRadio" );
  dialog.imagewidthInput = document.getElementById( "imagewidthInput" );
  dialog.imageheightInput = document.getElementById( "imageheightInput" );
  
  dialog.imagelrInput = document.getElementById( "imageleftrightInput" );
  dialog.imagetbInput = document.getElementById( "imagetopbottomInput" );
  dialog.imageborderInput = document.getElementById( "imageborderInput" );

  // Start in the mode initialized in the "doSeeAll" var above
  // THIS IS NOT WORKING NOW - After switching to "basic" mode,
  // then back to 
  if (doSeeAll) {
    dialog.MoreRow.style.visibility = "inherit"; // visible
  } else {
    dialog.MoreRow.style.visibility = "hidden"; // collapse
  }

  if (null == dialog.srcInput || 
      null == dialog.altTextInput )
  {
    dump("Not all dialog controls were found!!!\n");
  }
      
  initDialog();
  
  dialog.srcInput.focus();
}

function initDialog() {
  // Get a single selected anchor element
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
    dump("Element not selected - calling createElementWithDefaults\n");
    imageElement = editorShell.CreateElementWithDefaults(tagName);
	  if( !imageElement )
	  {
	    dump("Failed to get selected element or create a new one!\n");
	    window.close();
	  }  
  }
	
  // Set the controls to the image's attributes
  str = imageElement.getAttribute("src");
  if ( str == "null" )
  {
    str = "";
  }
  dialog.srcInput.value = str;
  
  str = imageElement.getAttribute("alt");
  if ( str == "null" )
  {
    str = "";
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
  dialog.imagewidthInput.value = dimvalue;
  
  dimvalue = imageElement.getAttribute("height");
  if ( dimvalue == "null" )
  {
    dimvalue = "";
  }
  dialog.imageheightInput.value = dimvalue;
  
  // this is not the correct way to determine if custom or original
  if ( dimvalue != "" )
  {
    dialog.customsizeRadio.checked = true;
  }

  alignpopup = document.getElementById("image.alignType");
  if ( alignpopup )
  {
    alignvalue = imageElement.getAttribute("align");
    if ( alignvalue == "" )
    {
      alignvalue = "at the bottom";
    }
    dump( "popup value = " + alignvalue + "\n" );
    alignpopup.setAttribute( "value", alignvalue );
  }

  // set spacing editfields
  sizevalue = imageElement.getAttribute("hspace");
  dialog.imagelrInput.value = sizevalue;
  
  sizevalue = imageElement.getAttribute("vspace");
  dialog.imagetbInput.value = sizevalue;
  
  sizevalue = imageElement.getAttribute("border");
  dialog.imageborderInput.value = sizevalue;    

  // force wasEnableAll to be different so everything gets updated
  wasEnableAll = !(dialog.srcInput.value.length > 0);
  doOverallEnabling();
}

function chooseFile()
{
  // Get a local file, converted into URL format
  fileName = editorShell.GetLocalFileURL(window, "img");
  if (fileName && fileName != "") {
    dialog.srcInput.value = fileName;
    doValueChanged();
  }
  // Put focus into the input field
  dialog.srcInput.focus();
}

function onMoreFewer()
{
  if (doSeeAll) {
    doSeeAll = false;
    // BUG: This works to hide the row, but
    //   setting visibility to "show" doesn't bring it back
    dialog.MoreRow.style.visibility = "collapse"; // hidden
  } else {
    doSeeAll = true;
    dialog.MoreRow.style.visibility = "visible"; // inherit
  }
}

function doValueChanged()
{
  if ( !hasAnyChanged )
  {
    hasAnyChanged = true;
    doOverallEnabling();
    hasAnyChanged = false;
  }
}

function SelectWidthUnits()
{
   list = document.getElementById("WidthUnits");
   value = list.options[list.selectedIndex].value;
   dump("Selected item: "+value+"\n");

   doValueChanged();
}

function OnChangeSrc()
{
  dump("OnChangeSrc ****************\n");
  doValueChanged();
}

function doDimensionEnabling( doEnable )
{
  SetLabelEnabledByID( "originalsizeLabel", doEnable );
  SetLabelEnabledByID( "customsizeLabel", doEnable );

	customradio = document.getElementById( "customsizeRadio" );
  if ( customradio )
  {
      // disable or enable custom setting controls
    SetElementEnabledByID( "imagewidthInput", doEnable && customradio.checked );
    SetElementEnabledByID( "widthunitSelect", doEnable && customradio.checked );
    SetElementEnabledByID( "imageheightInput", doEnable && customradio.checked );
    SetElementEnabledByID( "heightunitSelect", doEnable && customradio.checked );
    SetElementEnabledByID( "constrainCheckbox", doEnable && customradio.checked );

    SetLabelEnabledByID( "imagewidthLabel", doEnable && customradio.checked );
    SetLabelEnabledByID( "imageheightLabel", doEnable && customradio.checked );
    SetLabelEnabledByID( "constrainLabel", doEnable && customradio.checked );
  }
}

function doOverallEnabling()
{
  var canEnableAll;
  canEnableAll = (dialog.srcInput.value.length > 0);

  if ( wasEnableAll == canEnableAll )
    return;
  
  wasEnableAll = canEnableAll;

  btn = document.getElementById("OK");
  if ( btn )
  {
    btn.disabled = (!canEnableAll && hasAnyChanged);
  }
  
	fieldset = document.getElementById("imagedimensionsFieldset");
	if ( fieldset )
  {
    SetElementEnabledByID("imagedimensionsFieldset", canEnableAll );
    doDimensionEnabling( canEnableAll );
  }
  
  // handle altText and MoreFewer button
  SetLabelEnabledByID( "image.altTextLabel", canEnableAll );
  SetElementEnabledByID("image.altTextInput", canEnableAll );
  SetElementEnabledByID("MoreFewerButton", canEnableAll );
  SetElementEnabledByID("AdvancedButton", canEnableAll );

    // commented out since it asserts right now
  SetLabelEnabledByID( "imagealignmentLabel", canEnableAll );
  SetElementEnabledByID("image.alignType", canEnableAll );

    // spacing fieldset
  SetElementEnabledByID("spacing.fieldset", canEnableAll );
  SetElementEnabledByID("imageleftrightInput", canEnableAll );
  SetElementEnabledByID("imagetopbottomInput", canEnableAll );
  SetElementEnabledByID("imageborderInput", canEnableAll );

    // do spacing labels
    // commented out since they all assert right now
  SetLabelEnabledByID( "leftrightLabel", canEnableAll );
  SetLabelEnabledByID( "leftrighttypeLabel", canEnableAll );
  SetLabelEnabledByID( "topbottomLabel", canEnableAll );
  SetLabelEnabledByID( "topbottomtypeLabel", canEnableAll );
  SetLabelEnabledByID( "borderLabel", canEnableAll );
  SetLabelEnabledByID( "bordertypeLabel", canEnableAll );
}

function SetImageAlignment(align)
{
// do stuff

//  contentWindow.focus();
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
  checkbox = document.getElementById( "constrainCheckbox" );
  if ( !checkbox)
  	return;
  if ( !checkbox.checked )
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

function onOK()
{
  imageElement.setAttribute("src",dialog.srcInput.value);
  // We must convert to "file:///" format else image doesn't load!
  
  // TODO: we should confirm with user if no alt tag
  imageElement.setAttribute("alt", dialog.altTextInput.value);
  
  // set width if custom size and width is greater than 0
  if ( dialog.customsizeRadio.checked 
     && ( dialog.imagewidthInput.value.length > 0 )
     && ( dialog.imageheightInput.value.length > 0 ) )
  {
    imageElement.setAttribute( "width", dialog.imagewidthInput.value );
    imageElement.setAttribute( "height", dialog.imageheightInput.value );
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

  // handle insertion of new image
  if (insertNew) {
    dump("src="+imageElement.getAttribute("src")+" alt="+imageElement.getAttribute("alt")+"\n");
    // 'true' means delete the selection before inserting
    editorShell.InsertElement(imageElement, true);
  }

  // dismiss dialog
  window.close();
}
