/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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
#include "nscore.h"
#include "nsINameSpaceManager.h"
#include "nsINameSpace.h"
#include "nsISupportsArray.h"
#include "nsIElementFactory.h"
#include "nsIServiceManager.h"
#include "nsHashtable.h"
#include "nsVoidArray.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsContentCID.h"


extern nsresult NS_NewXMLElementFactory(nsIElementFactory** aResult);

static const char kXMLNSNameSpaceURI[] = "http://www.w3.org/2000/xmlns/";
static const char kXMLNameSpaceURI[] = "http://www.w3.org/XML/1998/namespace";
static const char kXHTMLNameSpaceURI[] = "http://www.w3.org/1999/xhtml";
static const char kXLinkNameSpaceURI[] = "http://www.w3.org/1999/xlink";
static const char kXSLTNameSpaceURI[] = "http://www.w3.org/1999/XSL/Transform";
static const char kXBLNameSpaceURI[] = "http://www.mozilla.org/xbl";
static const char kMathMLNameSpaceURI[] = "http://www.w3.org/1998/Math/MathML";

//-----------------------------------------------------------
// Name Space ID table support

static PRBool             gNameSpaceManagerIsInitialized = PR_FALSE;
static nsHashtable*       gURIToIDTable;
static nsVoidArray*       gURIArray;
static nsISupportsArray*  gElementFactoryArray;
static nsIElementFactory* gDefaultElementFactory;

#ifdef NS_DEBUG
static PRBool            gNameSpaceManagerWasShutDown = PR_FALSE;
#endif

static void InitializeNameSpaceManager()
{
  if (gNameSpaceManagerIsInitialized) {
    return;
  }

  NS_ASSERTION(!gURIToIDTable, "already have URI table");
  NS_ASSERTION(!gURIArray, "already have URI array");

  gURIToIDTable = new nsHashtable();
  gURIArray = new nsVoidArray();

  nsString* xmlns  = new nsString(NS_ConvertASCIItoUCS2(kXMLNSNameSpaceURI));
  nsString* xml    = new nsString(NS_ConvertASCIItoUCS2(kXMLNameSpaceURI));
  nsString* xhtml  = new nsString(NS_ConvertASCIItoUCS2(kXHTMLNameSpaceURI));
  nsString* xlink  = new nsString(NS_ConvertASCIItoUCS2(kXLinkNameSpaceURI));
  nsString* xslt   = new nsString(NS_ConvertASCIItoUCS2(kXSLTNameSpaceURI));
  nsString* xbl    = new nsString(NS_ConvertASCIItoUCS2(kXBLNameSpaceURI));
  nsString* mathml = new nsString(NS_ConvertASCIItoUCS2(kMathMLNameSpaceURI));

  gURIArray->AppendElement(xmlns);  // ordering here needs to match IDs
  gURIArray->AppendElement(xml);
  gURIArray->AppendElement(xhtml); 
  gURIArray->AppendElement(xlink);
  gURIArray->AppendElement(xslt);
  gURIArray->AppendElement(xbl);
  gURIArray->AppendElement(mathml);

  nsStringKey xmlnsKey(*xmlns);
  nsStringKey xmlKey(*xml);
  nsStringKey xhtmlKey(*xhtml);
  nsStringKey xlinkKey(*xlink);
  nsStringKey xsltKey(*xslt);
  nsStringKey xblKey(*xbl);
  nsStringKey mathmlKey(*mathml);

  gURIToIDTable->Put(&xmlnsKey,  NS_INT32_TO_PTR(kNameSpaceID_XMLNS));
  gURIToIDTable->Put(&xmlKey,    NS_INT32_TO_PTR(kNameSpaceID_XML));
  gURIToIDTable->Put(&xhtmlKey,  NS_INT32_TO_PTR(kNameSpaceID_XHTML));
  gURIToIDTable->Put(&xlinkKey,  NS_INT32_TO_PTR(kNameSpaceID_XLink));
  gURIToIDTable->Put(&xsltKey,   NS_INT32_TO_PTR(kNameSpaceID_XSLT));
  gURIToIDTable->Put(&xblKey,    NS_INT32_TO_PTR(kNameSpaceID_XBL));
  gURIToIDTable->Put(&mathmlKey, NS_INT32_TO_PTR(kNameSpaceID_MathML));

  NS_NewISupportsArray(&gElementFactoryArray);
  NS_NewXMLElementFactory(&gDefaultElementFactory);

  NS_ASSERTION(gURIToIDTable, "no URI table");
  NS_ASSERTION(gURIArray, "no URI array");
  NS_ASSERTION(gElementFactoryArray, "no element factory array");

  gNameSpaceManagerIsInitialized = PR_TRUE;
}

void NS_NameSpaceManagerShutdown()
{
  delete gURIToIDTable;
  PRInt32 index = gURIArray->Count();
  while (0 < index--) {
    nsString* str = (nsString*)gURIArray->ElementAt(index);
    delete str;
  }
  delete gURIArray;
  gURIToIDTable = nsnull;
  gURIArray = nsnull;

  NS_IF_RELEASE(gElementFactoryArray);
  NS_IF_RELEASE(gDefaultElementFactory);

#ifdef NS_DEBUG
  gNameSpaceManagerWasShutDown = PR_TRUE;
#endif
}

static PRInt32 FindNameSpaceID(const nsAString& aURI)
{
  if (aURI.IsEmpty()) {
    return kNameSpaceID_None;  // xmlns="", see bug 75700 for details
  }
  
  NS_ASSERTION(gURIToIDTable, "no URI table");
  const nsPromiseFlatString& flatString = PromiseFlatString(aURI); 
  nsStringKey key(flatString);
  void* value = gURIToIDTable->Get(&key);
  if (value) {
    return NS_PTR_TO_INT32(value);
  }
  
  return kNameSpaceID_Unknown;
}

static const nsString* FindNameSpaceURI(PRInt32 aID)
{
  NS_ASSERTION(gURIArray, "no URI array");
  return (const nsString*)gURIArray->SafeElementAt(aID - 1);
}

//-----------------------------------------------------------
// Name Space 

class NameSpaceImpl : public nsINameSpace {
public:
  NameSpaceImpl(nsINameSpaceManager* aManager,
                NameSpaceImpl* aParent, 
                nsIAtom* aPrefix, 
                const nsAString& aURI);
  NameSpaceImpl(nsINameSpaceManager* aManager,
                NameSpaceImpl* aParent, 
                nsIAtom* aPrefix, 
                PRInt32 aNameSpaceID);
  virtual ~NameSpaceImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetNameSpaceManager(nsINameSpaceManager*& aManager) const;

  NS_IMETHOD GetNameSpaceID(PRInt32& aID) const;
  NS_IMETHOD GetNameSpaceURI(nsAString& aURI) const;
  NS_IMETHOD GetNameSpacePrefix(nsIAtom*& aPrefix) const;

  NS_IMETHOD GetParentNameSpace(nsINameSpace*& aParent) const;

  NS_IMETHOD FindNameSpace(nsIAtom* aPrefix, nsINameSpace*& aNameSpace) const;
  NS_IMETHOD FindNameSpaceID(nsIAtom* aPrefix, PRInt32& aNameSpaceID) const;
  NS_IMETHOD FindNameSpacePrefix(PRInt32 aNameSpaceID, nsIAtom*& aPrefix) const;

  NS_IMETHOD CreateChildNameSpace(nsIAtom* aPrefix, const nsAString& aURI,
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
                             const nsAString& aURI)
  : mManager(aManager),
    mParent(aParent),
    mPrefix(aPrefix)
{
  NS_ASSERTION(aManager, "null namespace manager");
  NS_INIT_ISUPPORTS();
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
  NS_ASSERTION(aManager, "null namespace manager");
  NS_INIT_ISUPPORTS();
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

NS_IMPL_ISUPPORTS1(NameSpaceImpl, nsINameSpace)

NS_IMETHODIMP
NameSpaceImpl::GetNameSpaceManager(nsINameSpaceManager*& aManager) const
{
  NS_ASSERTION(aManager, "null namespace manager");
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
NameSpaceImpl::GetNameSpaceURI(nsAString& aURI) const
{
  NS_ASSERTION(mManager, "null namespace manager");
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
  } while (nameSpace);

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
  } while (nameSpace);

  if (!aPrefix) {
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
  } while (nameSpace);

  aPrefix = nsnull;
  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
NameSpaceImpl::CreateChildNameSpace(nsIAtom* aPrefix, const nsAString& aURI,
                                nsINameSpace*& aChildNameSpace)
{
  NameSpaceImpl* child = new NameSpaceImpl(mManager, this, aPrefix, aURI);

  if (child) {
    return child->QueryInterface(NS_GET_IID(nsINameSpace), (void**)&aChildNameSpace);
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
      return child->QueryInterface(NS_GET_IID(nsINameSpace), (void**)&aChildNameSpace);
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

  NS_IMETHOD RegisterNameSpace(const nsAString& aURI, 
			                         PRInt32& aNameSpaceID);

  NS_IMETHOD GetNameSpaceURI(PRInt32 aNameSpaceID, nsAString& aURI);
  NS_IMETHOD GetNameSpaceID(const nsAString& aURI,
                            PRInt32& aNameSpaceID);
  NS_IMETHOD GetElementFactory(PRInt32 aNameSpaceID,
                               nsIElementFactory **aElementFactory);
  NS_IMETHOD HasRegisteredFactory(PRInt32 aNameSpaceID,
                                  PRBool* aHasFactory);
private:
  // These are not supported and are not implemented!
  NameSpaceManagerImpl(const NameSpaceManagerImpl& aCopy);
  NameSpaceManagerImpl& operator=(const NameSpaceManagerImpl& aCopy);

protected:
  virtual ~NameSpaceManagerImpl();

};

NameSpaceManagerImpl::NameSpaceManagerImpl()
{
  NS_ASSERTION(!gNameSpaceManagerWasShutDown,
               "Namespace manager used past content module shutdown!!!");

  NS_INIT_ISUPPORTS();

  InitializeNameSpaceManager();
}

NameSpaceManagerImpl::~NameSpaceManagerImpl()
{
}

NS_IMPL_ISUPPORTS1(NameSpaceManagerImpl, nsINameSpaceManager)

NS_IMETHODIMP
NameSpaceManagerImpl::CreateRootNameSpace(nsINameSpace*& aRootNameSpace)
{
  NS_ASSERTION(!gNameSpaceManagerWasShutDown,
               "Namespace manager used past content module shutdown!!!");

  nsresult  rv = NS_ERROR_OUT_OF_MEMORY;
  aRootNameSpace = nsnull;

  NameSpaceImpl* xmlns = new NameSpaceImpl(this, nsnull,
                                           nsLayoutAtoms::xmlnsNameSpace,
                                           kNameSpaceID_XMLNS);
  if (nsnull != xmlns) {
    NameSpaceImpl* xml = new NameSpaceImpl(this, xmlns,
                                           nsLayoutAtoms::xmlNameSpace,
                                           kNameSpaceID_XML);
    if (xml) {
      rv = CallQueryInterface(xml, &aRootNameSpace);
    }
    else {
      delete xmlns;
    }
  }
  return rv;
}

NS_IMETHODIMP
NameSpaceManagerImpl::RegisterNameSpace(const nsAString& aURI, 
                                        PRInt32& aNameSpaceID)
{
  NS_ASSERTION(!gNameSpaceManagerWasShutDown,
               "Namespace manager used past content module shutdown!!!");

  PRInt32 id = FindNameSpaceID(aURI);

  if (kNameSpaceID_Unknown == id) {
    nsString* uri = new nsString(aURI);
    if (!uri)
      return NS_ERROR_OUT_OF_MEMORY;
    gURIArray->AppendElement(uri); 
    id = gURIArray->Count();  // id is index + 1
    nsStringKey key(*uri);
    gURIToIDTable->Put(&key, (void*)id);
  }
  
  aNameSpaceID = id;
  return NS_OK;
}

NS_IMETHODIMP
NameSpaceManagerImpl::GetNameSpaceURI(PRInt32 aNameSpaceID, nsAString& aURI)
{
  NS_ASSERTION(!gNameSpaceManagerWasShutDown,
               "Namespace manager used past content module shutdown!!!");

  const nsString* result = FindNameSpaceURI(aNameSpaceID);
  if (result) {
    aURI = *result;
    return NS_OK;
  }
  aURI.Truncate();
  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
NameSpaceManagerImpl::GetNameSpaceID(const nsAString& aURI, PRInt32& aNameSpaceID)
{
  NS_ASSERTION(!gNameSpaceManagerWasShutDown,
               "Namespace manager used past content module shutdown!!!");

  aNameSpaceID = FindNameSpaceID(aURI);
  return NS_OK;
}

NS_IMETHODIMP
NameSpaceManagerImpl::GetElementFactory(PRInt32 aNameSpaceID,
                                        nsIElementFactory **aElementFactory)
{
  NS_ASSERTION(!gNameSpaceManagerWasShutDown,
               "Namespace manager used past content module shutdown!!!");

  *aElementFactory = nsnull;

  NS_ENSURE_TRUE(gElementFactoryArray, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(aNameSpaceID >= 0, NS_ERROR_ILLEGAL_VALUE);

  gElementFactoryArray->QueryElementAt(aNameSpaceID,
                                       NS_GET_IID(nsIElementFactory),
                                       (void **)aElementFactory);

  if (*aElementFactory) {
    return NS_OK;
  }

  nsAutoString uri;

  GetNameSpaceURI(aNameSpaceID, uri);

  nsCOMPtr<nsIElementFactory> ef;

  if (!uri.IsEmpty()) {
    nsCAutoString contract_id(NS_ELEMENT_FACTORY_CONTRACTID_PREFIX);
    contract_id.Append(NS_ConvertUCS2toUTF8(uri));

    ef = do_GetService(contract_id.get());
  }

  if (!ef) {
    ef = gDefaultElementFactory;
  }

  PRUint32 count = 0;
  gElementFactoryArray->Count(&count);

  if ((PRUint32)aNameSpaceID < count) {
    gElementFactoryArray->ReplaceElementAt(ef, aNameSpaceID);
  } else {
    // This sucks, simply doing an InsertElementAt() should IMNSHO
    // automatically grow the array and insert null's as needed to
    // fill up the array!?!!

    for (PRInt32 i = count; i < aNameSpaceID; i++) {
      gElementFactoryArray->AppendElement(nsnull);
    }

    gElementFactoryArray->AppendElement(ef);
  }

  *aElementFactory = ef;
  NS_ADDREF(*aElementFactory);

  return NS_OK;
}

NS_IMETHODIMP
NameSpaceManagerImpl::HasRegisteredFactory(PRInt32 aNameSpaceID,
                                           PRBool* aHasFactory)
{
  *aHasFactory = PR_FALSE;
  nsCOMPtr<nsIElementFactory> ef;
  GetElementFactory(aNameSpaceID, getter_AddRefs(ef));
  NS_ENSURE_TRUE(gDefaultElementFactory, NS_ERROR_FAILURE);
  *aHasFactory = ef != gDefaultElementFactory;
  return NS_OK;
}

nsresult
NS_NewNameSpaceManager(nsINameSpaceManager** aInstancePtrResult)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  *aInstancePtrResult = new NameSpaceManagerImpl();

  if (!*aInstancePtrResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}
