var msgCompSendFormat = Components.interfaces.nsIMsgCompSendFormat;
var param = null;

function Startup()
{
	if (window.arguments && window.arguments[0] && window.arguments[0])
  {
		param = window.arguments[0];
		param.abort = true;         //if the user hit the close box, we will abort.
		if (param.action)
		{
      // display the main text
      var messageParent = document.getElementById("info.box");
    	var messageText = messageParent.getAttribute("value");
      var htmlNode = document.createElement("html");
      var text = document.createTextNode(messageText);
      htmlNode.appendChild(text);
      messageParent.appendChild(htmlNode);

		  //Set the default radio array value
	    var group = document.getElementById("mailDefaultHTMLAction");
	    var element = document.getElementById("SendPlainTextAndHtml");
			group.selectedItem= element;
			group.data = element.data;

	    //change the button label
	    labels = document.getElementById("okCancelButtons");
	    element = document.getElementById("ok");
	    element.setAttribute("value", labels.getAttribute("button1Label"));
	    element = document.getElementById("cancel");
	    element.setAttribute("value", labels.getAttribute("button2Label"));
/*
	    element = document.getElementById("Button2");
	    element.setAttribute("value", labels.getAttribute("button3Label"));
      element.removeAttribute("hidden");
	    element.setAttribute("disabled", "true");
	    element = document.getElementById("Button3");
	    element.setAttribute("value", labels.getAttribute("button4Label"));
      element.removeAttribute("hidden");
	    element.setAttribute("disabled", "true");
*/		    
	    //set buttons action
	    doSetOKCancel(Send, DontSend, Recipients, Help);

      //At this point, we cannot position the window because it still doesn't have a width and a height.
      //Let move it out of the screen and then move it back at the right position, this after the first refresh.
      window.moveTo(32000, 32000);
		  setTimeout("moveToAlertPosition();", 0);
		}
	}
	else 
	{
		dump("error, no return object registered\n");
	}

}

function Send()
{
    if (param)
    {
		switch (document.getElementById("mailDefaultHTMLAction").data)
		{
			case "0": param.action = msgCompSendFormat.Both;		break;
			case "1": param.action = msgCompSendFormat.PlainText;	break;
			case "2": param.action = msgCompSendFormat.HTML;		break;
		}
        param.abort = false;
    }
    return true;
}

function DontSend()
{
    if (param)
        param.abort = true;
    return true;
}

function Recipients()
{
    return false;
}

function Help()
{
    return false;
}
