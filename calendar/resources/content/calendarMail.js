/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Dan Parent <danp@oeone.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gMailAccounts = false;
var gMAILDEBUG = 1;  // 0 - no output, 1 dump to terminal, > 1 use alerts
var gMailIdentity;

/**** checkForMailNews
 *
 * PURPOSE: if mailnews is installed return true, else false
 *
 */
 
function checkForMailNews()
{
	var AccountManagerComponent;
	var AccountManagerService;
	var AccountManager;
	var DefaultAccount;
	var SmtpServiceComponent;
	var SmtpService;
	var Smtp;
	var DefaultSmtp;

	try
	{
		AccountManagerComponent = Components.classes["@mozilla.org/messenger/account-manager;1"];
		AccountManagerService = AccountManagerComponent.getService();
	}
	catch(ex) // yikes no account manager == no accounts
	{
		// from bug 122651 - scenario 1
		noMail();
		return;
	}
	try
	{
		AccountManager = AccountManagerService.QueryInterface(Components.interfaces.nsIMsgAccountManager);
		DefaultAccount = AccountManager.defaultAccount;
		gMailIdentity = DefaultAccount.defaultIdentity; // we'll store the user's account info globally for now
	}
	catch(ex)
	{
		// from bug 122651 - scenario 2
		noAccount();
		return;
	}
	try
	{
		SmtpServiceComponent = Components.classes["@mozilla.org/messengercompose/smtp;1"];
		SmtpService = SmtpServiceComponent.getService();
		Smtp = SmtpService.QueryInterface(Components.interfaces.nsISmtpService);
		DefaultSmtp = Smtp.defaultServer;
	}
	catch(ex)
	{
		// if we don't have an SMTP server to use I'm not going
		// to allow mail features to be enabled for now
		// from bug 122651: scenarios 3, and 6
		noSmtp();
		return;
	}
	// if we get here that means we must have a default account and default smtp server otherwise
	// the thrown exceptions would have been caught.
	// from bug 122651 - scenarios 4, 5, 7, and 8
	// we'll just use the default account for now.
	gMailAccounts = true;
}

/**** noMail
 *
 * PURPOSE: function to handle the scenario when mailnews is not installed
 *
 */
 
function noMail()
{
	// maybe provide a way of telling the user how to get a mailnews build
	// this should be a pref with a window that has a checkbox to shut off
	// this warning (for people that don't want mailnews installed)
	mdebug("You don't have mail installed");
}

/**** noAccount
 *
 * PURPOSE: function to handle the scenario where mailnews is installed but
 *          an account has not been created
 */

function noAccount()
{
	// this could call the account wizard I guess thats a later feature
	mdebug("You don't have a mail account");
}


/**** noSmtp
 *
 * PURPOSE: function to handle the scenario where mailnews is installed, has
 *          a default incoming account but does not have an smtp service
 *
 * XXX: can this happen?
 *
 */

function noSmtp()
{
	/* I don't really know if its possible to create this scenario, I'm putting it here for
	 * the sake of completion so that I can say I've at least covered all possible scenarios
	 * from bug 122651
	 */
	 mdebug("You don't have an smtp account");
}

/**** sendEvent
 *
 * PURPOSE: implement iMip :) - that is send out a calendar event both
 *          as an attachment of type text/calendar and as text/plain
 */

function sendEvent()
{
	var Event;
	var CalendarDataFilePath;
	var nsIMsgCompFieldsComponent;
	var nsIMsgCompFields;
	var nsIMsgComposeParamsComponent;
	var nsIMsgComposeParams;
	var nsIMsgComposeServiceComponent;
	var nsIMsgComposeService;
	var nsIMsgCompFormat;
	var nsIMsgCompType;
	var nsIMsgAttachmentComponent;
	var nsIMsgAttachment;

	Event = gCalendarWindow.EventSelection.selectedEvents[0];
	CalendarDataFilePath = getCalendarDataFilePath();
	if (! CalendarDataFilePath)
	{
		return;
	}
	/* Want output like
	 * When: Thursday, November 09, 2000 11:00 PM-11:30 PM (GMT-08:00) Pacific Time
     * (US & Canada); Tijuana.
     * Where: San Francisco
	 * see bug 59630
	 */
	if (Event)
	{
		// lets open a composer with fields and body prefilled
		try
		{
			saveCalendarObject(CalendarDataFilePath, Event.getIcalString());
			nsIMsgAttachmentComponent = Components.classes["@mozilla.org/messengercompose/attachment;1"];
			nsIMsgAttachment = nsIMsgAttachmentComponent.createInstance(Components.interfaces.nsIMsgAttachment);
			nsIMsgAttachment.name = "iCal Event"
			nsIMsgAttachment.contentType = "text/calendar;"
			nsIMsgAttachment.temporary = true;
			nsIMsgAttachment.url = "file://" + CalendarDataFilePath;
			// lets setup the fields for the message
			nsIMsgCompFieldsComponent = Components.classes["@mozilla.org/messengercompose/composefields;1"];
			nsIMsgCompFields = nsIMsgCompFieldsComponent.createInstance(Components.interfaces.nsIMsgCompFields);
			nsIMsgCompFields.useMultipartAlternative = true;
			nsIMsgCompFields.attachVCard = true;
			nsIMsgCompFields.from = gMailIdentity.email;
			nsIMsgCompFields.replyTo = gMailIdentity.replyTo;
			nsIMsgCompFields.subject = gMailIdentity.fullName + " would like to schedule a meeting with you";
			nsIMsgCompFields.organization = gMailIdentity.organization;
			nsIMsgCompFields.body = "When: " + Event.start + "-" + Event.end + "\nWhere: " + Event.location + "\nOrganizer: " + gMailIdentity.fullName + " <" + gMailIdentity.email + ">" + "\nSummary:" + Event.description;
			nsIMsgCompFields.addAttachment(nsIMsgAttachment);
			/* later on we may be able to add:
			 * returnReceipt, attachVCard
			 */
			// time to handle the message paramaters
			nsIMsgComposeParamsComponent = Components.classes["@mozilla.org/messengercompose/composeparams;1"];
			nsIMsgComposeParams = nsIMsgComposeParamsComponent.createInstance(Components.interfaces.nsIMsgComposeParams);
			nsIMsgComposeParams.composeFields = nsIMsgCompFields;
			nsIMsgCompFormat = Components.interfaces.nsIMsgCompFormat;
			nsIMsgCompType = Components.interfaces.nsIMsgCompType;
			nsIMsgComposeParams.format = nsIMsgCompFormat.PlainText; // this could be a pref for the user
			nsIMsgComposeParams.type = nsIMsgCompType.New;
			// finally lets pop open a composer window after all this work :)
			nsIMsgComposeServiceComponent = Components.classes["@mozilla.org/messengercompose;1"];
			nsIMsgComposeService = nsIMsgComposeServiceComponent.getService().QueryInterface(Components.interfaces.nsIMsgComposeService);
			nsIMsgComposeService.OpenComposeWindowWithParams(null, nsIMsgComposeParams);
		}
		catch(ex)
		{
			mdebug("failed to get composer window\nex: " + ex);
		}
	}
}

/**** getCalendarDataFilePath
 *
 * PURPOSE: get the user's current profile directory and use it as a place
 *          to temporarily store the iCal tempfile that I need to attach
 *          to the email
 */

function getCalendarDataFilePath()
{
	var FilePath;
	var DirUtilsComponent;
	var DirUtilsInstance;
	var nsIFile;
	
	try
	{
		DirUtilsComponent = Components.classes["@mozilla.org/file/directory_service;1"];
		DirUtilsInstance = DirUtilsComponent.createInstance(Components.interfaces.nsIProperties);
		nsIFile = Components.interfaces.nsIFile;
		FilePath = DirUtilsInstance.get("ProfD", nsIFile).path; 
		var aFile = Components.classes["@mozilla.org/file/local;1"].createInstance();
      var aLocalFile = aFile.QueryInterface(Components.interfaces.nsILocalFile);
      if (!aLocalFile) return false;
      
      aLocalFile.initWithPath(FilePath);
      aLocalFile.append( "tempIcal.ics" );

      FilePath = aLocalFile.path;
	}
	catch(ex)
	{
		mdebug("No filepath has been found, ex:\n" + ex);
		FilePath = null;
	}
	return(FilePath);
}

/**** saveCalendarObject
 *
 * PURPOSE: open a tmp file and write Ical info to it, this has
 *          to be done until there is a way to pass an ical URL to
 *          the messenger attachment code
 */

function saveCalendarObject(FilePath, CalendarStream)
{
	var LocalFileComponent;
	var LocalFileInstance;
	var FileChannelComponent;
	var FileChannelInstance;
	var FileTransportComponent;
	var FileTransportService;
	var FileTransport;
	var FileOutput;
	
	// need LocalFile, otherwise what are we going to write to?
	LocalFileComponent = Components.classes["@mozilla.org/file/local;1"];
	LocalFileInstance = LocalFileComponent.createInstance(Components.interfaces.nsILocalFile);
	LocalFileInstance.initWithPath(FilePath);
	if (!LocalFileInstance.exists())
	{
		try
		{
			LocalFileInstance.create(LocalFileInstance.NORMAL_FILE_TYPE, 0644);
		}
		catch(ex)
		{
			mdebug("Unable to create temporary iCal file");
		}
	}
	// need nsIFileChannel for ioflags
	FileChannelComponent = Components.classes["@mozilla.org/network/local-file-channel;1"];
	FileChannelInstance = FileChannelComponent.createInstance(Components.interfaces.nsIFileChannel);
	// need nsIFileTransport to create a transport
	FileTransportComponent = Components.classes["@mozilla.org/network/file-transport-service;1"];
	FileTransportService = FileTransportComponent.getService(Components.interfaces.nsIFileTransportService);
	// need nsITransport to open an output stream
	FileTransport = FileTransportService.createTransport(LocalFileInstance, FileChannelInstance.NS_RDWR, 0644, true);
	// need nsIOutputStream to write out the actual file
	FileOutput = FileTransport.openOutputStream(0, -1, null);
	// now lets actually write the output to the file
	FileOutput.write(CalendarStream, CalendarStream.length);
	FileOutput.close();
}

/**** mdebug
 *
 * PURPOSE: display debugging messages
 *
 */

function mdebug(message)
{
	if (gMAILDEBUG == 1)
	{
		dump(message + "\n");
	}
	else if (gMAILDEBUG > 1)
	{
		alert(message);
	}
}
