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

static NS_DEFINE_CID(kRDFServiceCID,							NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,		NS_RDFINMEMORYDATASOURCE_CID);

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::IID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRDFCursorIID, NS_IRDFCURSOR_IID);

nsIRDFResource* nsMSGFolderDataSource::kNC_Child;
nsIRDFResource* nsMSGFolderDataSource::kNC_MessageChild;
nsIRDFResource* nsMSGFolderDataSource::kNC_Folder;
nsIRDFResource* nsMSGFolderDataSource::kNC_Name;
nsIRDFResource* nsMSGFolderDataSource::kNC_MSGFolderRoot;

nsIRDFResource* nsMSGFolderDataSource::kNC_Subject;

static const char kURINC_MSGFolderRoot[]  = "mailbox:/";

#define NC_NAMESPACE_URI "http://home.netscape.com/NC-rdf#"
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, MessageChild);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Folder);

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Subject);

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
                                             nsIRDFService::IID(),
                                             (nsISupports**) &gRDFService);

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

  nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
  gRDFService = nsnull;
}


NS_IMPL_ISUPPORTS(nsMSGFolderDataSource, nsIRDFMSGFolderDataSource::IID());

 // nsIRDFDataSource methods
NS_IMETHODIMP nsMSGFolderDataSource::Init(const char* uri)
{
  if (mInitialized)
      return NS_ERROR_ALREADY_INITIALIZED;

  if ((mURI = PL_strdup(uri)) == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;

  if (! kNC_Child) {
    gRDFService->GetResource(kURINC_child,   &kNC_Child);
    gRDFService->GetResource(kURINC_MessageChild,   &kNC_MessageChild);
    gRDFService->GetResource(kURINC_Folder,  &kNC_Folder);
    gRDFService->GetResource(kURINC_Name,    &kNC_Name);
    gRDFService->GetResource(kURINC_MSGFolderRoot, &kNC_MSGFolderRoot);

    gRDFService->GetResource(kURINC_Subject, &kNC_Subject);
  }
#if 0
	//create the folder for the root folder
  nsresult rv;
	nsIMsgFolder *rootFolder;
#if 0
	if(NS_SUCCEEDED(kNC_MSGFolderRoot->QueryInterface(nsIMsgFolder::IID(), (void**)&rootFolder)))
	{		
		if(rootFolder)
		{
			rootFolder->SetName("Mail and News");
			rootFolder->SetDepth(0);
			nsNativeFileSpec startPath("c:\\Program Files\\Netscape\\Users\\mscott\\Mail", PR_FALSE);
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

  // we only have positive assertions in the mail data source.
	if (! tv)
		return NS_ERROR_RDF_NO_VALUE;

	nsIMsgFolder* folder;
	nsIMessage* message;
	if (NS_SUCCEEDED(source->QueryInterface(nsIMsgFolder::IID(), (void**) &folder)))
	{
    nsresult rv;

		if (peq(kNC_Name, property)) {
			char *name;
			rv = folder->GetName(&name);
			nsString nameString(name);
			createNode(nameString, target);
      PR_FREEIF(name);
		}
		else {
			rv = NS_ERROR_RDF_NO_VALUE;
		}
		if(folder)
			NS_RELEASE(folder);

		return rv;
  }
	else if (NS_SUCCEEDED(source->QueryInterface(nsIMessage::IID(), (void**) &message)))
	{
    nsresult rv;

		if (peq(kNC_Name, property) ||
        peq(kNC_Subject, property)) {
			nsAutoString subject;
			rv = message->GetProperty("subject", subject);
			createNode(subject, target);
		}
		else {
			rv = NS_ERROR_RDF_NO_VALUE;
		}
		if(folder)
			NS_RELEASE(folder);

		return rv;
  }
  else {
    return NS_ERROR_RDF_NO_VALUE;
  }
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
	if (NS_SUCCEEDED(source->QueryInterface(nsIMsgFolder::IID(), (void**)&folder)))
	{

		if (peq(kNC_Child, property))
		{
			nsIEnumerator *subFolders;

      folder->GetSubFolders(&subFolders);
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

			folder->GetMessages(&messages);
		  nsRDFEnumeratorAssertionCursor* cursor =
        new nsRDFEnumeratorAssertionCursor(this, source, kNC_MessageChild, messages);
			NS_IF_RELEASE(messages);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
			rv = NS_OK;
		}
		else if(peq(kNC_Name, property))
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
  else if (NS_SUCCEEDED(source->QueryInterface(nsIMessage::IID(), (void**)&message))) {
    if(peq(kNC_Name, property))
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

  arcs->AppendElement(kNC_Child);
  arcs->AppendElement(kNC_MessageChild);
  arcs->AppendElement(kNC_Name);
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

NS_IMETHODIMP nsMSGFolderDataSource::IsCommandEnabled(const char* aCommand,
                                                      nsIRDFResource* aCommandTarget,
                                                      PRBool* aResult)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMSGFolderDataSource::DoCommand(const char* aCommand,
                         nsIRDFResource* aCommandTarget)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}
