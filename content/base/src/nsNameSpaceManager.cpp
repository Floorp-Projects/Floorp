/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */
#include "nsINameSpaceManager.h"
#include "nsINameSpace.h"
#include "nsHashtable.h"
#include "nsVoidArray.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"
#include "nsCRT.h"

static NS_DEFINE_IID(kINameSpaceManagerIID, NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kINameSpaceIID, NS_INAMESPACE_IID);


static const char kXMLNSNameSpaceURI[] = "http://www.w3.org/2000/xmlns";
static const char kXMLNameSpaceURI[] = "http://www.w3.org/XML/1998/namespace";
static const char kHTMLNameSpaceURI[] = "http://www.w3.org/TR/REC-html40";  // XXX?? "urn:w3-org-ns:HTML"??
// XXX To be removed: Bug 7834 ---
static const char kXHTMLNameSpaceURI[] = "http://www.w3.org/1999/xhtml";
static const char kXLinkNameSpaceURI[] = "http://www.w3.org/1999/xlink";

//-----------------------------------------------------------
// Name Space ID table support

static PRInt32      gNameSpaceTableRefs;
static nsHashtable* gURIToIDTable;
static nsVoidArray* gURIArray;

static void AddRefTable()
{
  if (0 == gNameSpaceTableRefs++) {
    NS_ASSERTION(nsnull == gURIToIDTable, "already have URI table");
    NS_ASSERTION(nsnull == gURIArray, "already have URI array");

    gURIToIDTable = new nsHashtable();
    gURIArray = new nsVoidArray();

    nsString* xmlns = new nsString( NS_ConvertToString(kXMLNSNameSpaceURI) );
    nsString* xml = new nsString( NS_ConvertToString(kXMLNameSpaceURI) );
    nsString* xhtml = new nsString( NS_ConvertToString(kXHTMLNameSpaceURI) );
    nsString* xlink = new nsString( NS_ConvertToString(kXLinkNameSpaceURI) );
    nsString* html = new nsString( NS_ConvertToString(kHTMLNameSpaceURI) );
    gURIArray->AppendElement(xmlns);  // ordering here needs to match IDs
    gURIArray->AppendElement(xml);
    gURIArray->AppendElement(xhtml); 
    gURIArray->AppendElement(xlink);
    gURIArray->AppendElement(html); 
    nsStringKey xmlnsKey(*xmlns);
    nsStringKey xmlKey(*xml);
    nsStringKey xhtmlKey(*xhtml);
    nsStringKey xlinkKey(*xlink);
    nsStringKey htmlKey(*html);
    gURIToIDTable->Put(&xmlnsKey, (void*)kNameSpaceID_XMLNS);
    gURIToIDTable->Put(&xmlKey, (void*)kNameSpaceID_XML);
    gURIToIDTable->Put(&xhtmlKey, (void*)kNameSpaceID_HTML);
    gURIToIDTable->Put(&xlinkKey, (void*)kNameSpaceID_XLink);
    gURIToIDTable->Put(&htmlKey, (void*)kNameSpaceID_HTML);
  }
  NS_ASSERTION(nsnull != gURIToIDTable, "no URI table");
  NS_ASSERTION(nsnull != gURIArray, "no URI array");
}

static void ReleaseTable()
{
  if (0 == --gNameSpaceTableRefs) {
    delete gURIToIDTable;
    PRInt32 index = gURIArray->Count();
    while (0 < index--) {
      nsString* str = (nsString*)gURIArray->ElementAt(index);
      delete str;
    }
    delete gURIArray;
    gURIToIDTable = nsnull;
    gURIArray = nsnull;
  }
}

static PRInt32 FindNameSpaceID(const nsAReadableString& aURI)
{
  NS_ASSERTION(nsnull != gURIToIDTable, "no URI table");
  nsStringKey key(aURI);
  void* value = gURIToIDTable->Get(&key);
  if (nsnull != value) {
    return PRInt32(value);
  }
  return kNameSpaceID_Unknown;
}

static const nsString* FindNameSpaceURI(PRInt32 aID)
{
  NS_ASSERTION(nsnull != gURIArray, "no URI array");
  return (const nsString*)gURIArray->ElementAt(aID - 1);
}

//-----------------------------------------------------------
// Name Space 

class NameSpaceImpl : public nsINameSpace {
public:
  NameSpaceImpl(nsINameSpaceManager* aManager,
                NameSpaceImpl* aParent, 
                nsIAtom* aPrefix, 
                const nsAReadableString& aURI);
  NameSpaceImpl(nsINameSpaceManager* aManager,
                NameSpaceImpl* aParent, 
                nsIAtom* aPrefix, 
                PRInt32 aNameSpaceID);
  virtual ~NameSpaceImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetNameSpaceManager(nsINameSpaceManager*& aManager) const;

  NS_IMETHOD GetNameSpaceID(PRInt32& aID) const;
  NS_IMETHOD GetNameSpaceURI(nsAWritableString& aURI) const;
  NS_IMETHOD GetNameSpacePrefix(nsIAtom*& aPrefix) const;

  NS_IMETHOD GetParentNameSpace(nsINameSpace*& aParent) const;

  NS_IMETHOD FindNameSpace(nsIAtom* aPrefix, nsINameSpace*& aNameSpace) const;
  NS_IMETHOD FindNameSpaceID(nsIAtom* aPrefix, PRInt32& aNameSpaceID) const;
  NS_IMETHOD FindNameSpacePrefix(PRInt32 aNameSpaceID, nsIAtom*& aPrefix) const;

  NS_IMETHOD CreateChildNameSpace(nsIAtom* aPrefix, const nsAReadableString& aURI,
                                  nsINameSpace*& aChildNameSpace);
  NS_IMETHOD CreateChildNameSpace(nsIAtom* aPrefix, PRInt32 aNameSpaceID,
                                  nsINameSpace*& aChildNameSpace);

private:
  // These are not supported and are not implemented!
  NameSpaceImpl(const NameSpaceImpl& aCopy);
  NameSpaceImpl& operator=(const NameSpaceImpl& aCopy);

public:
  nsINameSpaceManager*  mManager;
  NameSpaceImpl*        mParent;
  nsIAtom*              mPrefix;
  PRInt32               mID;
};

NameSpaceImpl::NameSpaceImpl(nsINameSpaceManager* aManager,
                             NameSpaceImpl* aParent, 
                             nsIAtom* aPrefix, 
                             const nsAReadableString& aURI)
  : mManager(aManager),
    mParent(aParent),
    mPrefix(aPrefix)
{
  NS_ASSERTION(nsnull != aManager, "null namespace manager");
  NS_INIT_REFCNT();
  NS_ADDREF(mManager);
  NS_IF_ADDREF(mParent);
  NS_IF_ADDREF(mPrefix);
  mManager->RegisterNameSpace(aURI, mID);
}

NameSpaceImpl::NameSpaceImpl(nsINameSpaceManager* aManager,
                             NameSpaceImpl* aParent, 
                             nsIAtom* aPrefix, 
                             PRInt32 aNameSpaceID)
  : mManager(aManager),
    mParent(aParent),
    mPrefix(aPrefix),
    mID(aNameSpaceID)
{
  NS_ASSERTION(nsnull != aManager, "null namespace manager");
  NS_INIT_REFCNT();
  NS_ADDREF(mManager);
  NS_IF_ADDREF(mParent);
  NS_IF_ADDREF(mPrefix);
}

NameSpaceImpl::~NameSpaceImpl()
{
  NS_RELEASE(mManager);
  NS_IF_RELEASE(mParent);
  NS_IF_RELEASE(mPrefix);
}

NS_IMPL_ISUPPORTS(NameSpaceImpl, kINameSpaceIID)

NS_IMETHODIMP
NameSpaceImpl::GetNameSpaceManager(nsINameSpaceManager*& aManager) const
{
  NS_ASSERTION(nsnull != aManager, "null namespace manager");
  aManager = mManager;
  NS_ADDREF(aManager);
  return NS_OK;
}

NS_IMETHODIMP
NameSpaceImpl::GetNameSpaceID(PRInt32& aID) const
{
  aID = mID;
  return NS_OK;
}

NS_IMETHODIMP
NameSpaceImpl::GetNameSpaceURI(nsAWritableString& aURI) const
{
  NS_ASSERTION(nsnull != mManager, "null namespace manager");
  return mManager->GetNameSpaceURI(mID, aURI);
}

NS_IMETHODIMP
NameSpaceImpl::GetNameSpacePrefix(nsIAtom*& aPrefix) const
{
  aPrefix = mPrefix;
  NS_IF_ADDREF(aPrefix);
  return NS_OK;
}

NS_IMETHODIMP
NameSpaceImpl::GetParentNameSpace(nsINameSpace*& aParent) const
{
  aParent = mParent;
  NS_IF_ADDREF(aParent);
  return NS_OK;
}

NS_IMETHODIMP
NameSpaceImpl::FindNameSpace(nsIAtom* aPrefix, nsINameSpace*& aNameSpace) const
{
  const NameSpaceImpl*  nameSpace = this;
  do {
    if (aPrefix == nameSpace->mPrefix) {
      aNameSpace = (nsINameSpace*)nameSpace;
      NS_ADDREF(aNameSpace);
      return NS_OK;
    }
    nameSpace = nameSpace->mParent;
  }
  while (nsnull != nameSpace);
  aNameSpace = nsnull;
  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
NameSpaceImpl::FindNameSpaceID(nsIAtom* aPrefix, PRInt32& aNameSpaceID) const
{
  const NameSpaceImpl*  nameSpace = this;
  do {
    if (aPrefix == nameSpace->mPrefix) {
      aNameSpaceID = nameSpace->mID;
      return NS_OK;
    }
    nameSpace = nameSpace->mParent;
  }
  while (nsnull != nameSpace);
  if (nsnull == aPrefix) {
    aNameSpaceID = kNameSpaceID_None;
  }
  else {
    aNameSpaceID = kNameSpaceID_Unknown;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
NameSpaceImpl::FindNameSpacePrefix(PRInt32 aNameSpaceID, nsIAtom*& aPrefix) const 
{
  const NameSpaceImpl*  nameSpace = this;
  do {
    if (aNameSpaceID == nameSpace->mID) {
      aPrefix = nameSpace->mPrefix;
      NS_IF_ADDREF(aPrefix);
      return NS_OK;
    }
    nameSpace = nameSpace->mParent;
  }
  while (nsnull != nameSpace);
  aPrefix = nsnull;
  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
NameSpaceImpl::CreateChildNameSpace(nsIAtom* aPrefix, const nsAReadableString& aURI,
                                nsINameSpace*& aChildNameSpace)
{
  NameSpaceImpl* child = new NameSpaceImpl(mManager, this, aPrefix, aURI);

  if (child) {
    return child->QueryInterface(kINameSpaceIID, (void**)&aChildNameSpace);
  }
  aChildNameSpace = nsnull;
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
NameSpaceImpl::CreateChildNameSpace(nsIAtom* aPrefix, PRInt32 aNameSpaceID,
                                nsINameSpace*& aChildNameSpace)
{
  if (FindNameSpaceURI(aNameSpaceID)) {
    NameSpaceImpl* child = new NameSpaceImpl(mManager, this, aPrefix, aNameSpaceID);

    if (child) {
      return child->QueryInterface(kINameSpaceIID, (void**)&aChildNameSpace);
    }
    aChildNameSpace = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aChildNameSpace = nsnull;
  return NS_ERROR_ILLEGAL_VALUE;
}

//-----------------------------------------------------------
// Name Space Manager

class NameSpaceManagerImpl : public nsINameSpaceManager {
public:
  NameSpaceManagerImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateRootNameSpace(nsINameSpace*& aRootNameSpace);

  NS_IMETHOD RegisterNameSpace(const nsAReadableString& aURI, 
			                         PRInt32& aNameSpaceID);

  NS_IMETHOD GetNameSpaceURI(PRInt32 aNameSpaceID, nsAWritableString& aURI);
  NS_IMETHOD GetNameSpaceID(const nsAReadableString& aURI, PRInt32& aNameSpaceID);

private:
  // These are not supported and are not implemented!
  NameSpaceManagerImpl(const NameSpaceManagerImpl& aCopy);
  NameSpaceManagerImpl& operator=(const NameSpaceManagerImpl& aCopy);

protected:
  virtual ~NameSpaceManagerImpl();

};

NameSpaceManagerImpl::NameSpaceManagerImpl()
{
  NS_INIT_REFCNT();
  AddRefTable();
}

NameSpaceManagerImpl::~NameSpaceManagerImpl()
{
  ReleaseTable();
}

NS_IMPL_ISUPPORTS(NameSpaceManagerImpl, kINameSpaceManagerIID)

NS_IMETHODIMP
NameSpaceManagerImpl::CreateRootNameSpace(nsINameSpace*& aRootNameSpace)
{
  nsresult  rv = NS_ERROR_OUT_OF_MEMORY;
  aRootNameSpace = nsnull;

  NameSpaceImpl* xmlns = new NameSpaceImpl(this, nsnull, nsLayoutAtoms::xmlnsNameSpace, kNameSpaceID_XMLNS);
  if (nsnull != xmlns) {
    NameSpaceImpl* xml = new NameSpaceImpl(this, xmlns, nsLayoutAtoms::xmlNameSpace, kNameSpaceID_XML);
    if (nsnull != xml) {
      rv = xml->QueryInterface(kINameSpaceIID, (void**)&aRootNameSpace);
    }
    else {
      delete xmlns;
    }
  }
  return rv;
}

NS_IMETHODIMP
NameSpaceManagerImpl::RegisterNameSpace(const nsAReadableString& aURI, 
                                        PRInt32& aNameSpaceID)
{
  PRInt32 id = FindNameSpaceID(aURI);

  if (kNameSpaceID_Unknown == id) {
    nsString* uri = new nsString(aURI);
    gURIArray->AppendElement(uri); 
    id = gURIArray->Count();  // id is index + 1
    nsStringKey key(*uri);
    gURIToIDTable->Put(&key, (void*)id);
  }
  aNameSpaceID = id;
  return NS_OK;
}

NS_IMETHODIMP
NameSpaceManagerImpl::GetNameSpaceURI(PRInt32 aNameSpaceID, nsAWritableString& aURI)
{
  const nsString* result = FindNameSpaceURI(aNameSpaceID);
  if (nsnull != result) {
    aURI = *result;
    return NS_OK;
  }
  aURI.Truncate();
  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
NameSpaceManagerImpl::GetNameSpaceID(const nsAReadableString& aURI, PRInt32& aNameSpaceID)
{
  aNameSpaceID = FindNameSpaceID(aURI);
  return NS_OK;
}

NS_LAYOUT nsresult
NS_NewNameSpaceManager(nsINameSpaceManager** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  NameSpaceManagerImpl  *it = new NameSpaceManagerImpl();

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kINameSpaceManagerIID, (void **) aInstancePtrResult);
}
