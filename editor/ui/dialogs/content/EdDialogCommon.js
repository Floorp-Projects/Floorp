function ClearList(list)
{
  for( i = (list.length-1); i >= 0; i-- ) {
    list.remove(i);
  }
}

function AppendStringToList(list, string)
{
  
  // THIS DOESN'T WORK! Result is a XULElement -- namespace problem
  //optionNode1 = document.createElement("option");
  // "Unsanctioned method from Vidur:
  // createElementWithNamespace("http://... [the HTML4 URL], "option);

  // This works - Thanks to Vidur! Params = name, value
  optionNode = new Option(string, string);
  if (optionNode) {
    list.add(optionNode, null);    
  } else {
    dump("Failed to create OPTION node. String content="+string+"\n");
  }
}

// "value" may be a number or string type
function ValidateNumberString(value, minValue, maxValue)
{
  // Get the number version
  number = value - 0;
  if ((value+"") != "") {
    if (number && number >= minValue && number <= maxValue ){
      // Return string version of the number
      return number + "";
    }
  }
  // Return an empty string to indicate error
  //TODO: Popup a message box telling the user about the error
  return "";
}

// All dialogs share this simple method
function onCancel()
{
  window.close();
}
