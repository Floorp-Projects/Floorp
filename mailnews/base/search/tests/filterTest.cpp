/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

///////////////////////////////////////////////////////////////////////////////
// This is at test harness for mail filters.
//
// For command (1) parsing a filter file,
//		You are prompted for the name of a filter file you wish to parse
//		CURRENTLY this file name must be in the form of a file url:
//		i.e. "D|/mozilla/MailboxFile". 
#include "msgCore.h"
#include "prprf.h"

#include <stdio.h>
#include <assert.h>

#ifdef XP_PC
#include <windows.h>
#endif

#include "nsFileSpec.h"
#include "nsIPref.h"
#include "plstr.h"
#include "plevent.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsString.h"

#include "nsXPComCIID.h"
#include "nsMsgFilterService.h"
#include "nsIMsgFilterList.h"

#include "nsCOMPtr.h"

#include "nsMsgBaseCID.h"

#include "nsFileStream.h"

#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsRDFCID.h"
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);

/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kMsgFilterServiceCID, NS_MSGFILTERSERVICE_CID);

/////////////////////////////////////////////////////////////////////////////////
// Define default values to be used to drive the test
/////////////////////////////////////////////////////////////////////////////////

/* strip out non-printable characters */
static void strip_nonprintable(char *string) {
    char *dest, *src;

    dest=src=string;
    while (*src) {
        if (isprint(*src)) {
            (*src)=(*dest);
            src++; dest++;
        } else {
            src++;
        }
    }
    (*dest)='\0';
}

//////////////////////////////////////////////////////////////////////////////////
// The nsFilterTestDriver is a class that I envision could be generalized to form the
// building block of a protocol test harness. To configure it, you would list all of
// the events you know how to handle and when one of those events is triggered, you
// would be asked to process it....right now it is just Mailbox specific....
///////////////////////////////////////////////////////////////////////////////////

class nsFilterTestDriver
{
public:

	nsFilterTestDriver(nsIMsgFilterService *);
	virtual ~nsFilterTestDriver();

	// run driver initializes the instance, lists the commands, runs the command and when
	// the command is finished, it reads in the next command and continues...theoretically,
	// the client should only ever have to call RunDriver(). It should do the rest of the 
	// work....
	nsresult RunDriver(); 

	// User drive commands
	void InitializeTestDriver(); // will end up prompting the user for things like host, port, etc.
	nsresult ListCommands();   // will list all available commands to the user...i.e. "get groups, get article, etc."
	nsresult ReadAndDispatchCommand(); // reads a command number in from the user and calls the appropriate command generator
	nsresult PromptForUserData(const char * userPrompt);

	nsresult OnOpenFilterFile();
	nsresult OnExit(); 

	nsresult OnWriteFilterList();

protected:
	char m_userData[500];	// generic string buffer for storing the current user entered data...

	nsFileSpec	m_folderSpec;

	PRBool	    m_runTestHarness;
	nsCOMPtr<nsIMsgFilterService>	m_filterService;
};

nsFilterTestDriver::nsFilterTestDriver(nsIMsgFilterService *filterService) : m_folderSpec("")
{
	m_runTestHarness = PR_TRUE;
	m_filterService = filterService;
	InitializeTestDriver(); // prompts user for initialization information...

}

nsFilterTestDriver::~nsFilterTestDriver()
{
}

nsresult nsFilterTestDriver::RunDriver()
{
	nsresult status = NS_OK;

	while (m_runTestHarness)
	{
		status = ReadAndDispatchCommand();

	} // until the user has asked us to stop

	return status;
}

void nsFilterTestDriver::InitializeTestDriver()
{
}

// prints the userPrompt and then reads in the user data. Assumes urlData has already been allocated.
// it also reconstructs the url string in m_urlString but does NOT reload it....
nsresult nsFilterTestDriver::PromptForUserData(const char * userPrompt)
{
	char tempBuffer[500];
	tempBuffer[0] = '\0'; 

	if (userPrompt)
		printf(userPrompt);
	else
		printf("Enter data for command: ");

    fgets(tempBuffer, sizeof(tempBuffer), stdin);
    strip_nonprintable(tempBuffer);
	// only replace m_userData if the user actually entered a valid line...
	// this allows the command function to set a default value on m_userData before
	// calling this routine....
	if (tempBuffer && *tempBuffer)
		PL_strcpy(m_userData, tempBuffer);

	return NS_OK;
}

nsresult nsFilterTestDriver::ReadAndDispatchCommand()
{
	nsresult status = NS_OK;
	PRInt32 command = 0; 
	char commandString[5];
	commandString[0] = '\0';

	printf("Enter command number: ");
    fgets(commandString, sizeof(commandString), stdin);
    strip_nonprintable(commandString);
	if (commandString && *commandString)
	{
		command = atoi(commandString);
	}

	// now switch on command to the appropriate 
	switch (command)
	{
	case 0:
		status = ListCommands();
		break;
	case 1:
		status = OnOpenFilterFile();
		break;
	case 2:
		status = OnWriteFilterList();
		break;
	default:
		status = OnExit();
		break;
	}

	return status;
}

nsresult nsFilterTestDriver::ListCommands()
{
	printf("Commands currently available: \n");
	printf("0) List commands. \n");
	printf("1) Open a filter file. \n");
	printf("2) Output a filter file. \n");
	printf("9) Exit the test application. \n");
	return NS_OK;
}

nsresult nsFilterTestDriver::OnExit()
{
	printf("Terminating Mailbox test harness....\n");
	m_runTestHarness = PR_FALSE; // next time through the test driver loop, we'll kick out....
	return NS_OK;
}


nsresult nsFilterTestDriver::OnOpenFilterFile()
{
	nsCOMPtr <nsIMsgFilterList> filterList;

	PromptForUserData("enter filter file name: ");
	nsFileSpec filterFile(m_userData);

	if (filterFile.Exists())
	{
		nsresult res = m_filterService->OpenFilterList(&filterFile, nsnull, getter_AddRefs(filterList));
		if (NS_SUCCEEDED(res))
		{
		}
	}
	else
		printf("filter file doesn't exist\n");
	return NS_OK;
}

nsresult nsFilterTestDriver::OnWriteFilterList()
{
	nsCOMPtr <nsIMsgFilterList> filterList;

	PromptForUserData("enter filter file name: ");
	nsFileSpec filterFile(m_userData);

	if (filterFile.Exists())
	{
		nsresult res = m_filterService->OpenFilterList(&filterFile, nsnull, getter_AddRefs(filterList));
		if (NS_SUCCEEDED(res))
		{
			filterFile.MakeUnique();
			nsIOFileStream fileStream(filterFile);

			return filterList->SaveToFile(&fileStream);
		}

	}
	return NS_OK;
}

int main()
{
    nsresult rv = NS_OK;

	nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
	nsCOMPtr<nsIMsgFilterService> filterService = 
	         do_GetService(kMsgFilterServiceCID, &rv);
	if (NS_FAILED(rv)) return rv;


	// okay, everything is set up, now we just need to create a test driver and run it...
	nsFilterTestDriver * driver = new nsFilterTestDriver(filterService);
	if (driver)
	{
		driver->RunDriver();
		// when it kicks out...it is done....so delete it...
		delete driver;
	}

	// shut down:
    
    return 0;
}
