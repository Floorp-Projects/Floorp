var msgCompSendFormat = Components.interfaces.nsIMsgCompSendFormat;
var param = null;

function Startup()
{
	/* dump("Startup()\n"); */

	if (window.arguments && window.arguments[0] && window.arguments[0])
    {
		param = window.arguments[0];
		param.abort = true;         //if the user hit the close box, we will abort.
		if (param.action)
		{
		    //Set the default radio array value
		    var element = document.getElementById("SendPlainTextAndHtml");
		    element.checked = true;
		    
		    //change the button label
		    labels = document.getElementById("okCancelButtons");
		    element = document.getElementById("ok");
		    element.setAttribute("value", labels.getAttribute("button1Label"));
		    element = document.getElementById("cancel");
		    element.setAttribute("value", labels.getAttribute("button2Label"));
		    element = document.getElementById("Button2");
		    element.setAttribute("value", labels.getAttribute("button3Label"));
            element.removeAttribute("hidden");
		    element.setAttribute("disabled", "true");
		    element = document.getElementById("Button3");
		    element.setAttribute("value", labels.getAttribute("button4Label"));
            element.removeAttribute("hidden");
		    element.setAttribute("disabled", "true");
		    
		    //set buttons action
		    doSetOKCancel(Send, DontSend, Recipients, Help);

		   moveToAlertPosition();
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
        if (document.getElementById("SendPlainTextAndHtml").checked)
            param.action = msgCompSendFormat.Both;
        else if (document.getElementById("SendPlainTextOnly").checked)
            param.action = msgCompSendFormat.PlainText;
        else if (document.getElementById("SendHtmlOnly").checked)
            param.action = msgCompSendFormat.HTML;
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
