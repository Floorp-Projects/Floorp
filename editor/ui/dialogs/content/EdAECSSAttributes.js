
// build attribute list in tree form from element attributes
function BuildCSSAttributeTable()
{
  dump("populating CSS Attributes tree\n");
  dump("elStyle: " + element.getAttribute("style") + "\n");
  var style = element.getAttribute("style");
  if(style == undefined || style == "")
  {
    dump("no style attributes to add\n");
    return false;
  }
  if(style.indexOf(";") == -1) {
    if(style.indexOf(":") != -1) {
      name = TrimString(nvpairs.split(":")[0]);
      value = TrimString(nvpairs.split(":")[1]);
      if(!AddCSSAttribute(name,value)) {
        dump("Failed to add CSS attribute: " + i + "\n");
      }    
    } else
      return false;
  }
  nvpairs = style.split(";");
  for(i = 0; i < nvpairs.length; i++)
  {
    if(nvpairs[i].indexOf(":") != -1) {
      name = TrimString(nvpairs[i].split(":")[0]);
      value = TrimString(nvpairs[i].split(":")[1]);
      if(!AddCSSAttribute(name,value)) {
        dump("Failed to add CSS attribute: " + i + "\n");
      }    
    }
  }
}
  
function AddCSSAttribute(name,value)
{
  var treekids = document.getElementById("CSSAList");
  if(!CheckAttributeNameSimilarity(name, CSSAttrs)) {
    dump("repeated CSS attribute, ignoring\n");
    return false;
  }
  CSSAttrs[CSSAttrs.length] = name;
  
  var treeitem    = document.createElement("treeitem");
  var treerow     = document.createElement("treerow");
  var attrcell    = document.createElement("treecell");
  var attrcontent = document.createTextNode(name.toLowerCase());
  attrcell.appendChild(attrcontent);
  treerow.appendChild(attrcell);
  var valcell     = document.createElement("treecell");
  valcell.setAttribute("class","value");
  var valField    = document.createElement("html:input");
  var attrValue   = value;
  valField.setAttribute("type","text");
  valField.setAttribute("id",name.toLowerCase());
  valField.setAttribute("value",attrValue);
  valField.setAttribute("flex","100%");
  valField.setAttribute("class","AttributesCell");
  valcell.appendChild(valField);
  treerow.appendChild(valcell);
  treeitem.appendChild(treerow);
  treekids.appendChild(treeitem);
  return true;
}

// add an attribute to the tree widget
function onAddCSSAttribute()
{
  var name = dialog.AddCSSAttributeNameInput.value;
  var value = TrimString(dialog.AddCSSAttributeValueInput.value);

  if(name == "")
    return;

  // WHAT'S GOING ON? NAME ALWAYS HAS A VALUE OF accented "a"???
  dump(name+"= New Attribute Name - SHOULD BE EMPTY\n");

  if(AddCSSAttribute(name,value)) {
    dialog.AddCSSAttributeNameInput.value = "";
    dialog.AddCSSAttributeValueInput.value = "";
  } 
  dialog.AddCSSAttributeNameInput.focus();
}

// does enabling based on any user input.
function doCSSEnabling()
{
    var name = TrimString(dialog.AddCSSAttributeNameInput.value).toLowerCase();
    if( name == "" || !CheckAttributeNameSimilarity(name,CSSAttrs)) {
        dialog.AddCSSAttribute.setAttribute("disabled","true");
    } else {
        dialog.AddCSSAttribute.removeAttribute("disabled");
    }
}
