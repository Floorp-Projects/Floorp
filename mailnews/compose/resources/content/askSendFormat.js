/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
var msgCompSendFormat = Components.interfaces.nsIMsgCompSendFormat;
var msgCompConvertible = Components.interfaces.nsIMsgCompConvertible;
var param = null;

/* There is 3 preferences that let you customize the behavior of this dialog

1. pref("mail.asksendformat.default", 1); //1=plaintext, 2=html, 3=both
   This define the default action selected when the dialog open. This could be overwritten by the preference
   mail.asksendformat.recommended_as_default (described here after)


2. pref("mail.asksendformat.recommended_as_default", true);
   If you set this preference to true and we have a recommended action, this action will be selected by default.
   In this case, we ignore the preference mail.asksendformat.default

 
3. pref("mail.asksendformat.display_recommendation", true);
   When this preference is set to false, the recommended action label will not be displayed next to the action
   radio button. However, the default action might be change to the recommended one if the preference
   mail.asksendformat.recommended_as_default is set.
*/

var defaultAction = msgCompSendFormat.PlainText;
var recommended_as_default = true;
var display_recommendation = true;
var useDefault =false;

var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService();
if (prefs) {
  prefs = prefs.QueryInterface(Components.interfaces.nsIPrefBranch);
  if (prefs) {
    try {
      defaultAction = prefs.getIntPref("mail.asksendformat.default");
      useDefault = true;
    } catch (ex) {}
    try {
      recommended_as_default = prefs.getBoolPref("mail.asksendformat.recommended_as_default");
    } catch (ex) {}
    try {
      display_recommendation = prefs.getBoolPref("mail.asksendformat.display_recommendation");
    } catch (ex) {}
  }
}

function Startup()
{
  if (window.arguments && window.arguments[0])
  {    
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
          labeldeck.setAttribute("selectedIndex", 1);
          // No icon
          break;
        case msgCompConvertible.Yes:
          labeldeck.setAttribute("selectedIndex", 1);
          icon.setAttribute("id", "convertYes");
          break;
        case msgCompConvertible.Altering:
          labeldeck.setAttribute("selectedIndex", 2);
          icon.setAttribute("id", "convertAltering");
          break;
        case msgCompConvertible.No:
          labeldeck.setAttribute("selectedIndex", 3);
          icon.setAttribute("id", "convertNo");
          break;
      }

      // Set the default radio array value and recommendation
      var group = document.getElementById("mailDefaultHTMLAction");
      var recommlabels = document.getElementById("hiddenLabels");
      var label;
      var haveRecommendation = false;
      var format = (useDefault) ? defaultAction : param.action;

      switch (format)
      {
        case msgCompSendFormat.AskUser:
          //haveRecommendation = false;
          break;
        case msgCompSendFormat.PlainText:
          //label = recommlabels.getAttribute("plainTextOnlyRecommendedLabel");
          label = document.getElementById("plainTextOnlyRecommended");
               // elements for "recommended" are a workaround for bug 49623
          haveRecommendation = true;
          break;
        case msgCompSendFormat.Both:
          //label = recommlabels.getAttribute("plainTextAndHtmlRecommendedLabel");
          label = document.getElementById("plainTextAndHtmlRecommended");
          haveRecommendation = true;
          break;
        case msgCompSendFormat.HTML:
          //label = recommlabels.getAttribute("htmlOnlyRecommendedLabel");
          label = document.getElementById("htmlOnlyRecommended");
          haveRecommendation = true;
          break;
      }
      if (haveRecommendation)
      {
        if (display_recommendation)
        {
          /*
          dump(element.getAttribute("value"));
          element.setAttribute("value", label);
          element.setAttribute("value", "foo");
          dump(element.getAttribute("value"));
          */
          
          label.removeAttribute("hidden");
        }
        if (recommended_as_default)
        {
          group.value = format;
        }
      }
      if (!haveRecommendation || !recommended_as_default)
      {
        group.value = defaultAction;
      }

      //change the button label
      var buttonlabels = document.getElementById("hiddenLabels");
      var sendButton = document.documentElement.getButton("accept");
      sendButton.setAttribute("label", buttonlabels.getAttribute("sendLabel"));
/*
      var recipientsButton = document.documentElement.getButton("disclosure");
      recipientsButton.setAttribute("value", buttonlabels.getAttribute("recipientsLabel"));
      recipientsButton.setAttribute("disabled", "true");
*/        
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
    // param.action should be an integer for when it is returned to MsgComposeCommands.js
    param.action = parseInt(document.getElementById("mailDefaultHTMLAction").value);
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
