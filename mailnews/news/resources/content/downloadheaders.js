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
 *	sspitzer@netscape.com
 */  

var newmessages = "";
var newsgroupname = "";
var Bundle = srGetStrBundle("chrome://messenger/locale/news.properties");
var prefs = Components.classes['component://netscape/preferences'].getService();
prefs = prefs.QueryInterface(Components.interfaces.nsIPref); 

var serverid = null;
var markreadElement = null;
var numberElement = null;

var server = null;
var nntpServer = null;



function OnLoad()
{
	doSetOKCancel(OkButtonCallback, null);

	if (window.arguments && window.arguments[0]) {
		var args = window.arguments[0].split(",");
		// args will be of the form <number>,<newsgroup name>,<serverid>
		newmessages = args[0];
		newsgroupname = args[1];
		serverid = args[2];

		dump("new message count = " + newmessages + "\n");
		dump("newsgroup name = " + newsgroupname + "\n");
		dump("serverid = " + serverid + "\n");

		var accountManager = Components.classes["component://netscape/messenger/account-manager"].getService(Components.interfaces.nsIMsgAccountManager);
		server = accountManager.getIncomingServer(serverid);
		nntpServer = server.QueryInterface(Components.interfaces.nsINntpIncomingServer);

		var downloadHeadersTitlePrefix = Bundle.GetStringFromName("downloadHeadersTitlePrefix");
		var downloadHeadersInfoText1 = Bundle.GetStringFromName("downloadHeadersInfoText1");
		var downloadHeadersInfoText2 = Bundle.GetStringFromName("downloadHeadersInfoText2");

		// doesn't JS have a printf?
		window.title = downloadHeadersTitlePrefix + " " + newsgroupname;
		var infotext = downloadHeadersInfoText1 + " " + newmessages + " " + downloadHeadersInfoText2;
		setDivText('info',infotext);
	}


	numberElement = document.getElementById("number");
	numberElement.value = nntpServer.maxArticles;

	markreadElement = document.getElementById("markread");
	markreadElement.checked = nntpServer.markOldRead;

	return true;
}

function setDivText(divname, value) {
    var div = document.getElementById(divname);
    if (!div) return;
    if (div.firstChild)
        div.removeChild(div.firstChild);
    div.appendChild(document.createTextNode(value));
}

function OkButtonCallback() {
	nntpServer.maxArticles = numberElement.value;
	nntpServer.markOldRead = markreadElement.checked;

	return true;
}
