/** EdAEHTMLAttributes.js
 *  - this file applies to the Editor Advanced Edit dialog box. 
 *  - contains functions for creating the HTML Attributes list
 **/

// build attribute list in tree form from element attributes
function BuildHTMLAttributeTable()
{
  var nodeMap = element.attributes;
  var i;
  if(nodeMap.length > 0) {
    for(i = 0; i < nodeMap.length; i++)
    {
      if ( !CheckAttributeNameSimilarity( nodeMap[i].nodeName, JSEAttrs ) ||
          IsEventHandler( nodeMap[i].nodeName ) ||
          TrimString( nodeMap[i].nodeName.toLowerCase() ) == "style" ) {
        continue;   // repeated or non-HTML attribute, ignore this one and go to next
      }
      var name  = nodeMap[i].nodeName.toLowerCase();
      var value = element.getAttribute ( nodeMap[i].nodeName );
      if (name.indexOf("_moz") != 0)
        AddTreeItem ( name, value, "HTMLAList", HTMLAttrs );
    }
  }
}

// add an attribute to the tree widget
function onAddHTMLAttribute(which)
{
  if(!which) 
    return;
  if( which.getAttribute ( "disabled" ) )
    return;

  var name  = dialog.AddHTMLAttributeNameInput.value;
  var value = TrimString(dialog.AddHTMLAttributeValueInput.value);
  if(name == "")
    return;

  if ( !CheckAttributeNameSimilarity( name, CSSAttrs ) )
    return;

  if ( AddTreeItem ( name, value, "HTMLAList", HTMLAttrs ) ) {
    dialog.AddHTMLAttributeNameInput.value = "";
    dialog.AddHTMLAttributeValueInput.value = "";
  }
  dialog.AddHTMLAttributeNameInput.focus();
}

// does enabling based on any user input.
function doHTMLEnabling( keycode )
{
dump("***doHTMLEnabling\n");
  if(keycode == 13) {
    onAddHTMLAttribute(document.getElementById("AddHTMLAttribute"));
    return;
  }
  var name = TrimString(dialog.AddHTMLAttributeNameInput.value).toLowerCase();
  if( name == "" || !CheckAttributeNameSimilarity(name,HTMLAttrs)) {
      dialog.AddHTMLAttribute.setAttribute("disabled","true");
  } else {
      dialog.AddHTMLAttribute.removeAttribute("disabled");
  }
}

// update the object with added and removed attributes
function UpdateHTMLAttributes()
{
  dump("===============[ Setting and Updating HTML ]===============\n");
  var HTMLAList = document.getElementById("HTMLAList");
  var name;
  var i;
  for( i = 0; i < HTMLAList.childNodes.length; i++)
  {
    var item = HTMLAList.childNodes[i];
    name = TrimString(item.firstChild.firstChild.getAttribute("value"));
    var value = TrimString(item.firstChild.lastChild.firstChild.value);
    // set the attribute
    element.setAttribute(name,value);
  }
  // remove removed attributes
  for( i = 0; i < HTMLRAttrs.length; i++ )
  {
    name = HTMLRAttrs[i];
    if(element.getAttribute(name))
      element.removeAttribute(name);
    else continue; // doesn't exist, so don't bother removing it.
  }
}