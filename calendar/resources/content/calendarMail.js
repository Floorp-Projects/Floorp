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

var emailStringBundle;
      
/**** checkForMailNews
 *
 * PURPOSE: if mailnews is installed return true, else false
 *
 */
 
function checkForMailNews()
{
	emailStringBundle = srGetStrBundle("chrome://calendar/locale/email.properties");
   
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

  CalendarDataFilePath = getCalendarDataFilePath();
  if (! CalendarDataFilePath) {
    alert( "No calendarDataFilePath in calendarMail.js" );
    return;
  }
  /* Outputs plain text for message body like
   *
   *   Summary: Pre-midnight gathering
   *   When: Friday, 31 December 1999 11:00 PM -- 11:45 PM
   *   Where: City hall
   *   Organizer: Moz <moz@mozilla.org>
   *
   *   We will gather and ride over en-masse.
   * 
   * For recurring event, date is next occurrence (or prev if none).
   * Date is not repeated if end is same.  Time is omitted for all day events.
   */
  var CalendarText = "";
  var EmailBody = "";
  var Separator = "";

  for( var i = 0; i < gCalendarWindow.EventSelection.selectedEvents.length; i++ ) {
    var event = gCalendarWindow.EventSelection.selectedEvents[i].clone();

    if( event ) {
      event.alarm = false;
      if( event.method == 0 )
	event.method = event.ICAL_METHOD_PUBLISH;

      CalendarText += event.getIcalString();
      var dateFormat = new DateFormater();
      var eventDuration  = event.end.getTime() - event.start.getTime();
      var displayStartDate = getNextOrPreviousOccurrence(event);
      var displayEndDate   = new Date(displayStartDate.getTime() + eventDuration);
      if (event.allDay)
	displayEndDate.setDate(displayEndDate.getDate() - 1);
      var sameDay = (displayStartDate.getFullYear() == displayEndDate.getFullYear() &&
		     displayStartDate.getMonth() == displayEndDate.getMonth() &&
		     displayStartDate.getDay() == displayEndDate.getDay());
      var sameTime = (displayStartDate.getHours() == displayEndDate.getHours() &&
		      displayStartDate.getMinutes() == displayEndDate.getMinutes());
      var when = (event.allDay
		  ? (sameDay
		     // just one day
		     ? dateFormat.getFormatedDate(displayStartDate)
		     // range of days
		     : makeRange(dateFormat.getFormatedDate(displayStartDate),
				 dateFormat.getFormatedDate(displayEndDate)))
		  : (sameDay
		     ? (sameTime
			// just one date time
			? (dateFormat.getFormatedDate(displayStartDate) +
			   " "+ dateFormat.getFormatedTime(displayStartDate))
			// range of times on same day
			: (dateFormat.getFormatedDate(displayStartDate)+
			   " " + makeRange(dateFormat.getFormatedTime(displayStartDate),
					   dateFormat.getFormatedTime(displayEndDate))))
		     // range across different days
		     : makeRange(dateFormat.getFormatedDate(displayStartDate) +
				 " "+ dateFormat.getFormatedTime(displayStartDate),
				 dateFormat.getFormatedDate(displayEndDate) +
				 " "+ dateFormat.getFormatedTime(displayEndDate))));

      EmailBody += Separator;
      Separator = "\n\n";
      EmailBody += (emailStringBundle.GetStringFromName( "Summary" )+" " + nullToEmpty(event.title) +"\n"+
		    emailStringBundle.GetStringFromName( "When" )+" " + when + "\n"+
		    emailStringBundle.GetStringFromName( "Where" )+" " + nullToEmpty(event.location) + "\n"+
		    emailStringBundle.GetStringFromName( "Organizer" )+" " + gMailIdentity.fullName + " <" + gMailIdentity.email + ">" + "\n" +
		    "\n" +
		    (event.description == null? "" : event.description + "\n"));
    }
  }
   
  var EmailSubject;

  if( gCalendarWindow.EventSelection.selectedEvents.length == 1 )
    EmailSubject = gCalendarWindow.EventSelection.selectedEvents[0].title;
  else
    EmailSubject = emailStringBundle.GetStringFromName( "EmailSubject" );

  saveCalendarObject(CalendarDataFilePath, CalendarText);

  // lets open a composer with fields and body prefilled
  try {
    nsIMsgAttachmentComponent = Components.classes["@mozilla.org/messengercompose/attachment;1"];
    nsIMsgAttachment = nsIMsgAttachmentComponent.createInstance(Components.interfaces.nsIMsgAttachment);
    nsIMsgAttachment.name = emailStringBundle.GetStringFromName( "AttachmentName" );
    nsIMsgAttachment.contentType = "text/calendar";
    nsIMsgAttachment.temporary = true;
    nsIMsgAttachment.url = "file://" + CalendarDataFilePath;
    // lets setup the fields for the message
    nsIMsgCompFieldsComponent = Components.classes["@mozilla.org/messengercompose/composefields;1"];
    nsIMsgCompFields = nsIMsgCompFieldsComponent.createInstance(Components.interfaces.nsIMsgCompFields);
    nsIMsgCompFields.useMultipartAlternative = true;
    nsIMsgCompFields.attachVCard = true;
    nsIMsgCompFields.from = gMailIdentity.email;
    nsIMsgCompFields.replyTo = gMailIdentity.replyTo;
    nsIMsgCompFields.subject = EmailSubject;
    nsIMsgCompFields.organization = gMailIdentity.organization;
    nsIMsgCompFields.body = EmailBody;
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
  } catch(ex) {
    alert( ex );
    mdebug("failed to get composer window\nex: " + ex);
  }
}

/** from unifinder.js **/
function getNextOrPreviousOccurrence( calendarEvent )
{
  var isValid = false;

  if( calendarEvent.recur ) {
    var now = new Date();
    var result = new Object();
    isValid = calendarEvent.getNextRecurrence( now.getTime(), result );
    if( isValid )
      return( new Date( result.value ) );

    isValid = calendarEvent.getPreviousOccurrence( now.getTime(), result );
    if( isValid )
      return( new Date( result.value ) );
  }
  // !isValid or !calendarEvent.recur
  if (calendarEvent.start != null) 
    return( new Date( calendarEvent.start.getTime() ) );
  dump("calendarEvent == "+calendarEvent+"\n");
  return null;
}

/** PRIVATE makeRange takes two strings and concatenates them with
    "--" in the middle if they have no spaces, or " -- " if they do. 

    Range dash should look different from hyphen used in dates like 1999-12-31.
    Western typeset text uses an &ndash;.  (Far eastern text uses no spaces.)
    Plain text convention is to use - for hyphen and minus, -- for ndash,
    and --- for mdash.  For now use -- so works with plain text email.
    Add spaces around it only if fromDateTime or toDateTime includes space,
    e.g., "1999-12-31--2000-01-01", "1999-12-31 23:55 -- 2000.01.01 00:05".
**/
function makeRange(fromDateTime, toDateTime) {
  if (fromDateTime.indexOf(" ") == -1 && toDateTime.indexOf(" ") == -1)
    return fromDateTime + "--" + toDateTime;
  else
    return fromDateTime + " -- "+ toDateTime;
}

/** PRIVATE Return string unless string is null, in which case return "" **/
function nullToEmpty(string) {
  return string == null? "" : string;
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
      aLocalFile.append( "Calendar" );
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
   var TempFile = new File( FilePath );
   TempFile.open( "w", 0644 );
   TempFile.write( CalendarStream );
   TempFile.close();
   return;
}

/***************************************************************
 *
 * AUTHOR: Dan Parent
 * DATE:   Monday May 22, 2001
 * 
 * NOTES:
 * Example Implementation (use brackets properly I removed them to keep this short):
 * function sendMessage()
 * {
 *     var EmailSent = sendEmail("Here is a subject", "Here is the body", "danp@oeone.com", null, null, "Here is some text", null, null);
 *     if (EmailSent)
 *         alert("Mail should be on it's way");
 *     else
 *         alert("OOOoooouuuuuGAAAAHHHH!!!");
 * }
 *
 * IMPLEMENTATION NOTES
 * Attachments are still very untested, as is priority, all arguments to 
 * this function are strings, with the to, cc and bcc being comma separated
 * email addresses.
 **************************************************************/


function sendEmail(Subject, Body, To, Cc, Bcc, Attachment, Priority)
{
	/* The message composer component */
	var MessageComposeComponent = Components.classes["@mozilla.org/messengercompose/compose;1"];
	var MessageComposeService = MessageComposeComponent.getService();
	var MessageCompose = MessageComposeService.QueryInterface(Components.interfaces.nsIMsgCompose);
	/* The message composer parameters component, needed by MessageCompose in order to initialize */
	var MessageComposeParamsComponent = Components.classes["@mozilla.org/messengercompose/composeparams;1"];
	var MessageComposeParams = MessageComposeParamsComponent.createInstance(Components.interfaces.nsIMsgComposeParams);
	
   nsIMsgCompFormat = Components.interfaces.nsIMsgCompFormat;
	MessageComposeParams.format = nsIMsgCompFormat.PlainText; // this could be a pref for the user

   /* The type of delivery to use, we always use send now */
	var MessageDeliverMode = Components.interfaces.nsIMsgCompDeliverMode;
	var AccountExists = true;
	if ( !hasDefaultAccount())
	{
		AccountExists = accountSetupError();
	}
	/* The account manager is needed to get the default identity to send the mail out */
	var AccountManagerComponent = Components.classes["@mozilla.org/messenger/account-manager;1"];
	var AccountManagerService = AccountManagerComponent.getService();
	var AccountManager = AccountManagerService.QueryInterface(Components.interfaces.nsIMsgAccountManager);
	try
    {
      	var Account = AccountManager.defaultAccount;
    }
   catch(e)
   {
      alert( "\n------------------\nThere is no default account, and I caught an exception in penemail.js on line 45. "+e +"\n--------------------------------\n" );
	   AccountExists = accountSetupError();
   }
	
   if (!AccountExists)
	{
		return(false);
	}

	/* try to initialize the message composer, if not return false */
	try
	{
		MessageCompose.Initialize(null, MessageComposeParams);
	}
	catch(Exception)
	{
		alert( "can't Initialize"+Exception );
      return(false);
	}
	
	/* The composer fields that need to be set in order to properly send an email */
	/* TODO: should this really be set by the default account or is there going to be
	   a system account, or is the system account the default account? */
	var MessageComposeFields = MessageCompose.compFields;
	if (Subject == "")
	{
		MessageComposeFields.subject = "[no subject]";
	}
	else
	{
		MessageComposeFields.subject = Subject;
	}
	if (To == "" || To == null)
	{
		return(false); // the email *has* to be addressed to somebody
	}
	else
	{
		MessageComposeFields.to = To;
	}
	if (Cc != "" && Cc != null)
	{
		MessageComposeFields.cc = Cc;
	}
	if (Bcc != "" && Bcc != null)
	{
		MessageComposeFields.bcc = Bcc;
	}
	if (Body != "" && Body != null)
		MessageComposeFields.body = "\n"+Body+"\n";
	if (Attachment != "" && Attachment != null)
		MessageComposeFields.attachments = Attachment;
	if (Priority != "" && Priority != null)
		MessageComposeFields.priority = Priority;
	MessageComposeFields.from = Account.email;
    MessageComposeFields.forcePlainText = true;
    MessageComposeFields.useMultipartAlternative = false;

	/* try to send the email away, don't set a listener, we just trust that it makes it for now :) */
	/* TODO: properly set a listener on this action and report errors back to the calling function */
	try
	{
		MessageCompose.SendMsg(MessageDeliverMode.Now, Account.defaultIdentity, Account.key,null,null);
	}
	catch(Exception)
	{
		alert( "can't SendMsg"+Exception );
      return(false);
	}
	
   /* we made it here so everything must be good unless something happens during the transport of the message */
	return(true);
}

function accountSetupError( )
{
    var rv = confirm( "You do not have a default email account setup. Click OK and the Account Wizard will be started, otherwise click Cancel." );
    if (rv)
    {
		launchAccountWizard();
    }
	var AccountManagerComponent = Components.classes["@mozilla.org/messenger/account-manager;1"];
	var AccountManagerService = AccountManagerComponent.getService();
	var AccountManager = AccountManagerService.QueryInterface(Components.interfaces.nsIMsgAccountManager);
	try
	{
		if (AccountManager.defaultAccount)
		{
			return(true);
		}
		else
		{
			return(false);
		}
	}
	catch(ex)
	{
		return(false);
	}
}

/**** ADDED BY DAN 2001-10-29 ****
 *
 * RETURNS true if default account, false if no account
 *
 ********************************/

function hasDefaultAccount()
{
	try
	{
		var AccountManagerComponent = Components.classes["@mozilla.org/messenger/account-manager;1"];
		var AccountManagerService = AccountManagerComponent.getService();
		var AccountManager = AccountManagerService.QueryInterface(Components.interfaces.nsIMsgAccountManager);
		var DefaultAccount = AccountManager.defaultAccount;
		var DefaultIncomingServer = DefaultAccount.incomingServer;
		emailStringBundle = srGetStrBundle("chrome://messenger/locale/messenger.properties");
      var LocalFolders = emailStringBundle.GetStringFromName( "localFolders" );
      if (DefaultIncomingServer.hostName == LocalFolders)
		{
			return(false);
		}
		else
		{
			return(true);
		}
	}
	catch(ex)
	{
		return(false);
	}
}

/**** launchAccountWizard ****
 *
 * This will launch the Account Wizard from any point the email service can be used
 *
 ****/

function launchAccountWizard()
{
	window.openDialog("chrome://messenger/content/AccountWizard.xul", "", "chrome,modal,titlebar,resizable");
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
