// build attribute list in tree form from element attributes
function BuildJSEAttributeTable()
{
  dump("Loading event handlers list...\n");
  var nodeMap = element.attributes;
  var nodeMapCount = nodeMap.length;
  var treekids = document.getElementById("JSEAList");

  dump("nmc: " + nodeMapCount + "\n");
  if(nodeMapCount > 0) {
      for(var i = 0; i < nodeMapCount; i++)
      {
        if(!CheckAttributeNameSimilarity(nodeMap[i].nodeName, JSEAttrs)) {
          dump("repeated JS handler!\n");
          continue;   // repeated attribute, ignore this one and go to next
        }
        if(!IsEventHandler(nodeMap[i].nodeName)) {
          dump("not an event handler\n");
          continue; // attribute isn't an event handler.
        }
        JSEAttrs[i] = nodeMap[i].nodeName;
        var treeitem    = document.createElement("treeitem");
        var treerow     = document.createElement("treerow");
        var attrcell    = document.createElement("treecell");
        var attrcontent = document.createTextNode(nodeMap[i].nodeName.toLowerCase());
        attrcell.appendChild(attrcontent);
        treerow.appendChild(attrcell);
        var valcell     = document.createElement("treecell");
        valcell.setAttribute("class","value");
        var valField    = document.createElement("html:input");
        var attrValue   = element.getAttribute(nodeMap[i].nodeName);
        valField.setAttribute("type","text");
        valField.setAttribute("id",nodeMap[i].nodeName.toLowerCase());
        valField.setAttribute("value",attrValue);
        valField.setAttribute("flex","100%");
        valField.setAttribute("class","AttributesCell");
        valcell.appendChild(valField);
        treerow.appendChild(valcell);
        treeitem.appendChild(treerow);
        treekids.appendChild(treeitem);
      }
  }
}

function IsEventHandler(which)
{
  var handlerName = which.toLowerCase();
  var firstTwo = handlerName.substring(0,2);
  if(firstTwo == "on")
    return true;
  else
    return false;
}

function AddJSEAttribute(name,value)
{
  JSEAttrs[JSEAttrs.length] = name;
  var treekids    = document.getElementById("JSEAList");
  var treeitem    = document.createElement("treeitem");
  var treerow     = document.createElement("treerow");
  var attrcell    = document.createElement("treecell");
  var attrcontent = document.createTextNode(name.toLowerCase());
  attrcell.appendChild(attrcontent);
  treerow.appendChild(attrcell);
  var valcell     = document.createElement("treecell");
  valcell.setAttribute("class","value");
  var valField    = document.createElement("html:input");
  valField.setAttribute("type","text");
  valField.setAttribute("id",name);
  valField.setAttribute("value",value);
  valField.setAttribute("flex","100%");
  valField.setAttribute("class","AttributesCell");
  valcell.appendChild(valField);
  treerow.appendChild(valcell);
  treeitem.appendChild(treerow);
  treekids.appendChild(treeitem);
}

// add an attribute to the tree widget
function onAddJSEAttribute()
{
  var name = dialog.AddJSEAttributeNameInput.value;
  var value = TrimString(dialog.AddJSEAttributeValueInput.value);
  if(name == "")
    return;
  // WHAT'S GOING ON? NAME ALWAYS HAS A VALUE OF accented "a"???
  dump(name+"= New Attribute Name - SHOULD BE EMPTY\n");
  if(AddJSEAttribute(name,value)) {
    dialog.AddJSEAttributeNameInput.value = "";
    dialog.AddJSEAttributeValueInput.value = "";
  }
  dialog.AddJSEAttributeNameInput.focus();
}

// does enabling based on any user input.
function doJSEEnabling()
{
    var name = TrimString(dialog.AddJSEAttributeNameInput.value).toLowerCase();
    if( name == "" || !CheckAttributeNameSimilarity(name,JSEAttrs)) {
        dialog.AddJSEAttribute.setAttribute("disabled","true");
    } else {
        dialog.AddJSEAttribute.removeAttribute("disabled");
    }
}
