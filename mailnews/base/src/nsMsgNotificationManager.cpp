/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "msgCore.h"
#include "nsMsgNotificationManager.h"
#include "nsRDFCID.h"
#include "nsIRDFService.h"
#include "rdf.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
#include "MailNewsTypes.h"
#include "nsIMsgFolder.h" // TO include biffState enum. Change to bool later...
#include "nsMemory.h"
#include "nsXPIDLString.h"
#include "prprf.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,		NS_RDFINMEMORYDATASOURCE_CID); 
static NS_DEFINE_CID(kMsgMailSessionCID,					NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);


nsIRDFResource* nsMsgNotificationManager::kNC_FlashRoot = nsnull;
nsIRDFResource* nsMsgNotificationManager::kNC_Type = nsnull;
nsIRDFResource* nsMsgNotificationManager::kNC_Source = nsnull;
nsIRDFResource* nsMsgNotificationManager::kNC_Description = nsnull;
nsIRDFResource* nsMsgNotificationManager::kNC_TimeStamp = nsnull;
nsIRDFResource* nsMsgNotificationManager::kNC_URL = nsnull;
nsIRDFResource* nsMsgNotificationManager::kNC_Child = nsnull;

nsIRDFResource* nsMsgNotificationManager::kNC_NewMessages = nsnull;

nsIAtom * nsMsgNotificationManager::kBiffStateAtom  = nsnull;
nsIAtom * nsMsgNotificationManager::kNumNewBiffMessagesAtom = nsnull;

#define NC_RDF_FLASHROOT		"NC:FlashRoot"
#define NC_RDF_TYPE				"http://home.netscape.com/NC-rdf#type"
#define NC_RDF_SOURCE			"http://home.netscape.com/NC-rdf#source"
#define NC_RDF_DESCRIPTION		"http://home.netscape.com/NC-rdf#description"
#define NC_RDF_TIMESTAMP		"http://home.netscape.com/NC-rdf#timestamp"
#define NC_RDF_URL				"http://home.netscape.com/NC-rdf#url"
#define NC_RDF_CHILD			"http://home.netscape.com/NC-rdf#child"

#define NC_RDF_NEWMESSAGES		"http://home.netscape.com/NC-rdf#MsgNewMessages"



nsMsgNotificationManager::nsMsgNotificationManager()
{
	NS_INIT_REFCNT();

}

nsMsgNotificationManager::~nsMsgNotificationManager()
{
	NS_IF_RELEASE(kNC_FlashRoot);
	NS_IF_RELEASE(kNC_Type);
	NS_IF_RELEASE(kNC_Source);
	NS_IF_RELEASE(kNC_Description);
	NS_IF_RELEASE(kNC_TimeStamp);
	NS_IF_RELEASE(kNC_URL);
	NS_IF_RELEASE(kNC_Child);
	NS_IF_RELEASE(kNC_NewMessages);

    NS_IF_RELEASE(kNumNewBiffMessagesAtom);
    NS_IF_RELEASE(kBiffStateAtom);

}

NS_IMPL_ADDREF(nsMsgNotificationManager)
NS_IMPL_RELEASE(nsMsgNotificationManager);

NS_IMETHODIMP
nsMsgNotificationManager::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
	if (iid.Equals(kISupportsIID))
	{
		*result = NS_STATIC_CAST(nsISupports*, this);
	}
	else if(iid.Equals(NS_GET_IID(nsIFolderListener)))
	{
		*result = NS_STATIC_CAST(nsIFolderListener*, this);
	}
	else if(iid.Equals(NS_GET_IID(nsIRDFDataSource)))
	{
        // Support nsIRDFDataSource by aggregation.
		return mInMemoryDataSourceISupports->QueryInterface(iid, result);
	}

	if(*result)
	{
		NS_ADDREF_THIS();
		return NS_OK;
	}
    return NS_NOINTERFACE;
}

nsresult nsMsgNotificationManager::Init()
{
	nsresult rv;

	rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID, 
                                          this, 
                                          NS_GET_IID(nsISupports), 
                                          getter_AddRefs(mInMemoryDataSourceISupports));

	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIMsgMailSession> mailSession = 
	         do_GetService(kMsgMailSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		rv = mailSession->AddFolderListener(this, nsIFolderListener::propertyChanged | nsIFolderListener::propertyFlagChanged);

	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFService> rdfService(do_GetService(kRDFServiceCID, &rv)); 
	if(NS_FAILED(rv))
		return rv;

	if (! kNC_FlashRoot)
	{
	   rdfService->GetResource(NC_RDF_FLASHROOT,   &kNC_FlashRoot);
	   rdfService->GetResource(NC_RDF_TYPE,   &kNC_Type);
	   rdfService->GetResource(NC_RDF_SOURCE,   &kNC_Source);
	   rdfService->GetResource(NC_RDF_DESCRIPTION,   &kNC_Description);
	   rdfService->GetResource(NC_RDF_TIMESTAMP,   &kNC_TimeStamp);
	   rdfService->GetResource(NC_RDF_URL,   &kNC_URL);
	   rdfService->GetResource(NC_RDF_CHILD,   &kNC_Child);
	   rdfService->GetResource(NC_RDF_NEWMESSAGES,   &kNC_NewMessages);

       kNumNewBiffMessagesAtom = NS_NewAtom("NumNewBiffMessages");
       kBiffStateAtom          = NS_NewAtom("BiffState");
	}
	return rv;

}

NS_IMETHODIMP nsMsgNotificationManager::OnItemAdded(nsISupports *parentItem, nsISupports *item, const char* viewString)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgNotificationManager::OnItemRemoved(nsISupports *parentItem, nsISupports *item, const char* viewString)
{
	return NS_OK;
}

NS_IMETHODIMP
nsMsgNotificationManager::OnItemPropertyChanged(nsISupports *item,
                                                nsIAtom *property,
                                                const char *oldValue,
                                                const char *newValue)

{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(item));
	if(folder)
	{
		if(kNumNewBiffMessagesAtom ==  property)
		{
			PRUint32 biffState;
			rv = folder->GetBiffState(&biffState);
			if(NS_SUCCEEDED(rv) && (biffState == nsIMsgFolder::nsMsgBiffState_NewMail))
			{
				rv = AddNewMailNotification(folder);
			}
		}
	}
	return rv;
}

NS_IMETHODIMP
nsMsgNotificationManager::OnItemUnicharPropertyChanged(nsISupports *item,
                                                       nsIAtom *property,
                                                       const PRUnichar* oldValue,
                                                       const PRUnichar* newValue)
{

	return NS_OK;
}


NS_IMETHODIMP
nsMsgNotificationManager::OnItemIntPropertyChanged(nsISupports *item,
                                                   nsIAtom *property,
                                                   PRInt32 oldValue,
                                                   PRInt32 newValue)
{

	return NS_OK;
}

NS_IMETHODIMP
nsMsgNotificationManager::OnItemBoolPropertyChanged(nsISupports *item,
                                                    nsIAtom *property,
                                                    PRBool oldValue,
                                                    PRBool newValue)
{

	return NS_OK;
}

NS_IMETHODIMP
nsMsgNotificationManager::OnItemPropertyFlagChanged(nsISupports *item,
                                                    nsIAtom *property,
                                                    PRUint32 oldFlag,
                                                    PRUint32 newFlag)

{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(item));
	if(folder)
	{
		if(kBiffStateAtom ==  property)
		{
			if(newFlag == nsIMsgFolder::nsMsgBiffState_NewMail)
			{
				rv = AddNewMailNotification(folder);
			}
			else
			{
				rv = RemoveNewMailNotification(folder);
			}
		}
	}
	return rv;
}

NS_IMETHODIMP
nsMsgNotificationManager::OnItemEvent(nsIFolder *folder, nsIAtom *aEvent)
{
  return NS_OK;
}
  

nsresult nsMsgNotificationManager::AddNewMailNotification(nsIMsgFolder *folder)
{
	nsresult rv;
	nsCOMPtr<nsIRDFService> rdfService(do_GetService(kRDFServiceCID, &rv)); 
	if(NS_FAILED(rv))
		return rv;

	//If there's currently a notification for this folder, remove it.
	RemoveNewMailNotification(folder);

	nsCAutoString newMailURI;
	rv = BuildNewMailURI(folder, newMailURI);
	if(NS_FAILED(rv))
		return rv;


	nsCOMPtr<nsIRDFResource> notificationResource;
	rv = rdfService->GetResource((const char *) newMailURI, getter_AddRefs(notificationResource));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFLiteral> type, source, description, timeStamp, url;
	nsString typeString, sourceString, descriptionString, timeStampString, urlString;

	sourceString.AssignWithConversion("Messenger");
	descriptionString.AssignWithConversion("You have mail");
	timeStampString.AssignWithConversion("3:33pm");
	urlString.AssignWithConversion("test");

    nsCOMPtr<nsIRDFDataSource> ds = do_QueryInterface(mInMemoryDataSourceISupports);

	ds->Assert(notificationResource, kNC_Type, kNC_NewMessages, PR_TRUE);

	PRUnichar* folderDescription = nsnull;
	rv = folder->GetNewMessagesNotificationDescription(&folderDescription);
	if(NS_SUCCEEDED(rv) && folderDescription)
	{
		sourceString = folderDescription;
		nsMemory::Free(folderDescription);
	}
	rv = rdfService->GetLiteral(sourceString.get(), getter_AddRefs(source));
	if(NS_SUCCEEDED(rv))
	{
		ds->Assert(notificationResource, kNC_Source, source, PR_TRUE);
	}

	PRInt32 newMessages;
	rv = folder->GetNumNewMessages(&newMessages);
	if(NS_SUCCEEDED(rv))
	{
		char *str = PR_smprintf("%d new %s", newMessages, (newMessages == 1) ? "message" : "messages");
		descriptionString.AssignWithConversion(str);
		PR_smprintf_free(str);
	}
	// if flash panel is going to ignore the source, I'm going to add it to the description for now
	nsXPIDLString folderName;
	rv = folder->GetPrettyName(getter_Copies(folderName));
	if (NS_SUCCEEDED(rv) && folderName)
	{
		descriptionString.AppendWithConversion(" in ");
		descriptionString.Append(folderName);
	}

	rv = rdfService->GetLiteral(descriptionString.get(), getter_AddRefs(description));
	if(NS_SUCCEEDED(rv))
	{
		ds->Assert(notificationResource, kNC_Description, description, PR_TRUE);
	}

	//Supposedly rdf will convert this into a localized time string.
	PRExplodedTime explode;
	PR_ExplodeTime( PR_Now(), PR_LocalTimeParameters, &explode);
	char buffer[128];
	PR_FormatTime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M %p", &explode);
	timeStampString.AssignWithConversion(buffer);
	
	rv = rdfService->GetLiteral(timeStampString.get(), getter_AddRefs(timeStamp));
	if(NS_SUCCEEDED(rv))
	{
		ds->Assert(notificationResource, kNC_TimeStamp, timeStamp, PR_TRUE);
	}

	rv = rdfService->GetLiteral(urlString.get(), getter_AddRefs(url));
	if(NS_SUCCEEDED(rv))
	{
		ds->Assert(notificationResource, kNC_URL, url, PR_TRUE);
	}

	ds->Assert(kNC_FlashRoot, kNC_Child, notificationResource, PR_TRUE);
	return NS_OK;
}

nsresult nsMsgNotificationManager::RemoveNewMailNotification(nsIMsgFolder *folder)
{
	nsresult rv;
	nsCOMPtr<nsIRDFService> rdfService(do_GetService(kRDFServiceCID, &rv)); 
	if(NS_FAILED(rv))
		return rv;

	nsCAutoString newMailURI;
	rv = BuildNewMailURI(folder, newMailURI);
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFResource> notificationResource;
	rv = rdfService->GetResource(newMailURI, getter_AddRefs(notificationResource));
	if(NS_FAILED(rv))
		return rv;
	RemoveOldValues(notificationResource);

    nsCOMPtr<nsIRDFDataSource> ds = do_QueryInterface(mInMemoryDataSourceISupports);

	ds->Unassert(kNC_FlashRoot, kNC_Child, notificationResource);

	return NS_OK;
}

nsresult nsMsgNotificationManager::RemoveOldValues(nsIRDFResource *notificationResource)
{
	nsCOMPtr<nsIRDFNode> target;
	nsresult rv;

    nsCOMPtr<nsIRDFDataSource> ds = do_QueryInterface(mInMemoryDataSourceISupports);

	rv = ds->GetTarget(notificationResource, kNC_Description, PR_TRUE, getter_AddRefs(target));
	if(NS_SUCCEEDED(rv) && (rv != NS_RDF_NO_VALUE))
		ds->Unassert(notificationResource, kNC_Description, target);

	rv = ds->GetTarget(notificationResource, kNC_Type, PR_TRUE, getter_AddRefs(target));
	if(NS_SUCCEEDED(rv) && (rv != NS_RDF_NO_VALUE))
		ds->Unassert(notificationResource, kNC_Type, target);

	rv = ds->GetTarget(notificationResource, kNC_Source, PR_TRUE, getter_AddRefs(target));
	if(NS_SUCCEEDED(rv) && (rv != NS_RDF_NO_VALUE))
		ds->Unassert(notificationResource, kNC_Source, target);

	rv = ds->GetTarget(notificationResource, kNC_TimeStamp, PR_TRUE, getter_AddRefs(target));
	if(NS_SUCCEEDED(rv) && (rv != NS_RDF_NO_VALUE))
		ds->Unassert(notificationResource, kNC_TimeStamp, target);

	rv = ds->GetTarget(notificationResource, kNC_URL, PR_TRUE, getter_AddRefs(target));
	if(NS_SUCCEEDED(rv) && (rv != NS_RDF_NO_VALUE))
		ds->Unassert(notificationResource, kNC_URL, target);

	return NS_OK;

}

nsresult nsMsgNotificationManager::BuildNewMailURI(nsIMsgFolder *folder, nsCAutoString &newMailURI)
{
	nsresult rv;
	nsCOMPtr<nsIRDFResource> folderResource = do_QueryInterface(folder);
	if(!folderResource)
		return NS_ERROR_NO_INTERFACE;

	char *folderURI;
	rv = folderResource->GetValue(&folderURI);
	if(!(NS_SUCCEEDED(rv) && folderURI))
		return rv;

	newMailURI = "newmail:";
	newMailURI += folderURI;
	nsMemory::Free(folderURI);
	return NS_OK;
}
