/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *	Seth Spitzer <sspitzer@netscape.com>
 *  Ben Goodger <ben@netscape.com>
 */  

var newmessages = "";
var newsgroupname = "";
var Bundle = srGetStrBundle("chrome://messenger/locale/news.properties");
var prefs = Components.classes['@mozilla.org/preferences;1'].getService();
prefs = prefs.QueryInterface(Components.interfaces.nsIPref); 

var serverid = null;
var markreadElement = null;
var numberElement = null;

var server = null;
var nntpServer = null;
var args = null;

function OnLoad()
{
	doSetOKCancel(OkButtonCallback, CancelButtonCallback);

	if (window.arguments && window.arguments[0]) {
        args = window.arguments[0].QueryInterface( Components.interfaces.nsINewsDownloadDialogArgs);
    
        args.hitOK = false; /* by default, act like the user hit cancel */
        args.downloadAll = false; /* by default, act like the user did not select download all */

		var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
		server = accountManager.getIncomingServer(args.serverKey);
		nntpServer = server.QueryInterface(Components.interfaces.nsINntpIncomingServer);

		var downloadHeadersTitlePrefix = Bundle.GetStringFromName("downloadHeadersTitlePrefix");
		var downloadHeadersInfoText1 = Bundle.GetStringFromName("downloadHeadersInfoText1");
		var downloadHeadersInfoText2 = Bundle.GetStringFromName("downloadHeadersInfoText2");
        var okButtonText = Bundle.GetStringFromName("okButtonText");

		window.title = downloadHeadersTitlePrefix;

        // this is not i18n friendly, fix this
		var infotext = downloadHeadersInfoText1 + " " + args.articleCount + " " + downloadHeadersInfoText2;
		setText('info',infotext); 
        var okbutton = document.getElementById("ok");
        okbutton.setAttribute("value", okButtonText);
        setText("newsgroupLabel", args.groupName);
	}

	numberElement = document.getElementById("number");
	numberElement.value = nntpServer.maxArticles;

	markreadElement = document.getElementById("markread");
	markreadElement.checked = nntpServer.markOldRead;

	return true;
}

function setText(id, value) {
    var element = document.getElementById(id);
    if (!element) return;
 	if (element.hasChildNodes())  
      element.removeChild(element.firstChild);
    var textNode = document.createTextNode(value);
    element.appendChild(textNode);
}

function OkButtonCallback() {
	nntpServer.maxArticles = numberElement.value;
	//dump("mark read checked?="+markreadElement.checked+"\n");
	nntpServer.markOldRead = markreadElement.checked;

    var radio = document.getElementById("all");
    if (radio) {
        args.downloadAll = radio.checked;
    }

    args.hitOK = true;
	return true;
}

function CancelButtonCallback() {
    args.hitOK = false;
	return true;
}

function setupDownloadUI(enable) {
  var checkbox = document.getElementById("markread");
  var numberFld = document.getElementById("number");

  if (enable) {
    checkbox.removeAttribute("disabled");
    numberFld.removeAttribute("disabled");
  }
  else {
    checkbox.setAttribute("disabled", "true");
    numberFld.setAttribute("disabled", "true");
  }
}
