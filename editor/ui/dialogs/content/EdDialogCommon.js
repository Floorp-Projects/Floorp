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

// this function takes an elementID and a flag
// if the element can be found by ID, then it is either enabled (by removing "disabled" attr)
// or disabled (setAttribute) as specified in the "doEnable" parameter
function SetElementEnabledByID( elementID, doEnable )
{
  element = document.getElementById(elementID);
  if ( element )
  {
    if ( doEnable )
    {
      element.removeAttribute( "disabled" );
    }
    else
    {
      element.setAttribute( "disabled", "true" );
    }
  }
}

// this function takes an ID for a label and a flag
// if an element can be found by its ID, then it is either enabled or disabled
// The class is set to either "enabled" or "disabled" depending on the flag passed in.
// This function relies on css having a special appearance for these two classes.
function SetLabelEnabledByID( labelID, doEnable )
{
  label = document.getElementById(labelID);
  if ( label )
  {
    if ( doEnable )
    {
     label.setAttribute( "class", "enabled" );
    }
    else
    {
     label.setAttribute( "class", "disabled" );
    }
  }
  else
  {
    dump( "not changing label"+labelID+"\n" );
  }
}

// All dialogs share this simple method
function onCancel()
{
  window.close();
}
