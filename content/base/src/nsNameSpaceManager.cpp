/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class for managing namespace IDs and mapping back and forth
 * between namespace IDs and namespace URIs.
 */

#include "nscore.h"
#include "nsINameSpaceManager.h"
#include "nsAutoPtr.h"
#include "nsINodeInfo.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsContentCreatorFunctions.h"
#include "nsDataHashtable.h"
#include "nsString.h"
#include "nsINodeInfo.h"
#include "mozilla/dom/XBLChildrenElement.h"

using namespace mozilla;
using namespace mozilla::dom;

#define kXMLNSNameSpaceURI "http://www.w3.org/2000/xmlns/"
#define kXMLNameSpaceURI "http://www.w3.org/XML/1998/namespace"
#define kXHTMLNameSpaceURI "http://www.w3.org/1999/xhtml"
#define kXLinkNameSpaceURI "http://www.w3.org/1999/xlink"
#define kXSLTNameSpaceURI "http://www.w3.org/1999/XSL/Transform"
#define kXBLNameSpaceURI "http://www.mozilla.org/xbl"
#define kMathMLNameSpaceURI "http://www.w3.org/1998/Math/MathML"
#define kRDFNameSpaceURI "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define kXULNameSpaceURI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
#define kSVGNameSpaceURI "http://www.w3.org/2000/svg"

class nsNameSpaceKey : public PLDHashEntryHdr
{
public:
  typedef const nsAString* KeyType;
  typedef const nsAString* KeyTypePointer;

  nsNameSpaceKey(KeyTypePointer aKey) : mKey(aKey)
  {
  }
  nsNameSpaceKey(const nsNameSpaceKey& toCopy) : mKey(toCopy.mKey)
  {
  }

  KeyType GetKey() const
  {
    return mKey;
  }
  bool KeyEquals(KeyType aKey) const
  {
    return mKey->Equals(*aKey);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey)
  {
    return aKey;
  }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return HashString(*aKey);
  }

  enum { 
    ALLOW_MEMMOVE = true
  };

private:
  const nsAString* mKey;
};

class NameSpaceManagerImpl : public nsINameSpaceManager {
public:
  NameSpaceManagerImpl()
    : mURIToIDTable(32)
  {
  }
  virtual ~NameSpaceManagerImpl()
  {
  }

  NS_DECL_ISUPPORTS

  nsresult Init();

  nsresult RegisterNameSpace(const nsAString& aURI,  int32_t& aNameSpaceID);

  nsresult GetNameSpaceURI(int32_t aNameSpaceID, nsAString& aURI);
  int32_t GetNameSpaceID(const nsAString& aURI);

  bool HasElementCreator(int32_t aNameSpaceID);

private:
  nsresult AddNameSpace(const nsAString& aURI, const int32_t aNameSpaceID);

  nsDataHashtable<nsNameSpaceKey,int32_t> mURIToIDTable;
  nsTArray< nsAutoPtr<nsString> > mURIArray;
};

static NameSpaceManagerImpl* sNameSpaceManager = nullptr;

NS_IMPL_ISUPPORTS1(NameSpaceManagerImpl, nsINameSpaceManager)

nsresult NameSpaceManagerImpl::Init()
{
  nsresult rv;
#define REGISTER_NAMESPACE(uri, id) \
  rv = AddNameSpace(NS_LITERAL_STRING(uri), id); \
  NS_ENSURE_SUCCESS(rv, rv)

  // Need to be ordered according to ID.
  REGISTER_NAMESPACE(kXMLNSNameSpaceURI, kNameSpaceID_XMLNS);
  REGISTER_NAMESPACE(kXMLNameSpaceURI, kNameSpaceID_XML);
  REGISTER_NAMESPACE(kXHTMLNameSpaceURI, kNameSpaceID_XHTML);
  REGISTER_NAMESPACE(kXLinkNameSpaceURI, kNameSpaceID_XLink);
  REGISTER_NAMESPACE(kXSLTNameSpaceURI, kNameSpaceID_XSLT);
  REGISTER_NAMESPACE(kXBLNameSpaceURI, kNameSpaceID_XBL);
  REGISTER_NAMESPACE(kMathMLNameSpaceURI, kNameSpaceID_MathML);
  REGISTER_NAMESPACE(kRDFNameSpaceURI, kNameSpaceID_RDF);
  REGISTER_NAMESPACE(kXULNameSpaceURI, kNameSpaceID_XUL);
  REGISTER_NAMESPACE(kSVGNameSpaceURI, kNameSpaceID_SVG);

#undef REGISTER_NAMESPACE

  return NS_OK;
}

nsresult
NameSpaceManagerImpl::RegisterNameSpace(const nsAString& aURI, 
                                        int32_t& aNameSpaceID)
{
  if (aURI.IsEmpty()) {
    aNameSpaceID = kNameSpaceID_None; // xmlns="", see bug 75700 for details

    return NS_OK;
  }

  nsresult rv = NS_OK;
  if (!mURIToIDTable.Get(&aURI, &aNameSpaceID)) {
    aNameSpaceID = mURIArray.Length() + 1; // id is index + 1

    rv = AddNameSpace(aURI, aNameSpaceID);
    if (NS_FAILED(rv)) {
      aNameSpaceID = kNameSpaceID_Unknown;
    }
  }

  NS_POSTCONDITION(aNameSpaceID >= -1, "Bogus namespace ID");
  
  return rv;
}

nsresult
NameSpaceManagerImpl::GetNameSpaceURI(int32_t aNameSpaceID, nsAString& aURI)
{
  NS_PRECONDITION(aNameSpaceID >= 0, "Bogus namespace ID");
  
  int32_t index = aNameSpaceID - 1; // id is index + 1
  if (index < 0 || index >= int32_t(mURIArray.Length())) {
    aURI.Truncate();

    return NS_ERROR_ILLEGAL_VALUE;
  }

  aURI = *mURIArray.ElementAt(index);

  return NS_OK;
}

int32_t
NameSpaceManagerImpl::GetNameSpaceID(const nsAString& aURI)
{
  if (aURI.IsEmpty()) {
    return kNameSpaceID_None; // xmlns="", see bug 75700 for details
  }

  int32_t nameSpaceID;

  if (mURIToIDTable.Get(&aURI, &nameSpaceID)) {
    NS_POSTCONDITION(nameSpaceID >= 0, "Bogus namespace ID");
    return nameSpaceID;
  }

  return kNameSpaceID_Unknown;
}

nsresult
NS_NewElement(nsIContent** aResult,
              already_AddRefed<nsINodeInfo> aNodeInfo, FromParser aFromParser)
{
  int32_t ns = aNodeInfo.get()->NamespaceID();
  if (ns == kNameSpaceID_XHTML) {
    return NS_NewHTMLElement(aResult, aNodeInfo, aFromParser);
  }
#ifdef MOZ_XUL
  if (ns == kNameSpaceID_XUL) {
    return NS_NewXULElement(aResult, aNodeInfo);
  }
#endif
  if (ns == kNameSpaceID_MathML) {
    return NS_NewMathMLElement(aResult, aNodeInfo);
  }
  if (ns == kNameSpaceID_SVG) {
    return NS_NewSVGElement(aResult, aNodeInfo, aFromParser);
  }
  if (ns == kNameSpaceID_XBL && aNodeInfo.get()->Equals(nsGkAtoms::children)) {
    NS_ADDREF(*aResult = new XBLChildrenElement(aNodeInfo));
    return NS_OK;
  }
  return NS_NewXMLElement(aResult, aNodeInfo);
}

bool
NameSpaceManagerImpl::HasElementCreator(int32_t aNameSpaceID)
{
  return aNameSpaceID == kNameSpaceID_XHTML ||
#ifdef MOZ_XUL
         aNameSpaceID == kNameSpaceID_XUL ||
#endif
         aNameSpaceID == kNameSpaceID_MathML ||
         aNameSpaceID == kNameSpaceID_SVG ||
         false;
}

nsresult NameSpaceManagerImpl::AddNameSpace(const nsAString& aURI,
                                            const int32_t aNameSpaceID)
{
  if (aNameSpaceID < 0) {
    // We've wrapped...  Can't do anything else here; just bail.
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  NS_ASSERTION(aNameSpaceID - 1 == (int32_t) mURIArray.Length(),
               "BAD! AddNameSpace not called in right order!");

  nsString* uri = new nsString(aURI);
  if (!uri || !mURIArray.AppendElement(uri)) {
    delete uri;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mURIToIDTable.Put(uri, aNameSpaceID);

  return NS_OK;
}

nsresult
NS_GetNameSpaceManager(nsINameSpaceManager** aInstancePtrResult)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  if (!sNameSpaceManager) {
    nsCOMPtr<NameSpaceManagerImpl> manager = new NameSpaceManagerImpl();
    if (manager) {
      nsresult rv = manager->Init();
      if (NS_SUCCEEDED(rv)) {
        manager.swap(sNameSpaceManager);
      }
    }
  }

  *aInstancePtrResult = sNameSpaceManager;
  NS_ENSURE_TRUE(sNameSpaceManager, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

void
NS_NameSpaceManagerShutdown()
{
  NS_IF_RELEASE(sNameSpaceManager);
}
