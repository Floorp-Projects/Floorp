
// build attribute list in tree form from element attributes
function BuildCSSAttributeTable()
{
  dump("populating CSS Attributes tree\n");
  dump("elStyle: " + element.getAttribute("style") + "\n");
  var style = element.getAttribute("style");
  if(style == undefined || style == "")
  {
    dump("no style attributes to add\n");
    return;
  }
  if(style.indexOf(";") == -1) {
    if(style.indexOf(":") != -1) {
      name = TrimString(nvpairs.split(":")[0]);
      value = TrimString(nvpairs.split(":")[1]);
      if ( !AddTreeItem( name, value, "CSSATree", CSSAttrs ) )
        dump("Failed to add CSS attribute: " + i + "\n");
    } else
      return;
  }
  nvpairs = style.split(";");
  for(i = 0; i < nvpairs.length; i++)
  {
    if(nvpairs[i].indexOf(":") != -1) {
      name = TrimString(nvpairs[i].split(":")[0]);
      value = TrimString(nvpairs[i].split(":")[1]);
      if( !AddTreeItem( name, value, "CSSATree", CSSAttrs ) )
        dump("Failed to add CSS attribute: " + i + "\n");
    }
  }
}
  
// add an attribute to the tree widget
function onAddCSSAttribute( which )
{
  if( !which ) 
    return;
  if( which.getAttribute ( "disabled" ) )
    return;

  var name = dialog.AddCSSAttributeNameInput.value;
  var value = TrimString(dialog.AddCSSAttributeValueInput.value);

  if(name == "")
    return;

  if ( !CheckAttributeNameSimilarity( name, CSSAttrs ) )
    return;

  if ( AddTreeItem ( name, value, "CSSAList", CSSAttrs ) ) {
    dialog.AddCSSAttributeNameInput.value = "";
    dialog.AddCSSAttributeValueInput.value = "";
  } 
  dialog.AddCSSAttributeNameInput.focus();
}

// does enabling based on any user input.
function doCSSEnabling( keycode )
{
  if(keycode == 13) {
    onAddCSSAttribute( document.getElementById ( "AddCSSAttribute" ) );
    return;
  }
  var name = TrimString(dialog.AddCSSAttributeNameInput.value).toLowerCase();
  if( name == "" || !CheckAttributeNameSimilarity(name,CSSAttrs))
    dialog.AddCSSAttribute.setAttribute("disabled","true");
  else
    dialog.AddCSSAttribute.removeAttribute("disabled");
}

function UpdateCSSAttributes()
{
  dump("===============[ Setting and Updating STYLE ]===============\n");
  var CSSAList = document.getElementById("CSSAList");
  var styleString = "";
  for(var i = 0; i < CSSAList.childNodes.length; i++)
  {
    var item = CSSAList.childNodes[i];
    var name = TrimString(item.firstChild.firstChild.getAttribute("value"));
    var value = TrimString(item.firstChild.lastChild.firstChild.value);
    // this code allows users to be sloppy in typing in values, and enter
    // things like "foo: " and "bar;". This will trim off everything after the 
    // respective character. 
    if(name.indexOf(":") != -1)
      name = name.substring(0,name.lastIndexOf(":"));
    if(value.indexOf(";") != -1)
      value = value.substring(0,value.lastIndexOf(";"));
    if(i == (CSSAList.childNodes.length - 1))
      styleString += name + ": " + value + ";";   // last property
    else
      styleString += name + ": " + value + "; ";
  }
  dump("stylestring: ||" + styleString + "||\n");
  if(styleString.length > 0) {
    element.removeAttribute("style");
    element.setAttribute("style",styleString);  // NOTE BUG 18894!!!
  } else {
    if(element.getAttribute("style"))
      element.removeAttribute("style");
  }
}

