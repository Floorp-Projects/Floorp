function filterEditorOnLoad(pickerID)
{
	MsgFolderPickerOnLoad(pickerID);
}


function updateRule(boxnode) {
  
  var ruleid=boxnode.getAttribute("id");

  var selectNode = document.getElementById(ruleid + "-attribute");
  if (!selectNode) return;
  
  var option = selectNode.options[selectNode.selectedIndex];
  if (!option) return;
  if (!option.value) return;
  
  var destBox = document.getElementById(ruleid + "-verb");
  var destParent = destBox.parentNode;

  dump("Looking for " + option.value + "-template\n");
  var verbTemplate = document.getElementById(option.value + "-template");
  var newVerb = verbTemplate.cloneNode(true);
  
  dump("I will dump " + option.value + " into " + destParent.nodeName + "\n");

  destParent.replaceChild(newVerb, destBox);
}

function updateFilterObject(node) {
    
    selectbox=document.getElementById("rule1");
    updateRule(selectbox);
}
