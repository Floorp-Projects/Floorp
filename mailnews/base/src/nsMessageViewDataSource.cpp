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

#include "nsMessageViewDataSource.h"
#include "plstr.h"

#define VIEW_SHOW_ALL 0x1
#define VIEW_SHOW_READ 0x2
#define VIEW_SHOW_UNREAD 0x4
#define VIEW_SHOW_WATCHED 0x8

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMPL_ADDREF(nsMessageViewDataSource)
NS_IMPL_RELEASE(nsMessageViewDataSource)

NS_IMETHODIMP
nsMessageViewDataSource::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

  *result = nsnull;
  if (iid.Equals(nsIRDFCompositeDataSource::GetIID()) ||
    iid.Equals(nsIRDFDataSource::GetIID()) ||
      iid.Equals(kISupportsIID))
  {
    *result = NS_STATIC_CAST(nsIRDFCompositeDataSource*, this);
    AddRef();
    return NS_OK;
  }
	else if(iid.Equals(nsIMessageView::GetIID()))
	{
    *result = NS_STATIC_CAST(nsIMessageView*, this);
    AddRef();
    return NS_OK;
	}
  return NS_NOINTERFACE;
}

nsMessageViewDataSource::nsMessageViewDataSource(void)
{
	NS_INIT_REFCNT();
	mDataSource = nsnull;
	mURI = nsnull;
	mObservers = nsnull;
	mShowStatus = 0;
}

nsMessageViewDataSource::~nsMessageViewDataSource (void)
{
	RemoveDataSource(mDataSource);
	if(mURI)
		PL_strfree(mURI);
	if (mObservers) {
	  for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
		  nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
		  NS_RELEASE(obs);
	  }
	  delete mObservers;
	}

}


NS_IMETHODIMP nsMessageViewDataSource::Init(const char* uri)
{
	mURI = PL_strdup(uri);
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetURI(char* *uri)
{
	*uri = mURI;
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetSource(nsIRDFResource* property,
					   nsIRDFNode* target,
					   PRBool tv,
					   nsIRDFResource** source /* out */)
{
	if(mDataSource)
		return mDataSource->GetSource(property, target, tv, source);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetTarget(nsIRDFResource* source,
					   nsIRDFResource* property,
					   PRBool tv,
					   nsIRDFNode** target)
{
	if(mDataSource)
		return mDataSource->GetTarget(source, property, tv, target);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetSources(nsIRDFResource* property,
						nsIRDFNode* target,
						PRBool tv,
						nsIRDFAssertionCursor** sources)
{
	if(mDataSource)
		return mDataSource->GetSources(property, target, tv, sources);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetTargets(nsIRDFResource* source,
						nsIRDFResource* property,    
						PRBool tv,
						nsIRDFAssertionCursor** targets)
{
	if(mDataSource)
		return mDataSource->GetTargets(source, property, tv, targets);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::Assert(nsIRDFResource* source,
					nsIRDFResource* property, 
					nsIRDFNode* target,
					PRBool tv)
{
	if(mDataSource)
		return mDataSource->Assert(source, property, target, tv);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::Unassert(nsIRDFResource* source,
					  nsIRDFResource* property,
					  nsIRDFNode* target)
{
	if(mDataSource)
		return mDataSource->Unassert(source, property, target);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::HasAssertion(nsIRDFResource* source,
						  nsIRDFResource* property,
						  nsIRDFNode* target,
						  PRBool tv,
						  PRBool* hasAssertion)
{
	if(mDataSource)
		return mDataSource->HasAssertion(source, property, target, tv, hasAssertion);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::AddObserver(nsIRDFObserver* n)
{
	if (! mObservers) {
		if ((mObservers = new nsVoidArray()) == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
	}
	mObservers->AppendElement(n);
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::RemoveObserver(nsIRDFObserver* n)
{
  if (! mObservers)
    return NS_OK;
  mObservers->RemoveElement(n);
  return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::ArcLabelsIn(nsIRDFNode* node,
						 nsIRDFArcsInCursor** labels)
{
	if(mDataSource)
		return mDataSource->ArcLabelsIn(node, labels);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::ArcLabelsOut(nsIRDFResource* source,
						  nsIRDFArcsOutCursor** labels)
{
	if(mDataSource)
		return mDataSource->ArcLabelsOut(source, labels);
	else
		return NS_OK;
} 

NS_IMETHODIMP nsMessageViewDataSource::GetAllResources(nsIRDFResourceCursor** aCursor)
{
	if(mDataSource)
		return mDataSource->GetAllResources(aCursor);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::Flush()
{
	if(mDataSource)
		return mDataSource->Flush();
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetAllCommands(nsIRDFResource* source,
							nsIEnumerator/*<nsIRDFResource>*/** commands)
{
	if(mDataSource)
		return mDataSource->GetAllCommands(source, commands);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
							  nsIRDFResource*   aCommand,
							  nsISupportsArray/*<nsIRDFResource>*/* aArguments,
							  PRBool* aResult)
{
	if(mDataSource)
		return mDataSource->IsCommandEnabled(aSources, aCommand, aArguments, aResult);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
					   nsIRDFResource*   aCommand,
					   nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	if(mDataSource)
		return mDataSource->DoCommand(aSources, aCommand, aArguments);
	else
		return NS_OK;
}

//We're only going to allow one datasource at a time.
NS_IMETHODIMP nsMessageViewDataSource::AddDataSource(nsIRDFDataSource* source)
{
	if(mDataSource)
		RemoveDataSource(mDataSource);

	mDataSource = source;
	NS_ADDREF(mDataSource);

	mDataSource->AddObserver(this);

	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::RemoveDataSource(nsIRDFDataSource* source)
{
	mDataSource->RemoveObserver(this);
	NS_RELEASE(mDataSource);
	mDataSource = nsnull;

	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::OnAssert(nsIRDFResource* subject,
						nsIRDFResource* predicate,
						nsIRDFNode* object)
{
	if (mObservers) {
    for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
        nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
        obs->OnAssert(subject, predicate, object);
    }
    }
    return NS_OK;

}

NS_IMETHODIMP nsMessageViewDataSource::OnUnassert(nsIRDFResource* subject,
                          nsIRDFResource* predicate,
                          nsIRDFNode* object)
{
    if (mObservers) {
        for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnUnassert(subject, predicate, object);
        }
    }
    return NS_OK;
}


NS_IMETHODIMP nsMessageViewDataSource::SetShowAll(PRBool showAll)
{
	if(showAll)
		mShowStatus |= VIEW_SHOW_ALL;
	else if(mShowStatus & VIEW_SHOW_ALL)
		mShowStatus &= ~VIEW_SHOW_ALL;

	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::SetShowUnread(PRBool showUnread)
{
	if(showUnread)
		mShowStatus |= VIEW_SHOW_UNREAD;
	else if(mShowStatus & VIEW_SHOW_UNREAD)
		mShowStatus &= ~VIEW_SHOW_UNREAD;
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::SetShowRead(PRBool showRead)
{
	if(showRead)
		mShowStatus |= VIEW_SHOW_READ;
	else if(mShowStatus & VIEW_SHOW_READ)
		mShowStatus &= ~VIEW_SHOW_READ;
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::SetShowWatched(PRBool showWatched)
{
	if(showWatched)
		mShowStatus |= VIEW_SHOW_WATCHED;
	else if(mShowStatus & VIEW_SHOW_WATCHED)
		mShowStatus &= ~VIEW_SHOW_WATCHED;
	return NS_OK;
}