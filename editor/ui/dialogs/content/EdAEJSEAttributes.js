
// build attribute list in tree form from element attributes
function BuildJSEAttributeTable()
{
  var nodeMap = element.attributes;
  if(nodeMap.length > 0) {
    for(var i = 0; i < nodeMap.length; i++)
    {
      if( !CheckAttributeNameSimilarity( nodeMap[i].nodeName, JSEAttrs ) )
        continue;   // repeated or non-JS handler, ignore this one and go to next
      if( !IsEventHandler( nodeMap[i].nodeName ) )
        continue; // attribute isn't an event handler.
      var name  = nodeMap[i].nodeName.toLowerCase();
      var value = element.getAttribute(nodeMap[i].nodeName);
      AddTreeItem( name, value, "JSEATree", JSEAttrs ); // add item to tree
    }
  }
}

// check to see if given string is an event handler.
function IsEventHandler( which )
{
  var handlerName = which.toLowerCase();
  var firstTwo = handlerName.substring(0,2);
  if(firstTwo == "on")
    return true;
  else
    return false;
}

// add an attribute to the tree widget
function onAddJSEAttribute( which )
{
  if( !which ) 
    return;
  if( which.getAttribute ( "disabled" ) )
    return;

  var name = dialog.AddJSEAttributeNameInput.value;
  var value = TrimString(dialog.AddJSEAttributeValueInput.value);
  if(name == "")
    return;
  if ( name.substring(0,2).toLowerCase() != "on" )
    name = "on" + name;   // user has entered event without "on" prefix, add it

  if ( AddTreeItem( name, value, "JSEAList", JSEAttrs ) ) {
    dialog.AddJSEAttributeNameInput.value = "";
    dialog.AddJSEAttributeValueInput.value = "";
  }
  dialog.AddJSEAttributeNameInput.focus();
}

// does enabling based on any user input.
function doJSEEnabling( keycode )
{
  if(keycode == 13) {
    onAddJSEAttribute( document.getElementById ( "AddJSEAttribute" ) );
    return;
  }
  var name = TrimString(dialog.AddJSEAttributeNameInput.value).toLowerCase();

  if ( name.substring(0,2) != "on" )
    name = "on" + name;   // user has entered event without "on" prefix, add it
    
  if( name == "" || !CheckAttributeNameSimilarity(name, JSEAttrs))
    dialog.AddJSEAttribute.setAttribute("disabled","true");
  else
    dialog.AddJSEAttribute.removeAttribute("disabled");
}

function UpdateJSEAttributes()
{
  dump("===============[ Setting and Updating JSE ]===============\n");
  var JSEAList = document.getElementById("JSEAList");
  var i;
  for( i = 0; i < JSEAList.childNodes.length; i++)
  {
    var item = JSEAList.childNodes[i];
    name = TrimString(item.firstChild.firstChild.getAttribute("value"));
    value = TrimString(item.firstChild.lastChild.firstChild.value);
    // set the event handler
    element.setAttribute(name,value);
  }
  // remove removed attributes
  for( i = 0; i < JSERAttrs.length; i++ )
  {
    var name = JSERAttrs[i];
    if(element.getAttribute(name))
      element.removeAttribute(name);
    else continue; // doesn't exist, so don't bother removing it.
  }
}