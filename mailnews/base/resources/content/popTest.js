netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
dump("AM = " + accountManager + "\n");
netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
var smtpservice = Components.classes["@mozilla.org/messengercompose/smtp;1"].getService(Components.interfaces.nsISmtpService);
dump("SMTPSERVICE =	" + smtpservice + "\n");
smtpserver = smtpservice.defaultServer;
smtpserver.hostname = "parp.mcom.com";
dump("SMTPSERVER CREATED \n");

var msgwindow = Components.classes["@mozilla.org/messenger/msgwindow;1"].getService(Components.interfaces.nsIMsgWindow);
var identity;
var subfolders;
var accountKey;
var server;
var time;
var rv = "";
var final_result = "Passed";
var createAccountResult;  //to store whether createAccount passed or not

var urlListener = {
    OnStartRunningUrl: function  (aUrl) {
		},
    OnStopRunningUrl: function (aUrl, aExitCode) {
		}
	};

function createAccount() {

netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
//POP Account
try { 
	dump("********Now we have " + accountManager.accounts.Count() + " accounts\n");
	for (var j=0; j<accountManager.allServers.Count(); j++) {
		var tserver = accountManager.allServers.GetElementAt(j).QueryInterface(Components.interfaces.nsIMsgIncomingServer);
		if (tserver.username == 'parp4')  {
			dump("Account parp4 already exists.. Using the existing account...\n");
			var account_exists = 1;
		}
	}

	if (account_exists != 1) {
		var old_count = accountManager.accounts.Count();
		var account = accountManager.createAccount();
		accountKey = account.key;
		dump("NEW KEY = " + accountKey + "\n");
		identity = accountManager.createIdentity();
		server = accountManager.createIncomingServer("parp4", "parp.mcom.com", "pop3");
		server.rememberPassword = true;
		account.addIdentity(identity);
		account.incomingServer = server;
		dump("Created identity " + identity + "\n");
		dump("Created server " + server + "\n");

		server.prettyName = "parp4";
		server.hostName = "parp.mcom.com";
		server.username = "parp4";
		server.password = "parp4";

		identity.identityName = "parp4";
		identity.fullName = "parp4";
		identity.email = "parp4@parp.mcom.com";

		dump("********Now we have " + accountManager.accounts.Count() + " accounts\n");
		var new_count = accountManager.accounts.Count();
		if (new_count == old_count + 1) {
			dump("Sucessfully created an Account\n");
			createAccountResult = "Passed";
			rv += "createAccount:" + "\t" + "Passed" + "\n";
		}

		else {
			dump("Failed to create an account \n");
			createAccountResult = "Failed";
			rv += "createAccount:" + "\t" + "Failed" + "\n";
			if (final_result == "Passed")
				final_result = "Failed";
		}
	} //if

	} //try
	catch (e) {
		dump("E = " + e + "\n");
	}



// local folder 
	var messengerMigrator = Components.classes["@mozilla.org/messenger/migrator;1"].getService(Components.interfaces.nsIMessengerMigrator);
	messengerMigrator.createLocalMailAccount(false);
	dump("Created local mail account \n");
}

function removeAccount() {
netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
try { 
	dump("********Now we have " + accountManager.accounts.Count() + " accounts\n");
	var old_count = accountManager.accounts.Count();
	if(old_count == 0) {
		dump("NO Accounts Found..\n");
		rv += "removeAccount:" + "\t" + "Failed" + "\n";
		dump("Writing Failed to file for removeAccount...\n");
		if (final_result == "Passed")
			final_result = "Failed";
	}
	else {
		var accounts = accountManager.accounts;
		var JSAccounts = nsISupportsArray2JSArray(accounts, Components.interfaces.nsIMsgAccount);
		for (var i=0; i<JSAccounts.length; i++) {
			if(JSAccounts[i].key == accountKey) {
				accountManager.removeAccount(JSAccounts[i]);
			}
		}  //for

		var	new_count = accountManager.accounts.Count();
		if (new_count == old_count - 1) {
			dump("Sucessfully Deleted an Account\n");
			rv += "removeAccount:" + "\t" + "Passed" + "\n";
			dump("Writing Passed to file for RemoveAccount...\n");
		} 
		else {
			rv += "removeAccount:" + "\t" + "Failed" + "\n";
			dump("Writing Failed to file for removeAccount...\n");
			if (final_result == "Passed")
				final_result = "Failed";
		} //else

	} //else

	// to remove local folder
	 dump("******** Now we have " + accountManager.accounts.Count() + " accounts\n");
	for (var j=0; j<accountManager.allServers.Count(); j++) { 
   		var server=accountManager.allServers.GetElementAt(j).QueryInterface(Components.interfaces.nsIMsgIncomingServer);  	
		if (server.type == 'none')  { 
			dump("server key is " + server.key + "\n");
			var localAccount = accountManager.FindAccountForServer(server);
			dump("LOCAL ACCOUNT = " + localAccount + "\n");
			if (localAccount) {
				accountManager.removeAccount(localAccount);
				dump("Deleted Local Account....\n");
			}
 		}  //if

	} //for

	 dump("******** Now we have " + accountManager.accounts.Count() + " accounts\n");
	} //try 

	catch (e) { 
	dump("E = " + e + "\n"); 
	} 

	dump("RV: " + rv + "\n");
	dump("POP MAILNEWS TEST: " + final_result + "\n");
}



function updateFolder() {

		netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
		try {
		dump(" Server Count = " + accountManager.allServers.Count() + "\n");
		for (var j=0; j < accountManager.allServers.Count(); j++) {
		var server = accountManager.allServers.GetElementAt(j).QueryInterface(Components.interfaces.nsIMsgIncomingServer);
		if (server.username == 'parp4')  {
			var rootFolder = server.RootFolder;
			dump("rootfolder = " + rootFolder + "\n");
			var subFolderEnumerator = rootFolder.GetSubFolders();
			// dump("subFolderEnumerator = " + subFolderEnumerator + "\n");
			subfolders = nsIEnumerator2JSArray(subFolderEnumerator, Components.interfaces.nsIMsgFolder);
			// dump("SUBFOLDER = " + subfolders + "\n");
			for (var i=0; i<subfolders.length; i++) {
				if((subfolders[i].name == 'Inbox') || (subfolders[i].name == 'INBOX')) {
					var resource = subfolders[i].QueryInterface(Components.interfaces.nsIRDFResource);
					dump("resource uri = " + resource.Value + "\n");
					subfolders[i].updateFolder(msgwindow);
					dump("Updated the folder!! \n");
					var messages_enumerator = subfolders[i].getMessages(msgwindow); 

				} //if

			}  //for

		}  //if

		} //for
	
} //try 
 
		catch (e) { 
 			dump("E = " + e + "\n"); 
 		} 

}


function getNewMail()
{
	netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead"); 
	netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect"); 
	try {
	var instanceofpop3service =Components.classes['@mozilla.org/messenger/popservice;1'].getService();
	var interfaceofpop3service =instanceofpop3service.QueryInterface(Components.interfaces.nsIPop3Service); 
	dump("interfaceofpop3service = " + interfaceofpop3service +"\n");
  	var popserver =server.QueryInterface(Components.interfaces.nsIPop3IncomingServer);   
	dump("popserver = " + popserver + "\n");  
	interfaceofpop3service.GetNewMail(msgwindow, urlListener, popserver);  
	}  //try
	catch (e) {
		dump(" E = " + e + "\n");
	}
}

function deleteMessage() {
	var flag = 0;
	netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
	try {
		var supportsarray =Components.classes["@mozilla.org/supports-array;1"].getService(Components.interfaces.nsISupportsArray);
 		dump("ARRAY = " + supportsarray + "\n");
 		netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 		dump("MSGWINDOW = " + msgwindow + "\n");
		for (var j=0; j<accountManager.allServers.Count(); j++) {
		var server = accountManager.allServers.GetElementAt(j).QueryInterface(Components.interfaces.nsIMsgIncomingServer);
		if (server.username == 'parp4')  {
			var rootFolder = server.RootFolder;
			dump("RootFolder = " + rootFolder + "\n");
			var subFolderEnumerator = rootFolder.GetSubFolders();
			var dsubfolders = nsIEnumerator2JSArray(subFolderEnumerator, Components.interfaces.nsIMsgFolder);
			for (var i=0; i<dsubfolders.length; i++) {
				if((dsubfolders[i].name == 'Inbox') || (dsubfolders[i].name == 'INBOX')) {
					var resource = dsubfolders[i].QueryInterface(Components.interfaces.nsIRDFResource);
					dump("resource uri = " + resource.Value + "\n");
					var messages_enumerator = dsubfolders[i].getMessages(msgwindow);
					var messages = nsISimpleEnumerator2JSArray(messages_enumerator, Components.interfaces.nsIMessage);

					for (var k=0; k<messages.length; k++) {
						if (messages[k].subject == time) {
							dump(k + " subject = " + messages[k].subject + "\n");
							dump(k + " Recepients = " + messages[k].recipients + "\n");
							supportsarray.AppendElement(messages[k]);
							dump("Added to Array \n");
							dsubfolders[i].deleteMessages(supportsarray, msgwindow, true, false);
							flag = 1;
						} //if
					}  //for

				} //if

			}  //for

		}  //if

		} //for
		if (flag == 1) {
			rv += "DeleteMsg:" + "\t" + "Passed" + "\n";
			dump("Writing Passed to file for DeleteMsg...\n");
			//document.write("<br>" + "deleteMsg: " + "\t" + "Passed" + "\n");
		}

		else {
			rv += "DeleteMsg:" + "\t" + "Failed" + "\n";
			dump("Writing Failed to file for DeleteMsg...\n");
			//document.write("<br>" + "deleteMsg: " + "\t" + "Failed" + "\n");
			if (final_result == "Passed")
				final_result = "Failed";
		}


	}  // try
	catch (e) {
		dump("E = " + e + "\n");
	}
} //fn



function getTimeStamp() {
	var day="";
	var month="";
	var myweekday="";
	var year="";
	var newdate = new Date();
	var mydate = new Date();
	var dston =  new Date('April 4, 1999 2:59:59');
	var dstoff = new Date('october 31, 1999 2:59:59');
	var myzone = newdate.getTimezoneOffset();
	var newtime = newdate.getTime();
	var zonea;
	var dst;
	var newtimea;
	var myday;
	var mymonth;
	var myminutes;
	var myyear;
	var myhours;
	var mytime;
	var weekday;
	var yhours;
	var mm;

	var zone = 8; 
//  references your time zone
	if((newdate > dston) && (newdate < dstoff))
	{
		zonea = zone - 1 ;
		dst = "  Pacific Daylight Savings Time";
	}
	else {
		zonea = zone ; 
		dst = "  Pacific Standard Time";
	}

	var newzone = (zonea*60*60*1000);
	newtimea = newtime+(myzone*60*1000)-newzone;
	mydate.setTime(newtimea);
	myday = mydate.getDay();
	mymonth = mydate.getMonth();
	myweekday= mydate.getDate();
	weekday= myweekday;
	var myyear= mydate.getYear();
	year = myyear;
	if (year <= 2000)    // Y2K Fix, Isaac Powell
		year = year + 1900; // http://onyx.idbsu.edu/~ipowell
	yhours = mydate.getHours();
	if (yhours > 12) {
		myhours = yhours - 12 ; mm = " PM";
	}
	else {
		myhours = yhours; mm = " AM";
	}
	myminutes = mydate.getMinutes();
	if (myminutes < 10){
		mytime = ":0" + myminutes;
	}
	else {
		mytime = ":" + myminutes;
	};

	var arday = new Array("Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday")
	var armonth = new Array("January ","February ","March ","April ","May ","June ","July ","August ","September ", "October ","November ","December ")
	var ardate = new Array("0th","1st","2nd","3rd","4th","5th","6th","7th","8th","9th","10th","11th","12th","13th","14th","15th","16th","17th","18th","19th","20th","21st","22nd","23rd","24th","25th","26th","27th","28th","29th","30th","31st");
	// rename locale as needed.

	time = ("In Mountain View, CA, it is: " + myhours + mytime+ mm + ", " + arday[myday] +", " + armonth[mymonth] +" "+ardate[myweekday] + ", " + year+", " + dst +".");
	return time;
}

function sendMessage()

{
	netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead"); 
	netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect"); 
	try {
	var msgComposeService = Components.classes["@mozilla.org/messengercompose;1"].getService();
	msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);

//	var msgCompose = null;

//	var MAX_RECIPIENTS = 0;

//	var numAttachments = 0;

//	var currentAttachment = null;

//	var other_header = "";

//	var update_compose_title_as_you_type = true;

	dump("IDENTITY = " + identity + "\n");
	var mycompose = msgComposeService.InitCompose(window, null, 0, 0, null,identity);
	mycompose.compFields.SetFrom(identity.email);
	mycompose.compFields.SetTo(identity.email);
	time = getTimeStamp();
	mycompose.compFields.SetSubject(time);
	mycompose.compFields.SetCharacterSet("ISO-8859-1");
	mycompose.compFields.SetBody("This is the body\n");
	mycompose.SendMsg(0, identity, null);
	dump("MSG SENT \n");
	dump("TIME = " + time + "\n");
	}  //try
	catch (e) {
		dump("E = " + e + "\n");
	}
}

function nsISupportsArray2JSArray(array, IID) {
    var result = new Array;
    var length = array.Count();
    for (var i=0; i<length; i++) {
      result[i] = array.GetElementAt(i).QueryInterface(IID);
    }
    return result;
}

function nsIEnumerator2JSArray(enumerator, iface) {
     var array = new Array;
     var i=0;
     var done = false;
     while (!done) {
         var element = enumerator.currentItem();
         array[i] = element.QueryInterface(iface);
         try {
            enumerator.next();
         } catch (ex) {
              done=true;
           }
                i++;
     }
     return array;
}

function nsISimpleEnumerator2JSArray(enumerator, iface) {
    var array = new Array;
    var i=0;
    while (enumerator.hasMoreElements()) {
       var element = enumerator.getNext();
       array[i] = element.QueryInterface(iface);
       i++;
    }

    return array;
}
