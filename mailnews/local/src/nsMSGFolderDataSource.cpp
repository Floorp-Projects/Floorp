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

static NS_DEFINE_CID(kRDFServiceCID,							NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,		NS_RDFINMEMORYDATASOURCE_CID);

nsIRDFResource* nsMSGFolderDataSource::kNC_Child;
nsIRDFResource* nsMSGFolderDataSource::kNC_Folder;
nsIRDFResource* nsMSGFolderDataSource::kNC_Name;
nsIRDFResource* nsMSGFolderDataSource::kNC_MSGFolderRoot;

static const char kURINC_MSGFolderRoot[]  = "mailbox:MSGFolderRoot";

#define NC_NAMESPACE_URI "http://home.netscape.com/NC-rdf#"
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Folder);

extern  nsresult  NS_NewRDFMsgFolderResourceFactory(nsIRDFResourceFactory** aInstancePtrResult);

////////////////////////////////////////////////////////////////////////
// The RDF service manager. Cached in the address book data source's
// constructor

static nsIRDFService* gRDFService = nsnull;

////////////////////////////////////////////////////////////////////////
// THE address book source.

static nsMSGFolderDataSource* gMsgFolderDataSource = nsnull;
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

static void createNode(const char *str, nsIRDFNode **node)
{
	nsIRDFLiteral * value;
	nsString nsStr(str);

	*node = nsnull;

	if(NS_SUCCEEDED(gRDFService->GetLiteral((const PRUnichar*)nsStr, &value)))
	{
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

	gMsgFolderDataSource = this;
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
  NS_RELEASE2(kNC_Folder, refcnt);
  NS_RELEASE2(kNC_Name, refcnt);
  NS_RELEASE2(kNC_MSGFolderRoot, refcnt);

  gMsgFolderDataSource = nsnull;

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

  nsresult rv;

  if (! kNC_Child) {
    gRDFService->GetResource(kURINC_child,   &kNC_Child);
    gRDFService->GetResource(kURINC_Folder,  &kNC_Folder);
    gRDFService->GetResource(kURINC_Name,    &kNC_Name);
    gRDFService->GetResource(kURINC_MSGFolderRoot, &kNC_MSGFolderRoot);
  }

	//create the folder for the root folder
	nsIMsgFolder *rootFolder;
	if(NS_SUCCEEDED(kNC_MSGFolderRoot->QueryInterface(nsIMsgFolder::IID(), (void**)&rootFolder)))
	{		
		if(rootFolder)
		{
			rootFolder->SetName("Mail and News");
			rootFolder->SetDepth(0);
			nsNativeFileSpec startPath("your mail path here", PR_FALSE);
			if (NS_FAILED(rv = InitLocalFolders(rootFolder, startPath, 1)))
				return rv;

			NS_RELEASE(rootFolder);
		}
	}

	mInitialized = PR_TRUE;
	return NS_OK;}

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
	if (NS_SUCCEEDED(source->QueryInterface(nsIMsgFolder::IID(), (void**) &folder)))
	{

    nsresult rv;


		if (peq(kNC_Name, property)) {
			char * name;
			rv = folder->GetName(&name);
			createNode(name, target);
		}
		else {
			rv = NS_ERROR_RDF_NO_VALUE;
		}
		if(folder)
			NS_RELEASE(folder);

		return rv;
  } else {
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
	if(NS_SUCCEEDED(source->QueryInterface(nsIMsgFolder::IID(), (void**)&folder)))
	{

		if (peq(kNC_Child, property))
		{
			nsISupportsArray *subFolders;

			folder->GetSubFolders (&subFolders);

		  *targets = new ArrayMsgFolderCursor(source, kNC_Child, subFolders);

			NS_IF_RELEASE(subFolders);
			rv = NS_OK;
		}
		else if(peq(kNC_Name, property))
		{
			*targets = new SingletonMsgFolderCursor(source, property, PR_FALSE);
			rv = NS_OK;
		}
		NS_IF_RELEASE(folder);
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
	nsISupportsArray *temp;
	NS_NewISupportsArray(&temp);

  temp->AppendElement(kNC_Child);
  temp->AppendElement(kNC_Name);
  *labels = new MsgFolderArcsOutCursor(source, temp);
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

nsresult nsMSGFolderDataSource::InitFoldersFromDirectory(nsIMsgFolder *folder, nsNativeFileSpec& aPath, PRInt32 depth)
{

	char *currentFolderName =nsnull;

	for (nsDirectoryIterator dir(aPath); dir; dir++)
	{
		nsNativeFileSpec currentFolderPath = (nsNativeFileSpec&)dir;
		if(currentFolderName)
			delete[] currentFolderName;

		currentFolderName = currentFolderPath.GetLeafName();
		if (ShouldIgnoreFile (currentFolderName))
				continue;

		InitLocalFolders (folder, currentFolderPath, depth);
	}
	if(currentFolderName)
		delete[] currentFolderName;

	return NS_OK;
}

nsresult
nsMSGFolderDataSource::InitLocalFolders(nsIMsgFolder* aParentFolder, nsNativeFileSpec& aPath, PRInt32 depth)
{
  const char *kDirExt = ".sbd";

  nsFilePath aFilePath(aPath);
	char* pathStr = (char*)aFilePath;

	PRInt32 newFlags = MSG_FOLDER_FLAG_MAIL;

	PRBool addNewFolderToTree = PR_TRUE;
	nsIMsgFolder *folder = nsnull;

	if(aPath.IsDirectory())
	{
		// pathStr specifies a filesystem directory
		char *lastFour = &pathStr[PL_strlen(pathStr) - PL_strlen(kDirExt)];
		if (PL_strcasecmp(lastFour, kDirExt))
		{
			newFlags |= (MSG_FOLDER_FLAG_DIRECTORY | MSG_FOLDER_FLAG_ELIDED);

			// Create a new resource for this folder and set the child and name properties
			PRInt16 fileurlSize = PL_strlen("mailbox:") + PL_strlen(pathStr);
			char *fileurl = (char*)PR_Malloc( fileurlSize + 1);
			nsIRDFResource* resource;

			PR_snprintf(fileurl, fileurlSize, "mailbox:%s", pathStr);
			gRDFService->GetResource(fileurl, (nsIRDFResource**)&resource);

			PR_Free(fileurl);

			if(NS_SUCCEEDED(resource->QueryInterface(nsIMsgFolder::IID(), (void**)&folder)))
			{
				char * folderName;

				folder->GetNameFromPathName(pathStr, &folderName);
				folder->SetName(folderName);

				nsIMsgLocalMailFolder *mailFolder;
				if(NS_SUCCEEDED(folder->QueryInterface(nsIMsgLocalMailFolder::IID(), (void**)&mailFolder)))
				{
					mailFolder->SetPathName(pathStr);
					NS_IF_RELEASE(mailFolder);
				}

				
				folder->SetDepth(depth);
				folder->SetFlag(newFlags);
				aParentFolder->AddSubFolder(folder);
			}

			InitFoldersFromDirectory(folder, aPath, depth + 1); 

			NS_IF_RELEASE(folder);
			NS_IF_RELEASE(resource);
		}
	}
	else
	{

		// Create a new resource for this folder and set the child and name properties
		PRInt16 fileurlSize = PL_strlen("mailbox:") + PL_strlen(pathStr);
		char *fileurl = (char*)PR_Malloc( fileurlSize + 1);
		nsIRDFResource* resource;

		PR_snprintf(fileurl, fileurlSize, "mailbox:%s", pathStr);
		gRDFService->GetResource(fileurl, (nsIRDFResource**)&resource);

		PR_Free(fileurl);

		if(NS_SUCCEEDED(resource->QueryInterface(nsIMsgFolder::IID(), (void**)&folder)))
		{
			char * folderName;
			folder->GetNameFromPathName(pathStr, &folderName);
			folder->SetName(folderName);

			nsIMsgLocalMailFolder *mailFolder;
			if(NS_SUCCEEDED(folder->QueryInterface(nsIMsgLocalMailFolder::IID(), (void**)&mailFolder)))
			{
				mailFolder->SetPathName(pathStr);
				NS_IF_RELEASE(mailFolder);
			}

			folder->SetDepth(depth);
			if (addNewFolderToTree)
			{
				aParentFolder->AddSubFolder(folder);
			}

			if (folder)
			{
				folder->UpdateSummaryTotals();
				// Look for a directory for this mail folder, and recurse into it.
				// e.g. if the folder is "inbox", look for "inbox.sbd". 
				char *folderName = aPath.GetLeafName();
				char *newLeafName = (char*)malloc(PL_strlen(folderName) + PL_strlen(kDirExt) + 2);
				PL_strcpy(newLeafName, folderName);
				PL_strcat(newLeafName, kDirExt);
				aPath.SetLeafName(newLeafName);
				if(folderName)
					delete[] folderName;
				if(newLeafName)
					delete[] newLeafName;

				if(aPath.IsDirectory())
				{
					// If we knew it was a directory before getting here, we must have
					// found that out from the folder cache. In that case, the elided bit
					// is already what it should be, and we shouldn't change it. Otherwise
					// the default setting is collapsed.
					// NOTE: these flags do not affect the sort order, so we don't have to call
					// QuickSort after changing them.

					PRBool hasFlag;
					folder->GetFlag(MSG_FOLDER_FLAG_DIRECTORY, &hasFlag);
					if (!hasFlag)
						folder->SetFlag (MSG_FOLDER_FLAG_ELIDED);
					folder->SetFlag (MSG_FOLDER_FLAG_DIRECTORY);

					InitFoldersFromDirectory(folder, aPath, depth + 1); 
				}
			}
		}
		NS_IF_RELEASE(folder);
		NS_IF_RELEASE(resource);


	}

	return NS_OK;

}


NS_EXPORT nsresult
NS_NewRDFMSGFolderDataSource(nsIRDFDataSource** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

  // Only one mail data source
  if (! gMsgFolderDataSource) {
    if ((gMsgFolderDataSource = new nsMSGFolderDataSource()) == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(gMsgFolderDataSource);
  *result = gMsgFolderDataSource;
  return NS_OK;
}

SingletonMsgFolderCursor::SingletonMsgFolderCursor(nsIRDFNode* u,
                                         nsIRDFResource* s,
                                         PRBool inversep)
{
  if (!inversep) {
    mSource = (nsIRDFResource*)u;
    mTarget = nsnull;
  } else {
    mSource = nsnull;
    mTarget = u;
  }
  NS_ADDREF(u);
  mProperty = s;
  NS_ADDREF(mProperty);
  mValueReturnedp = PR_FALSE;
  mInversep = inversep;
  mValue = nsnull;
}


SingletonMsgFolderCursor::~SingletonMsgFolderCursor(void)
{
  NS_IF_RELEASE(mSource);
  NS_IF_RELEASE(mValue);
  NS_IF_RELEASE(mProperty);
  NS_IF_RELEASE(mTarget);
}



    // nsIRDFCursor interface
NS_IMETHODIMP
SingletonMsgFolderCursor::Advance(void)
{
  nsresult rv = NS_ERROR_RDF_CURSOR_EMPTY;
  if (mValueReturnedp) {
      NS_IF_RELEASE(mValue);
      mValue = nsnull;
      return NS_ERROR_RDF_CURSOR_EMPTY;
  }
  mValueReturnedp = PR_TRUE;
  if (mInversep) {
      rv = gMsgFolderDataSource->GetSource(mProperty, mTarget, 1, (nsIRDFResource**)&mValue);
      mSource = (nsIRDFResource*)mValue;
  } else {
      rv = gMsgFolderDataSource->GetTarget(mSource, mProperty,  1, &mValue);
      mTarget = mValue;
  }
  if (mValue) {
      NS_ADDREF(mValue);
      NS_ADDREF(mValue);
  }
  // yes, its required twice, one for the value and one for the source/target
  if (rv == NS_ERROR_RDF_NO_VALUE) return NS_ERROR_RDF_CURSOR_EMPTY;
  return rv;
}

NS_IMETHODIMP
SingletonMsgFolderCursor::GetValue(nsIRDFNode** aValue)
{
  NS_ADDREF(mValue);
  *aValue = mValue;
  return NS_OK;
}

    // nsIRDFAssertionCursor interface
NS_IMETHODIMP
SingletonMsgFolderCursor::GetDataSource(nsIRDFDataSource** aDataSource)
{
  NS_ADDREF(gMsgFolderDataSource);
  *aDataSource = gMsgFolderDataSource;
  return NS_OK;
}

NS_IMETHODIMP
SingletonMsgFolderCursor::GetSubject(nsIRDFResource** aResource)
{
  NS_ADDREF(mSource);
  *aResource = mSource;
  return NS_OK;
}

NS_IMETHODIMP
SingletonMsgFolderCursor::GetPredicate(nsIRDFResource** aPredicate)
{
  NS_ADDREF(mProperty);
  *aPredicate = mProperty;
  return NS_OK;
}

NS_IMETHODIMP
SingletonMsgFolderCursor::GetObject(nsIRDFNode** aObject)
{
  NS_ADDREF(mTarget);
  *aObject = mTarget;
  return NS_OK;
}

NS_IMETHODIMP
SingletonMsgFolderCursor::GetTruthValue(PRBool* aTruthValue)
{
  *aTruthValue = PR_TRUE;
  return NS_OK;
}


NS_IMPL_ISUPPORTS(SingletonMsgFolderCursor, nsIRDFAssertionCursor::IID());

ArrayMsgFolderCursor::ArrayMsgFolderCursor(nsIRDFResource* u,
											   nsIRDFResource* s,
											   nsISupportsArray* array)
{
    //  getsources and gettargets will call this with the array
  mSource = u;
  mProperty = s;
  mArray = array;
  NS_ADDREF(mProperty);
  NS_ADDREF(u);
	NS_ADDREF(mArray);
  mCount = 0;
  mTarget = nsnull;
  mValue = nsnull;
}

ArrayMsgFolderCursor::~ArrayMsgFolderCursor(void)
{
  NS_IF_RELEASE(mSource);
  NS_IF_RELEASE(mValue);
  NS_IF_RELEASE(mProperty);
  NS_IF_RELEASE(mTarget);
	NS_IF_RELEASE(mArray);
}


    // nsIRDFCursor interface
NS_IMETHODIMP
ArrayMsgFolderCursor::Advance(void)
{
	nsresult rv = NS_ERROR_FAILURE;

  if (mArray->Count() <= mCount) return  NS_ERROR_RDF_CURSOR_EMPTY;
  NS_IF_RELEASE(mValue);
	mTarget = mValue = (nsIRDFNode*) mArray->ElementAt(mCount++);
  NS_ADDREF(mValue);
	return NS_OK;
}

NS_IMETHODIMP
ArrayMsgFolderCursor::GetValue(nsIRDFNode** aValue)
{
  NS_ADDREF(mValue);
  *aValue = mValue;
  return NS_OK;
}

    // nsIRDFAssertionCursor interface
NS_IMETHODIMP
ArrayMsgFolderCursor::GetDataSource(nsIRDFDataSource** aDataSource)
{
  NS_ADDREF(gMsgFolderDataSource);
  *aDataSource = gMsgFolderDataSource;
  return NS_OK;
}

NS_IMETHODIMP
ArrayMsgFolderCursor::GetSubject(nsIRDFResource** aResource)
{
	NS_ADDREF(mSource);
  *aResource = mSource;
  return NS_OK;
}

NS_IMETHODIMP
ArrayMsgFolderCursor::GetPredicate(nsIRDFResource** aPredicate)
{
  NS_ADDREF(mProperty);
  *aPredicate = mProperty;
  return NS_OK;
}

NS_IMETHODIMP
ArrayMsgFolderCursor::GetObject(nsIRDFNode** aObject)
{
  NS_ADDREF(mTarget);
  *aObject = mTarget;
  return NS_OK;
}

NS_IMETHODIMP
ArrayMsgFolderCursor::GetTruthValue(PRBool* aTruthValue)
{
  *aTruthValue = 1;
  return NS_OK;
}

NS_IMPL_ADDREF(ArrayMsgFolderCursor);
NS_IMPL_RELEASE(ArrayMsgFolderCursor);

NS_IMETHODIMP
ArrayMsgFolderCursor::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

  *result = nsnull;
  if (iid.Equals(nsIRDFAssertionCursor::IID()) ||
      iid.Equals(::nsIRDFCursor::IID()) ||
      iid.Equals(::nsISupports::IID())) {
      *result = NS_STATIC_CAST(nsIRDFAssertionCursor*, this);
      AddRef();
      return NS_OK;
  }
  return NS_NOINTERFACE;
}


MsgFolderArcsOutCursor::MsgFolderArcsOutCursor(nsIRDFResource* source,
											   nsISupportsArray* array)
{
    //  getsources and gettargets will call this with the array
  mSource = source;
  mProperty = nsnull;
  mArray = array;
  NS_ADDREF(source);
	NS_ADDREF(mArray);
  mCount = 0;
}

MsgFolderArcsOutCursor::~MsgFolderArcsOutCursor(void)
{
  NS_IF_RELEASE(mSource);
  NS_IF_RELEASE(mProperty);
	NS_IF_RELEASE(mArray);
}


    // nsIRDFCursor interface
NS_IMETHODIMP
MsgFolderArcsOutCursor::Advance(void)
{
	nsresult rv = NS_ERROR_FAILURE;

  if (mArray->Count() <= mCount) return  NS_ERROR_RDF_CURSOR_EMPTY;
  NS_IF_RELEASE(mProperty);
  mProperty = (nsIRDFResource*)mArray->ElementAt(mCount++);
  NS_ADDREF(mProperty);
	return NS_OK;
}

NS_IMETHODIMP
MsgFolderArcsOutCursor::GetValue(nsIRDFNode** aValue)
{
  NS_ADDREF(mProperty);
  *aValue = mProperty;
  return NS_OK;
}

    // nsIRDFAssertionCursor interface
NS_IMETHODIMP
MsgFolderArcsOutCursor::GetDataSource(nsIRDFDataSource** aDataSource)
{
  NS_ADDREF(gMsgFolderDataSource);
  *aDataSource = gMsgFolderDataSource;
  return NS_OK;
}

NS_IMETHODIMP
MsgFolderArcsOutCursor::GetSubject(nsIRDFResource** aResource)
{
	NS_ADDREF(mSource);
  *aResource = mSource;
  return NS_OK;
}

NS_IMETHODIMP
MsgFolderArcsOutCursor::GetPredicate(nsIRDFResource** aPredicate)
{
  NS_ADDREF(mProperty);
  *aPredicate = mProperty;
  return NS_OK;
}


NS_IMPL_ADDREF(MsgFolderArcsOutCursor);
NS_IMPL_RELEASE(MsgFolderArcsOutCursor);

NS_IMETHODIMP
MsgFolderArcsOutCursor::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

  *result = nsnull;
  if (iid.Equals(nsIRDFCursor::IID()) ||
      iid.Equals(nsIRDFArcsOutCursor::IID()) ||
      iid.Equals(::nsISupports::IID())) {
      *result = NS_STATIC_CAST(nsIRDFArcsOutCursor*, this);
      AddRef();
      return NS_OK;
  }
  return NS_NOINTERFACE;
}
