/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
var msgCompSendFormat = Components.interfaces.nsIMsgCompSendFormat;
var msgCompConvertible = Components.interfaces.nsIMsgCompConvertible;
var changeDefault = false;  /* Set default selection following
                               the recommendation. Some people think, the
                               default should *always* be the same. */
var param = null;

function Startup()
{
	if (window.arguments && window.arguments[0])
    {
        var defaultElement = document.getElementById("SendPlainTextOnly");
             // used only if changeDefault == false
             // maybe make that a (default) pref

		param = window.arguments[0];
		param.abort = true;    //if the user hit the close box, we will abort.
		if (param.action)
		{
		    // Set the question label
            var labeldeck = document.getElementById("mailSendFormatExplanation");
            var icon = document.getElementById("convertDefault");
            switch (param.convertible)
		    {
            case msgCompConvertible.Plain:
                // We shouldn't be here at all
                labeldeck.setAttribute("index", 1);
                // No icon
                break;
            case msgCompConvertible.Yes:
                labeldeck.setAttribute("index", 1);
                icon.setAttribute("id", "convertYes");
                break;
            case msgCompConvertible.Altering:
                labeldeck.setAttribute("index", 2);
                icon.setAttribute("id", "convertAltering");
                break;
            case msgCompConvertible.No:
                labeldeck.setAttribute("index", 3);
                icon.setAttribute("id", "convertNo");
                break;
		    }

		    // Set the default radio array value and recommendation
            var group = document.getElementById("mailDefaultHTMLAction");
            var element;
            var recommlabels = document.getElementById("hiddenLabels");
            var label;
            var setrecomm = false;
            switch (param.action)
		    {
            case msgCompSendFormat.AskUser:
                //setrecomm = false;
                break;
            case msgCompSendFormat.PlainText:
                element = document.getElementById("SendPlainTextOnly");
                //label = recommlabels.getAttribute("plainTextOnlyRecommendedLabel");
                label = document.getElementById("plainTextOnlyRecommended");
                     // elements for "recommended" are a workaround for bug 49623
                setrecomm = true;
                break;
            case msgCompSendFormat.Both:
                element = document.getElementById("SendPlainTextAndHtml");
                //label = recommlabels.getAttribute("plainTextAndHtmlRecommendedLabel");
                label = document.getElementById("plainTextAndHtmlRecommended");
                setrecomm = true;
                break;
			case msgCompSendFormat.HTML:
                element = document.getElementById("SendHtmlOnly");
                //label = recommlabels.getAttribute("htmlOnlyRecommendedLabel");
                label = document.getElementById("htmlOnlyRecommended");
                setrecomm = true;
                break;
		    }
            if (setrecomm)
            {
                /*
                dump(element.getAttribute("value"));
                element.setAttribute("value", label);
                element.setAttribute("value", "foo");
                dump(element.getAttribute("value"));
                */
                label.removeAttribute("hidden");
                if (changeDefault)
                {
                    group.selectedItem = element;
                    group.data = element.data;
                }
            }
            if (!changeDefault)
            {
                group.selectedItem = defaultElement;
                group.data = defaultElement.data;
            }

		    //change the button label
		    var buttonlabels = document.getElementById("okCancelButtons");
		    element = document.getElementById("ok");
		    element.setAttribute("value", buttonlabels.getAttribute("button1Label"));
		    element = document.getElementById("cancel");
		    element.setAttribute("value", buttonlabels.getAttribute("button2Label"));
/*
		    element = document.getElementById("Button2");
		    element.setAttribute("value", buttonlabels.getAttribute("button3Label"));
            element.removeAttribute("hidden");
		    element.setAttribute("disabled", "true");
		    element = document.getElementById("Button3");
		    element.setAttribute("value", buttonlabels.getAttribute("button4Label"));
            element.removeAttribute("hidden");
		    element.setAttribute("disabled", "true");
*/		    
		    //set buttons action
		    doSetOKCancel(Send, Cancel, Recipients, Help);

/* XXX Don't deliberately move windos around, compare bug 28260. Also, for any reason, the window will finally sit outside the visible screen for me. /BenB
      //At this point, we cannot position the window because it still doesn't have a width and a height.
      //Let move it out of the screen and then move it back at the right position, this after the first refresh.
      window.moveTo(32000, 32000);
		  setTimeout("moveToAlertPosition();", 0);
*/
//		  moveToAlertPosition();

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

function Cancel()
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
