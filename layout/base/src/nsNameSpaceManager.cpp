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
#include "nsINameSpaceManager.h"
#include "nsINameSpace.h"
#include "nsHashtable.h"
#include "nsVoidArray.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"
#include "nsCRT.h"

static NS_DEFINE_IID(kINameSpaceManagerIID, NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kINameSpaceIID, NS_INAMESPACE_IID);


static const char kXMLNSNameSpaceURI[] = "<This isn't really a namespace>";
static const char kXMLNameSpaceURI[] = "http://www.w3.org/XML/1998/namespace";
static const char kHTMLNameSpaceURI[] = "http://www.w3.org/TR/REC-html40";  // XXX?? "urn:w3-org-ns:HTML"??

//-----------------------------------------------------------
// Name Space 

class NameSpaceImpl : public nsINameSpace {
public:
  NameSpaceImpl(nsINameSpaceManager* aManager,
                NameSpaceImpl* aParent, 
                nsIAtom* aPrefix, 
                const nsString& aURI);
  NameSpaceImpl(nsINameSpaceManager* aManager,
                NameSpaceImpl* aParent, 
                nsIAtom* aPrefix, 
                PRInt32 aNameSpaceID);
  virtual ~NameSpaceImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetNameSpaceManager(nsINameSpaceManager*& aManager) const;

  NS_IMETHOD GetNameSpaceID(PRInt32& aID) const;
  NS_IMETHOD GetNameSpaceURI(nsString& aURI) const;
  NS_IMETHOD GetNameSpacePrefix(nsIAtom*& aPrefix) const;

  NS_IMETHOD GetParentNameSpace(nsINameSpace*& aParent) const;

  NS_IMETHOD FindNameSpace(nsIAtom* aPrefix, nsINameSpace*& aNameSpace) const;
  NS_IMETHOD FindNameSpaceID(nsIAtom* aPrefix, PRInt32& aNameSpaceID) const;
  NS_IMETHOD FindNameSpacePrefix(PRInt32 aNameSpaceID, nsIAtom*& aPrefix) const;

  NS_IMETHOD CreateChildNameSpace(nsIAtom* aPrefix, const nsString& aURI,
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
                             const nsString& aURI)
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
NameSpaceImpl::GetNameSpaceURI(nsString& aURI) const
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
NameSpaceImpl::CreateChildNameSpace(nsIAtom* aPrefix, const nsString& aURI,
                                nsINameSpace*& aChildNameSpace)
{
  NameSpaceImpl* child = new NameSpaceImpl(mManager, this, aPrefix, aURI);

  return child->QueryInterface(kINameSpaceIID, (void**)&aChildNameSpace);
}

//-----------------------------------------------------------
// Name Space Manager

static PRInt32      gNameSpaceTableRefs;
static nsHashtable* gURIToIDTable;
static nsVoidArray* gURIArray;

class NameSpaceURIKey : public nsHashKey {
public:
  NameSpaceURIKey(const nsString* aString)
    : mString(aString)
  { }

  virtual ~NameSpaceURIKey(void)
  { }

  virtual PRUint32 HashValue(void) const
  {
#if 1 // case insensitive XXX should this be case sensitive???
    PRUint32 hash = 0;
    const PRUnichar* string = mString->GetUnicode();
    PRUnichar ch;
    while ((ch = *string++) != 0) {
      // FYI: hash = hash*37 + ch
      ch = nsCRT::ToLower(ch);
      hash = ((hash << 5) + (hash << 2) + hash) + ch;
    }
    return hash;
#else
    if (nsnull != mString) {
      return nsCRT::HashValue(mString->GetUnicode());
    }
    return 0;
#endif
  }

  virtual PRBool Equals(const nsHashKey *aKey) const
  {
    const nsString* other = ((const NameSpaceURIKey*)aKey)->mString;
    if (nsnull != mString) {
      if (nsnull != other) {
        return mString->EqualsIgnoreCase(*other); // XXX case sensitive?
      }
      return PR_FALSE;
    }
    return PRBool(nsnull == other);
  }

  virtual nsHashKey *Clone(void) const
  {
    return new NameSpaceURIKey(mString);
  }

  const nsString* mString;
};

static void AddRefTable()
{
  if (0 == gNameSpaceTableRefs++) {
    NS_ASSERTION(nsnull == gURIToIDTable, "already have URI table");
    NS_ASSERTION(nsnull == gURIArray, "already have URI array");

    gURIToIDTable = new nsHashtable();
    gURIArray = new nsVoidArray();

    nsString* xmlns = new nsString(kXMLNSNameSpaceURI);
    nsString* xml = new nsString(kXMLNameSpaceURI);
    nsString* html = new nsString(kHTMLNameSpaceURI);
    gURIArray->AppendElement(xmlns);  // ordering here needs to match IDs
    gURIArray->AppendElement(xml);
    gURIArray->AppendElement(html); 
    NameSpaceURIKey xmlnsKey(xmlns);
    NameSpaceURIKey xmlKey(xml);
    NameSpaceURIKey htmlKey(html);
    gURIToIDTable->Put(&xmlnsKey, (void*)kNameSpaceID_XMLNS);
    gURIToIDTable->Put(&xmlKey, (void*)kNameSpaceID_XML);
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

static PRInt32 FindNameSpaceID(const nsString& aURI)
{
  NS_ASSERTION(nsnull != gURIToIDTable, "no URI table");
  NameSpaceURIKey key(&aURI);
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

class NameSpaceManagerImpl : public nsINameSpaceManager {
public:
  NameSpaceManagerImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateRootNameSpace(nsINameSpace*& aRootNameSpace);

  NS_IMETHOD RegisterNameSpace(const nsString& aURI, 
			                         PRInt32& aNameSpaceID);

  NS_IMETHOD GetNameSpaceURI(PRInt32 aNameSpaceID, nsString& aURI);
  NS_IMETHOD GetNameSpaceID(const nsString& aURI, PRInt32& aNameSpaceID);

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
NameSpaceManagerImpl::RegisterNameSpace(const nsString& aURI, 
                                        PRInt32& aNameSpaceID)
{
  PRInt32 id = FindNameSpaceID(aURI);

  if (kNameSpaceID_Unknown == id) {
    nsString* uri = new nsString(aURI);
    gURIArray->AppendElement(uri); 
    id = gURIArray->Count();  // id is index + 1
    NameSpaceURIKey key(uri);
    gURIToIDTable->Put(&key, (void*)id);
  }
  aNameSpaceID = id;
  return NS_OK;
}

NS_IMETHODIMP
NameSpaceManagerImpl::GetNameSpaceURI(PRInt32 aNameSpaceID, nsString& aURI)
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
NameSpaceManagerImpl::GetNameSpaceID(const nsString& aURI, PRInt32& aNameSpaceID)
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
