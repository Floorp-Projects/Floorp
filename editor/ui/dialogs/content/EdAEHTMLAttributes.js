
// build attribute list in tree form from element attributes
function BuildHTMLAttributeTable()
{
  dump("NODENAME: " + element.nodeName + "\n");
  var nodeMap = element.attributes;
  var nodeMapCount = nodeMap.length;
  var treekids = document.getElementById("HTMLAList");

  if(nodeMapCount > 0) {
      for(i = 0; i < nodeMapCount; i++)
      {
        if(!CheckAttributeNameSimilarity(nodeMap[i].nodeName, JSEAttrs) ||
            IsEventHandler(nodeMap[i].nodeName) ||
            TrimString(nodeMap[i].nodeName.toLowerCase()) == "style") {
          dump("repeated property, JS event handler or style property!\n");
          continue;   // repeated attribute, ignore this one and go to next
        }
        HTMLAttrs[i] = nodeMap[i].nodeName;
        var treeitem    = document.createElement("treeitem");
        var treerow     = document.createElement("treerow");
        var attrcell    = document.createElement("treecell");
        var attrcontent = document.createTextNode(nodeMap[i].nodeName.toUpperCase());
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

function AddHTMLAttribute(name,value)
{
  HTMLAttrs[HTMLAttrs.length] = name;
  var treekids    = document.getElementById("HTMLAList");
  var treeitem    = document.createElement("treeitem");
  var treerow     = document.createElement("treerow");
  var attrcell    = document.createElement("treecell");
  var attrcontent = document.createTextNode(name.toUpperCase());
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
  return true;
}

// add an attribute to the tree widget
function onAddHTMLAttribute()
{
  var name = dialog.AddHTMLAttributeNameInput.value;
  var value = TrimString(dialog.AddHTMLAttributeValueInput.value);
  if(name == "")
    return;

  // WHAT'S GOING ON? NAME ALWAYS HAS A VALUE OF accented "a"???
  dump(name+"= New Attribute Name - SHOULD BE EMPTY\n");
  
  if(AddHTMLAttribute(name,value)) {
    dialog.AddHTMLAttributeNameInput.value = "";
    dialog.AddHTMLAttributeValueInput.value = "";
  }
  dialog.AddHTMLAttributeNameInput.focus();
}

// does enabling based on any user input.
function doHTMLEnabling()
{
    var name = TrimString(dialog.AddHTMLAttributeNameInput.value).toLowerCase();
    if( name == "" || !CheckAttributeNameSimilarity(name,HTMLAttrs)) {
        dialog.AddHTMLAttribute.setAttribute("disabled","true");
    } else {
        dialog.AddHTMLAttribute.removeAttribute("disabled");
    }
}
