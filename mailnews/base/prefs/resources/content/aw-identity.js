/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 * Seth Spitzer <sspitzer@netscape.com>
 */

var currentDomain;
var Bundle = srGetStrBundle("chrome://messenger/locale/prefs.properties");

function validate(data)
{
  var name = document.getElementById("fullName").value;

  if (! name || name=="") {
    var alertText = Bundle.GetStringFromName("enterName");
    window.alert(alertText);
    return false;
  }
  if (!validateEmail()) return false;

  parent.UpdateWizardMap();
  
  return true;
}

// this is kind of wacky.. basically it validates the email entered in
// the text field to make sure it's in the form "user@host"..
//
// However, if there is a current domain (retrieved during onInit)
// then we have to do some special tricks:
// - if the user ALSO entered an @domain, then we just chop it off
// - at some point it would be useful to keep the @domain, in case they
//   wish to override the domain.
function validateEmail() {
  var emailElement = document.getElementById("email");
  var email = emailElement.value;

  
  var emailArray = email.split('@');

  if (currentDomain) {
    // check if user entered and @ sign even though we have a domain
    if (emailArray.length >= 2) {
      email = emailArray[0];
      emailElement.value = email;
    }
    
    if (email.length =="") {
      var alertText = Bundle.GetStringFromName("enterValidEmailPrefix");
      window.alert(alertText);
      return false;
    }
  }

  else {
    if (emailArray.length != 2 ||
        emailArray[0] == "" ||
        emailArray[1] == "") {
      alertText = Bundle.GetStringFromName("enterValidEmail");
      window.alert(alertText);
      return false;
    }
  }
    
  return true;
}

function onInit()
{
  setEmailDescriptionText();
  checkForDomain();
  checkForFullName(); 
  checkForEmail(); 
}

// Use email example data that ISP has provided. ISP data, if avaialble
// for the choice user has made, will be read into CurrentAccountData. 
// Default example data from properties will be used when the info is missing. 
function setEmailDescriptionText()
{
    var emailDescText = document.getElementById("emailDescText");
    var emailFieldLabel = document.getElementById("emailFieldLabel");
    var currentAccountData = parent.gCurrentAccountData;
   
    var displayText =  null;
    var emailFieldLabelData =  null;
    var setDefaultEmailDescStrings = true; 

    // Get values for customized data from current account 
    if (currentAccountData)
    {
        var emailProvider  = currentAccountData.emailProviderName;
        var sampleEmail    = currentAccountData.sampleEmail;
        var sampleUserName = currentAccountData.sampleUserName;
        var emailIDDesc    = currentAccountData.emailIDDescription;
        var emailIDTitle   = currentAccountData.emailIDFieldTitle;

        if (emailProvider  &&
            sampleEmail    &&
            sampleUserName &&
            emailIDDesc    &&
            emailIDTitle)
        {
            // Get email description data
            displayText = Bundle.GetStringFromName("customizedEmailText")
                                .replace(/%emailProvider%/g, emailProvider)
                                .replace(/%mailIDDesc%/g, emailIDDesc)
                                .replace(/%sampleEmail%/, sampleEmail)
                                .replace(/%sampleUserName%/, sampleUserName);

            // Set emailfield label
            emailFieldLabelData =  emailIDTitle;
            emailFieldLabel.setAttribute("value", emailFieldLabelData);

            // Need to display customized data. Turn off default settings.
            setDefaultEmailDescStrings = false; 
        }
    }

    if (setDefaultEmailDescStrings)
    {
        // Check for obtained values and set with default values if needed
        var username        = Bundle.GetStringFromName("exampleEmailUserName"); 
        var domain          = Bundle.GetStringFromName("exampleEmailDomain"); 

        displayText = Bundle.GetStringFromName("defaultEmailText")
                            .replace(/%username%/, username)
                            .replace(/%domain%/, domain);
    }

    // Create a text nodes with text to be displayed
    var emailDescTextNode       =  document.createTextNode(displayText);

    // Display the dynamically generated text for email description 
    emailDescText.appendChild(emailDescTextNode);
}

// retrieve the current domain from the parent wizard window,
// and update the UI to add the @domain static text
function checkForDomain()
{
  var accountData = parent.gCurrentAccountData;
  if (!accountData) return;
  if (!accountData.domain) return;

  // save in global variable
  currentDomain = accountData.domain;
  
  var postEmailText = document.getElementById("postEmailText");
  postEmailText.setAttribute("value", "@" + currentDomain);
}

function checkForFullName() {
    var name = document.getElementById("fullName");
    if (name.value=="") {
        try {
            var userInfo = Components.classes["@mozilla.org/userinfo;1"].getService(Components.interfaces.nsIUserInfo);
            name.value = userInfo.fullname;
        }
        catch (ex) {
            // dump ("checkForFullName failed: " + ex + "\n");
        }
    }
}

function checkForEmail() {
    var email = document.getElementById("email");
    if (email.value=="") {
        try {
            var userInfo = Components.classes["@mozilla.org/userinfo;1"].getService(Components.interfaces.nsIUserInfo);
            email.value = userInfo.emailAddress;
        }
        catch (ex) {
            // dump ("checkForEmail failed: " + ex + "\n"); 
        }
    }
}
