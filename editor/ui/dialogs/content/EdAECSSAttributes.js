
// build attribute list in tree form from element attributes
function BuildCSSAttributeTable()
{
  // dump("populating CSS Attributes tree\n");
  // dump("  style=\"" + element.getAttribute("style") + "\"\n");

  // get the CSS declaration from DOM 2 ElementCSSInlineStyle
  var style = element.style;

  if(style == undefined)
  {
    dump("Inline styles undefined\n");
    return;
  }

  var l = style.length;
  if(l == undefined || l == 0)
  {
    if (l == undefined) {
      dump("Failed to query the number of inline style declarations\n");
    }
    return;
  }

  for (i = 0; i < l; i++) {
    name = style.item(i);
    value = style.getPropertyValue(name);
    // dump("  adding property '" + name + "' with value '" + value +"'\n");
    if ( !AddTreeItem( name, value, "CSSAList", CSSAttrs ) )
      dump("Failed to add CSS attribute: " + i + "\n");
  }
  return;
}

// add an attribute to the tree widget
function onAddCSSAttribute()
{
  var which = document.getElementById("AddCSSAttribute");
  if(!which || which.getAttribute ( "disabled" ) )
    return;

  var name = dialog.AddCSSAttributeNameInput.value;
  var value = TrimString(dialog.AddCSSAttributeValueInput.value);

  if(!name || !CheckAttributeNameSimilarity( name, CSSAttrs ) )
    return;

  if ( AddTreeItem ( name, value, "CSSAList", CSSAttrs ) ) {
    dialog.AddCSSAttributeNameInput.value = "";
    dialog.AddCSSAttributeValueInput.value = "";
  }
  dialog.AddCSSAttributeNameInput.focus();
  doCSSEnabling();
}

// does enabling based on any user input.
function doCSSEnabling()
{
  var name = TrimString(dialog.AddCSSAttributeNameInput.value).toLowerCase();
  if( !name || !CheckAttributeNameSimilarity(name,CSSAttrs))
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
    var name = TrimString(item.firstChild.firstChild.getAttribute("label"));
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

