/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#ifdef MOZ_XTF
#include "nsIServiceManager.h"
#include "nsIXTFService.h"
#include "nsContentUtils.h"
static NS_DEFINE_CID(kXTFServiceCID, NS_XTFSERVICE_CID);
#endif

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
#define kXMLEventsNameSpaceURI "http://www.w3.org/2001/xml-events"

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
  virtual ~NameSpaceManagerImpl()
  {
  }

  NS_DECL_ISUPPORTS

  nsresult Init();

  nsresult RegisterNameSpace(const nsAString& aURI,  PRInt32& aNameSpaceID);

  nsresult GetNameSpaceURI(PRInt32 aNameSpaceID, nsAString& aURI);
  PRInt32 GetNameSpaceID(const nsAString& aURI);

  bool HasElementCreator(PRInt32 aNameSpaceID);

private:
  nsresult AddNameSpace(const nsAString& aURI, const PRInt32 aNameSpaceID);

  nsDataHashtable<nsNameSpaceKey,PRInt32> mURIToIDTable;
  nsTArray< nsAutoPtr<nsString> > mURIArray;
};

static NameSpaceManagerImpl* sNameSpaceManager = nsnull;

NS_IMPL_ISUPPORTS1(NameSpaceManagerImpl, nsINameSpaceManager)

nsresult NameSpaceManagerImpl::Init()
{
  nsresult rv = mURIToIDTable.Init(32);
  NS_ENSURE_SUCCESS(rv, rv);

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
  REGISTER_NAMESPACE(kXMLEventsNameSpaceURI, kNameSpaceID_XMLEvents);

#undef REGISTER_NAMESPACE

  return NS_OK;
}

nsresult
NameSpaceManagerImpl::RegisterNameSpace(const nsAString& aURI, 
                                        PRInt32& aNameSpaceID)
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
NameSpaceManagerImpl::GetNameSpaceURI(PRInt32 aNameSpaceID, nsAString& aURI)
{
  NS_PRECONDITION(aNameSpaceID >= 0, "Bogus namespace ID");
  
  PRInt32 index = aNameSpaceID - 1; // id is index + 1
  if (index < 0 || index >= PRInt32(mURIArray.Length())) {
    aURI.Truncate();

    return NS_ERROR_ILLEGAL_VALUE;
  }

  aURI = *mURIArray.ElementAt(index);

  return NS_OK;
}

PRInt32
NameSpaceManagerImpl::GetNameSpaceID(const nsAString& aURI)
{
  if (aURI.IsEmpty()) {
    return kNameSpaceID_None; // xmlns="", see bug 75700 for details
  }

  PRInt32 nameSpaceID;

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
  PRInt32 ns = aNodeInfo.get()->NamespaceID();
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
  if (ns == kNameSpaceID_XMLEvents) {
    return NS_NewXMLEventsElement(aResult, aNodeInfo);
  }
#ifdef MOZ_XTF
  if (ns > kNameSpaceID_LastBuiltin) {
    nsIXTFService* xtfService = nsContentUtils::GetXTFService();
    NS_ASSERTION(xtfService, "could not get xtf service");
    if (xtfService &&
        NS_SUCCEEDED(xtfService->CreateElement(aResult, aNodeInfo)))
      return NS_OK;
  }
#endif
  return NS_NewXMLElement(aResult, aNodeInfo);
}

bool
NameSpaceManagerImpl::HasElementCreator(PRInt32 aNameSpaceID)
{
  return aNameSpaceID == kNameSpaceID_XHTML ||
#ifdef MOZ_XUL
         aNameSpaceID == kNameSpaceID_XUL ||
#endif
         aNameSpaceID == kNameSpaceID_MathML ||
         aNameSpaceID == kNameSpaceID_SVG ||
         aNameSpaceID == kNameSpaceID_XMLEvents ||
         false;
}

nsresult NameSpaceManagerImpl::AddNameSpace(const nsAString& aURI,
                                            const PRInt32 aNameSpaceID)
{
  if (aNameSpaceID < 0) {
    // We've wrapped...  Can't do anything else here; just bail.
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  NS_ASSERTION(aNameSpaceID - 1 == (PRInt32) mURIArray.Length(),
               "BAD! AddNameSpace not called in right order!");

  nsString* uri = new nsString(aURI);
  if (!uri || !mURIArray.AppendElement(uri)) {
    delete uri;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!mURIToIDTable.Put(uri, aNameSpaceID)) {
    mURIArray.RemoveElementAt(aNameSpaceID - 1);

    return NS_ERROR_OUT_OF_MEMORY;
  }

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
