/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"    // precompiled header...

#include "nsMSGFolderDataSource.h"
#include "prlog.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "rdf.h"
#include "nsIRDFResourceFactory.h"
#include "nsIRDFObserver.h"
#include "nsIRDFNode.h"
#include "plstr.h"
#include "prmem.h"
#include "prio.h"
#include "prprf.h"
#include "nsString.h"
#include "nsIMsgFolder.h"
#include "nsISupportsArray.h"
#include "nsFileSpec.h"
#include "nsMsgFolderFlags.h"
#include "nsRDFCursorUtils.h"
#include "nsIMessage.h"
#include "nsMsgFolder.h"
#include "nsIMsgRFC822Parser.h"
#include "nsMsgBaseCID.h"
#include "nsCOMPtr.h"
//This is a temporary dependency.  I need this for parsemessageURI.  In the future
//we should have the nsIMessage have the ability to get the folder from it.
#include "nsLocalMailFolder.h"

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,    NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kMsgRFC822ParserCID,			NS_MSGRFC822PARSER_CID); 

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRDFCursorIID, NS_IRDFCURSOR_IID);

nsIRDFResource* nsMSGFolderDataSource::kNC_Child;
nsIRDFResource* nsMSGFolderDataSource::kNC_MessageChild;
nsIRDFResource* nsMSGFolderDataSource::kNC_Folder;
nsIRDFResource* nsMSGFolderDataSource::kNC_Name;
nsIRDFResource* nsMSGFolderDataSource::kNC_MSGFolderRoot;

nsIRDFResource* nsMSGFolderDataSource::kNC_Subject;
nsIRDFResource* nsMSGFolderDataSource::kNC_Sender;
nsIRDFResource* nsMSGFolderDataSource::kNC_Date;

// commands
nsIRDFResource* nsMSGFolderDataSource::kNC_Delete;
nsIRDFResource* nsMSGFolderDataSource::kNC_Reply;
nsIRDFResource* nsMSGFolderDataSource::kNC_Forward;

static const char kURINC_MSGFolderRoot[]  = "mailbox:/";

#define NC_NAMESPACE_URI "http://home.netscape.com/NC-rdf#"
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, MessageChild);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Folder);

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Subject);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Sender);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Date);

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Delete);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Reply);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Forward);

////////////////////////////////////////////////////////////////////////
// The RDF service manager. Cached in the address book data source's
// constructor

static nsIRDFService* gRDFService = nsnull;

////////////////////////////////////////////////////////////////////////
// Utilities

static PRBool
peq(nsIRDFResource* r1, nsIRDFResource* r2)
{
  PRBool result;
  if (NS_SUCCEEDED(r1->EqualsResource(r2, &result)) && result) {
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }
}

static void createNode(nsString& str, nsIRDFNode **node)
{
  nsIRDFLiteral * value;
  *node = nsnull;
  if(NS_SUCCEEDED(gRDFService->GetLiteral((const PRUnichar*)str, &value))) {
    *node = value;
  }
}

//Helper function to find the name of a folder from the given pathname.
static const char* NameFromPathname(const char* pathname) 
{
  char* ptr = PL_strrchr(pathname, '/');
  if (ptr) 
    return ptr + 1;
  return pathname;
}

static PRBool ShouldIgnoreFile (const char *name)
{
  if (name[0] == '.' || name[0] == '#' || name[PL_strlen(name) - 1] == '~')
    return PR_TRUE;

  if (!PL_strcasecmp (name, "rules.dat"))
    return PR_TRUE;

#if defined (XP_PC) || defined (XP_MAC) 
  // don't add summary files to the list of folders;
  //don't add popstate files to the list either, or rules (sort.dat). 
  if ((PL_strlen(name) > 4 &&
      !PL_strcasecmp(name + PL_strlen(name) - 4, ".snm")) ||
      !PL_strcasecmp(name, "popstate.dat") ||
      !PL_strcasecmp(name, "sort.dat") ||
      !PL_strcasecmp(name, "mailfilt.log") ||
      !PL_strcasecmp(name, "filters.js") ||
      !PL_strcasecmp(name + PL_strlen(name) - 4, ".toc"))
      return PR_TRUE;
#endif

  return PR_FALSE;
}

nsMSGFolderDataSource::nsMSGFolderDataSource()
{
  NS_INIT_REFCNT();

  mURI = nsnull;
  mInitialized = PR_FALSE;  
  mObservers = nsnull;

  nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                             nsIRDFService::GetIID(),
                                             (nsISupports**) &gRDFService); // XXX probably need shutdown listener here

  PR_ASSERT(NS_SUCCEEDED(rv));
}

nsMSGFolderDataSource::~nsMSGFolderDataSource (void)
{
  gRDFService->UnregisterDataSource(this);

  PL_strfree(mURI);
  if (mObservers) {
      for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
          nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
          NS_RELEASE(obs);
      }
      delete mObservers;
  }
  nsrefcnt refcnt;
  NS_RELEASE2(kNC_Child, refcnt);
  NS_RELEASE2(kNC_MessageChild, refcnt);
  NS_RELEASE2(kNC_Folder, refcnt);
  NS_RELEASE2(kNC_Name, refcnt);
  NS_RELEASE2(kNC_MSGFolderRoot, refcnt);

  NS_RELEASE2(kNC_Subject, refcnt);
  NS_RELEASE2(kNC_Sender, refcnt);
  NS_RELEASE2(kNC_Date, refcnt);

  NS_RELEASE2(kNC_Delete, refcnt);
  NS_RELEASE2(kNC_Reply, refcnt);
  NS_RELEASE2(kNC_Forward, refcnt);

  nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService); // XXX probably need shutdown listener here
  gRDFService = nsnull;
}


NS_IMPL_ADDREF(nsMSGFolderDataSource)
NS_IMPL_RELEASE(nsMSGFolderDataSource)

NS_IMETHODIMP
nsMSGFolderDataSource::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

  *result = nsnull;
  if (iid.Equals(nsIRDFMSGFolderDataSource::GetIID()) ||
    iid.Equals(nsIRDFDataSource::GetIID()) ||
      iid.Equals(kISupportsIID))
  {
    *result = NS_STATIC_CAST(nsIRDFMSGFolderDataSource*, this);
    AddRef();
    return NS_OK;
  }
	else if(iid.Equals(nsIFolderListener::GetIID()))
	{
    *result = NS_STATIC_CAST(nsIFolderListener*, this);
    AddRef();
    return NS_OK;
	}
  return NS_NOINTERFACE;
}

 // nsIRDFDataSource methods
NS_IMETHODIMP nsMSGFolderDataSource::Init(const char* uri)
{
  if (mInitialized)
      return NS_ERROR_ALREADY_INITIALIZED;

  if ((mURI = PL_strdup(uri)) == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;

  gRDFService->RegisterDataSource(this);

  if (! kNC_Child) {
    gRDFService->GetResource(kURINC_child,   &kNC_Child);
    gRDFService->GetResource(kURINC_MessageChild,   &kNC_MessageChild);
    gRDFService->GetResource(kURINC_Folder,  &kNC_Folder);
    gRDFService->GetResource(kURINC_Name,    &kNC_Name);
    gRDFService->GetResource(kURINC_MSGFolderRoot, &kNC_MSGFolderRoot);

    gRDFService->GetResource(kURINC_Subject, &kNC_Subject);
    gRDFService->GetResource(kURINC_Sender, &kNC_Sender);
    gRDFService->GetResource(kURINC_Date, &kNC_Date);

    gRDFService->GetResource(kURINC_Delete, &kNC_Delete);
    gRDFService->GetResource(kURINC_Reply, &kNC_Reply);
    gRDFService->GetResource(kURINC_Forward, &kNC_Forward);
  }
#if 0
  //create the folder for the root folder
  nsresult rv;
  nsIMsgFolder *rootFolder;
#if 0
  if(NS_SUCCEEDED(kNC_MSGFolderRoot->QueryInterface(nsIMsgFolder::GetIID(), (void**)&rootFolder)))
  {    
    if(rootFolder)
    {
      rootFolder->SetName("Mail and News");
      rootFolder->SetDepth(0);
      nsNativeFileSpec startPath("c:\\program files\\netscape\\users\\putterman\\Mail", PR_FALSE);
      if (NS_FAILED(rv = InitLocalFolders(rootFolder, startPath, 1)))
        return rv;

      NS_RELEASE(rootFolder);
    }
  }
#else
  if (NS_FAILED(rv = nsMsgFolder::GetRoot(&rootFolder)))
    return rv;
#endif
#endif
  mInitialized = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsMSGFolderDataSource::GetURI(const char* *uri) const
{
  *uri = mURI;
  return NS_OK;
}

NS_IMETHODIMP nsMSGFolderDataSource::GetSource(nsIRDFResource* property,
                                               nsIRDFNode* target,
                                               PRBool tv,
                                               nsIRDFResource** source /* out */)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMSGFolderDataSource::GetTarget(nsIRDFResource* source,
                                               nsIRDFResource* property,
                                               PRBool tv,
                                               nsIRDFNode** target)
{
  nsresult rv = NS_ERROR_RDF_NO_VALUE;

  // we only have positive assertions in the mail data source.
  if (! tv)
    return NS_ERROR_RDF_NO_VALUE;


  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv)) {
    if (peq(kNC_Name, property)) {
      char *name;
      rv = folder->GetName(&name);
      if (NS_FAILED(rv)) return rv;
      nsString nameString(name);
      createNode(nameString, target);
      PR_FREEIF(name);
      return rv;
    }
#if 1
    if (peq(kNC_Child, property)) {

      nsIEnumerator* subFolders;
      rv = folder->GetSubFolders(&subFolders);
      if (NS_FAILED(rv)) return rv;

      rv = subFolders->First();
      if (NS_SUCCEEDED(rv)) {
        nsISupports *firstFolder;
        rv = subFolders->CurrentItem(&firstFolder);
        if (NS_SUCCEEDED(rv)) {
          firstFolder->QueryInterface(nsIRDFResource::GetIID(), (void**)target);
        }
        NS_RELEASE(firstFolder);
      }
      NS_RELEASE(subFolders);
      return rv;
    }
    
    if (peq(kNC_MessageChild, property)){
      nsIEnumerator* messages;
      rv = folder->GetMessages(&messages);
      if (NS_SUCCEEDED(rv)) {
        if (NS_SUCCEEDED(messages->First())) {
          nsISupports *firstMessage;
          rv = messages->CurrentItem(&firstMessage);
          if (NS_SUCCEEDED(rv)) {
            *target = NS_STATIC_CAST(nsIRDFResource*, firstMessage);
          }
        }
        NS_RELEASE(messages);
      }
      return rv;
    }
#endif
    return rv;
  }

  nsCOMPtr<nsIMessage> message(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv)) {
    if (peq(kNC_Name, property) ||
        peq(kNC_Subject, property)) {
      nsAutoString subject;
      rv = message->GetProperty("subject", subject);
      createNode(subject, target);
    }
    else if (peq(kNC_Sender, property))
    {
      nsAutoString sender, senderUserName;
      rv = message->GetProperty("sender", sender);
			if(NS_SUCCEEDED(rv = GetSenderName(sender, &senderUserName)))
				createNode(senderUserName, target);
    }
    else if (peq(kNC_Date, property))
    {
      nsAutoString date;
      rv = message->GetProperty("date", date);
			PRInt32 error;
			time_t time = date.ToInteger(&error, 16);
			struct tm* tmTime = localtime(&time);
			char dateBuf[100];
			strftime(dateBuf, 100, "%m/%d/%y %I:%M %p", tmTime);
			date = dateBuf;
		  createNode(date, target);
    }
    return rv;
  }
	
  return rv;
}

//sender is the string we need to parse.  senderuserName is the parsed user name we get back.
nsresult nsMSGFolderDataSource::GetSenderName(nsAutoString& sender, nsAutoString *senderUserName)
{
	//XXXOnce we get the csid, use Intl version
	nsIMsgRFC822Parser *parser; 
	nsresult rv = NS_OK;
	if(NS_SUCCEEDED(rv = nsComponentManager::CreateInstance(kMsgRFC822ParserCID, 
													NULL, 
													nsIMsgRFC822Parser::GetIID(), 
													(void **) &parser)))
	{
		char *name;
		char *senderStr = sender.ToNewCString();
		if(NS_SUCCEEDED(rv = parser->ExtractRFC822AddressName (senderStr, &name)))
		{
			*senderUserName = name;
		}
		if(name)
			PL_strfree(name);
		if(senderStr)
			delete[] senderStr;
		NS_RELEASE(parser);
	}
	return rv;
}

NS_IMETHODIMP nsMSGFolderDataSource::GetSources(nsIRDFResource* property,
                                                nsIRDFNode* target,
                                                PRBool tv,
                                                nsIRDFAssertionCursor** sources)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMSGFolderDataSource::GetTargets(nsIRDFResource* source,
                                                nsIRDFResource* property,    
                                                PRBool tv,
                                                nsIRDFAssertionCursor** targets)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsIMsgFolder* folder;
  nsIMessage* message;
  if (NS_SUCCEEDED(source->QueryInterface(nsIMsgFolder::GetIID(), (void**)&folder)))
  {
    if (peq(kNC_Child, property))
    {
      nsIEnumerator *subFolders;

      rv = folder->GetSubFolders(&subFolders);
      if (NS_FAILED(rv)) return rv;
      //folder->GetMessages(&subFolders);
      nsRDFEnumeratorAssertionCursor* cursor =
        new nsRDFEnumeratorAssertionCursor(this, source, kNC_Child, subFolders);
      NS_IF_RELEASE(subFolders);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
      rv = NS_OK;
    }
    else if (peq(kNC_MessageChild, property))
    {
      nsIEnumerator *messages;

      rv = folder->GetMessages(&messages);
      if (NS_FAILED(rv)) return rv;
      nsRDFEnumeratorAssertionCursor* cursor =
        new nsRDFEnumeratorAssertionCursor(this, source, kNC_MessageChild, messages);
      NS_IF_RELEASE(messages);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
      rv = NS_OK;
    }
    else if(peq(kNC_Name, property) || peq(kNC_Sender, property))
    {
      nsRDFSingletonAssertionCursor* cursor =
        new nsRDFSingletonAssertionCursor(this, source, property);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
      rv = NS_OK;
    }
    NS_IF_RELEASE(folder);
  }
  else if (NS_SUCCEEDED(source->QueryInterface(nsIMessage::GetIID(), (void**)&message))) {
    if(peq(kNC_Name, property) || peq(kNC_Subject, property) || peq(kNC_Date, property))
    {
      nsRDFSingletonAssertionCursor* cursor =
        new nsRDFSingletonAssertionCursor(this, source, property, PR_FALSE);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
      rv = NS_OK;
    }
    NS_IF_RELEASE(message);
  }
  return rv;
}

NS_IMETHODIMP nsMSGFolderDataSource::Assert(nsIRDFResource* source,
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMSGFolderDataSource::Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMSGFolderDataSource::HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion)
{
  *hasAssertion = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMSGFolderDataSource::AddObserver(nsIRDFObserver* n)
{
  if (! mObservers) {
    if ((mObservers = new nsVoidArray()) == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  mObservers->AppendElement(n);
  return NS_OK;}

NS_IMETHODIMP nsMSGFolderDataSource::RemoveObserver(nsIRDFObserver* n)
{
  if (! mObservers)
    return NS_OK;
  mObservers->RemoveElement(n);
  return NS_OK;
}

nsresult nsMSGFolderDataSource::NotifyObservers(nsIRDFResource *subject, nsIRDFResource *property,
												nsIRDFNode *object, PRBool assert)
{
	if(mObservers)
	{
		PRInt32 numObservers = mObservers->Count();
		nsIRDFObserver *observer = nsnull;
		for(PRInt32 i = 0; i < numObservers; i++)
		{
			//Get each observer and tell it an assert or unassert happened.
			observer = (nsIRDFObserver*)mObservers->ElementAt(i);
			if(observer)
			{
				if(assert)
				{
					observer->OnAssert(subject, property, object);
				}
				else
				{
					observer->OnUnassert(subject, property, object);
				}
			}
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsMSGFolderDataSource::ArcLabelsIn(nsIRDFNode* node,
                                                 nsIRDFArcsInCursor** labels)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMSGFolderDataSource::ArcLabelsOut(nsIRDFResource* source,
                                                  nsIRDFArcsOutCursor** labels)
{
  nsISupportsArray *arcs;
  NS_NewISupportsArray(&arcs);
  if (arcs == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsIMsgFolder* folder;
  nsIMessage* message;
  if (NS_SUCCEEDED(source->QueryInterface(nsIMsgFolder::GetIID(), (void**)&folder)))
  {
    arcs->AppendElement(kNC_Name);
#if 1
    nsIEnumerator* subFolders;
    if(NS_SUCCEEDED(folder->GetSubFolders(&subFolders)))
	{
	    if(NS_SUCCEEDED(subFolders->First()))
		  arcs->AppendElement(kNC_Child);
		NS_RELEASE(subFolders);
	}

    nsIEnumerator* messages;
    if(NS_SUCCEEDED(folder->GetMessages(&messages)))
	{
		if(NS_SUCCEEDED(messages->First()))
		  arcs->AppendElement(kNC_MessageChild);
		NS_RELEASE(messages);
	}
#else
    arcs->AppendElement(kNC_Child);
    arcs->AppendElement(kNC_MessageChild);
#endif
    NS_IF_RELEASE(folder);
  }
  else if (NS_SUCCEEDED(source->QueryInterface(nsIMessage::GetIID(), (void**)&message)))
  {
    arcs->AppendElement(kNC_Subject);
    arcs->AppendElement(kNC_Sender);
    arcs->AppendElement(kNC_Date);
    NS_IF_RELEASE(message);
  }

  nsRDFArrayArcsOutCursor* cursor =
    new nsRDFArrayArcsOutCursor(this, source, arcs);
  NS_RELEASE(arcs);
  if (cursor == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(cursor);
  *labels = cursor;
  return NS_OK;
}

NS_IMETHODIMP
nsMSGFolderDataSource::GetAllResources(nsIRDFResourceCursor** aCursor)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMSGFolderDataSource::Flush()
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMSGFolderDataSource::GetAllCommands(nsIRDFResource* source,
                                      nsIEnumerator/*<nsIRDFResource>*/** commands)
{
  nsresult rv;

  nsIMsgFolder* folder;
  nsIMessage* message;
  nsISupportsArray* cmds = nsnull;

  if (NS_SUCCEEDED(source->QueryInterface(nsIMsgFolder::GetIID(), (void**)&folder))) {
    NS_RELEASE(folder);       // release now that we know it's a folder
    rv = NS_NewISupportsArray(&cmds);
    if (NS_FAILED(rv)) return rv;
    cmds->AppendElement(kNC_Delete);
  }
  else if (NS_SUCCEEDED(source->QueryInterface(nsIMessage::GetIID(), (void**)&message))) {
    NS_RELEASE(message);       // release now that we know it's a message
    rv = NS_NewISupportsArray(&cmds);
    if (NS_FAILED(rv)) return rv;
    cmds->AppendElement(kNC_Delete);
    cmds->AppendElement(kNC_Reply);
    cmds->AppendElement(kNC_Forward);
  }

  if (cmds != nsnull)
    return cmds->Enumerate(commands);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMSGFolderDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                        nsIRDFResource*   aCommand,
                                        nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
  nsIMsgFolder* folder;
  nsIMessage* message;

  PRUint32 cnt = aSources->Count();
  for (PRUint32 i = 0; i < cnt; i++) {
    nsCOMPtr<nsISupports> source = dont_QueryInterface((*aSources)[i]);
    if (NS_SUCCEEDED(source->QueryInterface(nsIMsgFolder::GetIID(), (void**)&folder))) {
      NS_RELEASE(folder);       // release now that we know it's a folder

      // we don't care about the arguments -- folder commands are always enabled
      if (!(peq(aCommand, kNC_Delete)))
        return NS_COMFALSE;
    }
    else if (NS_SUCCEEDED(source->QueryInterface(nsIMessage::GetIID(), (void**)&message))) {
      NS_RELEASE(message);       // release now that we know it's a message

      // we don't care about the arguments -- message commands are always enabled
      if (!(peq(aCommand, kNC_Delete) ||
            peq(aCommand, kNC_Reply) ||
            peq(aCommand, kNC_Forward)))
        return NS_COMFALSE;
    }
  }
  return NS_OK; // succeeded for all sources
}

NS_IMETHODIMP
nsMSGFolderDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                 nsIRDFResource*   aCommand,
                                 nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
  nsresult rv = NS_OK;

  // XXX need to handle batching of command applied to all sources

  PRUint32 cnt = aSources->Count();
  for (PRUint32 i = 0; i < cnt; i++) {
    nsISupports* source = (*aSources)[i];
    nsIMsgFolder* folder;
    nsIMessage* message;
    if (NS_SUCCEEDED(source->QueryInterface(nsIMsgFolder::GetIID(), (void**)&folder))) {

      if (peq(aCommand, kNC_Delete)) {
        // XXX delete folder
      }

      NS_RELEASE(folder);
    }
    else if (NS_SUCCEEDED(source->QueryInterface(nsIMessage::GetIID(), (void**)&message))) {

      if (peq(aCommand, kNC_Delete)) {
				nsIMsgFolder *folder;
				if (rv = NS_SUCCEEDED(GetFolderFromMessage(message, &folder))) {
					rv = folder->DeleteMessage(message);
				}
      }
      else if (peq(aCommand, kNC_Reply)) {
        // XXX reply to message
      }
      else if (peq(aCommand, kNC_Forward)) {
        // XXX forward message
      }

      NS_RELEASE(message);
    }
  }
  return rv;
}

NS_IMETHODIMP nsMSGFolderDataSource::OnItemAdded(nsIFolder *parentFolder, nsISupports *item)
{
	nsIMessage *message;
	nsIRDFResource *parentResource;

	if(NS_SUCCEEDED(parentFolder->QueryInterface(nsIRDFResource::GetIID(), (void**)&parentResource)))
	{
		//If we are adding a message
		if(NS_SUCCEEDED(item->QueryInterface(nsIMessage::GetIID(), (void**)&message)))
		{
			nsIRDFNode *itemNode;
			if(NS_SUCCEEDED(item->QueryInterface(nsIRDFNode::GetIID(), (void**)&itemNode)))
			{
				//Notify folders that a message was added.
				NotifyObservers(parentResource, kNC_MessageChild, itemNode, PR_TRUE);
				NS_RELEASE(itemNode);
			}
			NS_RELEASE(message);
		}
		NS_RELEASE(parentResource);
	}
  return NS_OK;
}

NS_IMETHODIMP nsMSGFolderDataSource::OnItemRemoved(nsIFolder *parentFolder, nsISupports *item)
{
	nsIMessage *message;
	nsIRDFResource *parentResource;

	if(NS_SUCCEEDED(parentFolder->QueryInterface(nsIRDFResource::GetIID(), (void**)&parentResource)))
	{
		//If we are adding a message
		if(NS_SUCCEEDED(item->QueryInterface(nsIMessage::GetIID(), (void**)&message)))
		{
			nsIRDFNode *itemNode;
			if(NS_SUCCEEDED(item->QueryInterface(nsIRDFNode::GetIID(), (void**)&itemNode)))
			{
				//Notify folders that a message was deleted.
				NotifyObservers(parentResource, kNC_MessageChild, itemNode, PR_FALSE);
				NS_RELEASE(itemNode);
			}
			NS_RELEASE(message);
		}
		NS_RELEASE(parentResource);
	}
  return NS_OK;
}

NS_IMETHODIMP nsMSGFolderDataSource::OnItemPropertyChanged(nsISupports *item, const char *property,const char *value)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsMSGFolderDataSource::GetFolderFromMessage(nsIMessage *message, nsIMsgFolder** folder)
{
	nsresult rv;

	const char *uri;
	nsIRDFResource *resource;
	if(NS_SUCCEEDED( rv = message->QueryInterface(nsIRDFResource::GetIID(), (void**)&resource)))
	{
		resource->GetValue(&uri);
		nsString folderURIStr;
		nsMsgKey key;
		nsParseLocalMessageURI(uri, folderURIStr, &key);
		char *folderURI = folderURIStr.ToNewCString();

		nsIRDFResource *folderResource;
		gRDFService->GetResource(folderURI, &folderResource);
		delete[] folderURI;

		rv = NS_SUCCEEDED(folderResource->QueryInterface(nsIMsgFolder::GetIID(), (void**)folder));

		NS_RELEASE(resource);
		NS_RELEASE(folderResource);
	}
	return rv;

}

