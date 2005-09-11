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
#include "nscore.h"
#include "nsINameSpaceManager.h"
#include "nsAutoPtr.h"
#include "nsINodeInfo.h"
#include "nsCOMArray.h"
#include "nsContentCreatorFunctions.h"
#include "nsDoubleHashtable.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"

#ifdef MOZ_XTF
#include "nsIServiceManager.h"
#include "nsIXTFService.h"
#include "nsContentUtils.h"
static NS_DEFINE_CID(kXTFServiceCID, NS_XTFSERVICE_CID);
#endif

#ifdef MOZ_SVG
#include "nsSVGUtils.h"
#endif

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
#define kXHTML2UnofficialNameSpaceURI "http://www.w3.org/TR/xhtml2" // Will eventually change
#define kWAIRolesNameSpaceURI "http://www.w3.org/2005/01/wai-rdf/GUIRoleTaxonomy#"
#define kWAIPropertiesNameSpaceURI "http://www.w3.org/2005/07/aaa"

class nsNameSpaceEntry : public PLDHashEntryHdr
{
public:
  nsNameSpaceEntry(const void* aKey) :
    mKey(NS_STATIC_CAST(const nsAString*, aKey))
  {
  }
  ~nsNameSpaceEntry()
  {
  }
  const void* GetKey() const {
    return mKey;
  }
  static PLDHashNumber HashKey(const void* aKey) {
    return HashString(*NS_STATIC_CAST(const nsAString*, aKey));
  }
  PRBool MatchEntry(const void* aKey) const {
    return NS_STATIC_CAST(const nsAString*, aKey)->Equals(*mKey);
  }

  const nsAString* mKey;
  PRInt32 mNameSpaceID;
};

DECL_DHASH_WRAPPER(nsNameSpaceHashBase, nsNameSpaceEntry, nsAString&)
DHASH_WRAPPER(nsNameSpaceHashBase, nsNameSpaceEntry, nsAString&)

class nsNameSpaceHash : public nsNameSpaceHashBase
{
public:
  nsNameSpaceHash()
  {
  }
  ~nsNameSpaceHash()
  {
  }
  nsNameSpaceEntry* LookupEntry(const nsAString& aUri)
  {
    return NS_STATIC_CAST(nsNameSpaceEntry*,
                          PL_DHashTableOperate(&mHashTable, &aUri,
                                               PL_DHASH_LOOKUP));
  }
};

class NameSpaceManagerImpl : public nsINameSpaceManager {
public:
  NameSpaceManagerImpl();
  virtual ~NameSpaceManagerImpl();

  NS_DECL_ISUPPORTS

  nsresult Init();

  NS_IMETHOD RegisterNameSpace(const nsAString& aURI, 
			                         PRInt32& aNameSpaceID);

  NS_IMETHOD GetNameSpaceURI(PRInt32 aNameSpaceID, nsAString& aURI);
  NS_IMETHOD GetNameSpaceID(const nsAString& aURI,
                            PRInt32* aNameSpaceID);

  PRBool HasElementCreator(PRInt32 aNameSpaceID);

  PRBool HasNameSpaceURI(PRInt32 aNameSpaceID);

private:
  // These are not supported and are not implemented!
  NameSpaceManagerImpl(const NameSpaceManagerImpl& aCopy);
  NameSpaceManagerImpl& operator=(const NameSpaceManagerImpl& aCopy);

  nsresult AddNameSpace(const nsAString& aURI, const PRInt32 aNameSpaceID);

  nsNameSpaceHash mURIToIDTable;
  nsStringArray mURIArray;
};

static NameSpaceManagerImpl* gNameSpaceManager = nsnull;


NameSpaceManagerImpl::NameSpaceManagerImpl()
{
}

NameSpaceManagerImpl::~NameSpaceManagerImpl()
{
}

NS_IMPL_ISUPPORTS1(NameSpaceManagerImpl, nsINameSpaceManager)

nsresult NameSpaceManagerImpl::Init()
{
  nsresult rv = mURIToIDTable.Init(32);
  NS_ENSURE_SUCCESS(rv, rv);

  // Need to be ordered according to ID.
  AddNameSpace(NS_LITERAL_STRING(kXMLNSNameSpaceURI), kNameSpaceID_XMLNS);
  AddNameSpace(NS_LITERAL_STRING(kXMLNameSpaceURI), kNameSpaceID_XML);
  AddNameSpace(NS_LITERAL_STRING(kXHTMLNameSpaceURI), kNameSpaceID_XHTML);
  AddNameSpace(NS_LITERAL_STRING(kXLinkNameSpaceURI), kNameSpaceID_XLink);
  AddNameSpace(NS_LITERAL_STRING(kXSLTNameSpaceURI), kNameSpaceID_XSLT);
  AddNameSpace(NS_LITERAL_STRING(kXBLNameSpaceURI), kNameSpaceID_XBL);
  AddNameSpace(NS_LITERAL_STRING(kMathMLNameSpaceURI), kNameSpaceID_MathML);
  AddNameSpace(NS_LITERAL_STRING(kRDFNameSpaceURI), kNameSpaceID_RDF);
  AddNameSpace(NS_LITERAL_STRING(kXULNameSpaceURI), kNameSpaceID_XUL);
  AddNameSpace(NS_LITERAL_STRING(kSVGNameSpaceURI), kNameSpaceID_SVG);
  AddNameSpace(NS_LITERAL_STRING(kXMLEventsNameSpaceURI), kNameSpaceID_XMLEvents);
  AddNameSpace(NS_LITERAL_STRING(kXHTML2UnofficialNameSpaceURI), kNameSpaceID_XHTML2_Unofficial);
  AddNameSpace(NS_LITERAL_STRING(kWAIRolesNameSpaceURI), kNameSpaceID_WAIRoles);
  AddNameSpace(NS_LITERAL_STRING(kWAIPropertiesNameSpaceURI), kNameSpaceID_WAIProperties);

  return NS_OK;
}

NS_IMETHODIMP
NameSpaceManagerImpl::RegisterNameSpace(const nsAString& aURI, 
                                        PRInt32& aNameSpaceID)
{
  if (aURI.IsEmpty()) {
    aNameSpaceID = kNameSpaceID_None; // xmlns="", see bug 75700 for details

    return NS_OK;
  }

  nsNameSpaceEntry* entry = mURIToIDTable.GetEntry(aURI);
  if (entry) {
    aNameSpaceID = entry->mNameSpaceID;

    return NS_OK;
  }

  aNameSpaceID = mURIArray.Count() + 1; // id is index + 1

  nsresult rv = AddNameSpace(aURI, aNameSpaceID);
  if (NS_FAILED(rv)) {
    aNameSpaceID = kNameSpaceID_Unknown;
  }

  return rv;
}

NS_IMETHODIMP
NameSpaceManagerImpl::GetNameSpaceURI(PRInt32 aNameSpaceID, nsAString& aURI)
{
  PRInt32 index = aNameSpaceID - 1; // id is index + 1
  if (index < 0 || index >= mURIArray.Count()) {
    aURI.Truncate();

    return NS_ERROR_ILLEGAL_VALUE;
  }

  mURIArray.StringAt(index, aURI);

  return NS_OK;
}

NS_IMETHODIMP
NameSpaceManagerImpl::GetNameSpaceID(const nsAString& aURI, PRInt32* aNameSpaceID)
{
  if (aURI.IsEmpty()) {
    *aNameSpaceID = kNameSpaceID_None; // xmlns="", see bug 75700 for details

    return NS_OK;
  }

  nsNameSpaceEntry* entry = mURIToIDTable.GetEntry(aURI);

  *aNameSpaceID = entry ? entry->mNameSpaceID : kNameSpaceID_Unknown;

  return NS_OK;
}

nsresult
NS_NewElement(nsIContent** aResult, PRInt32 aElementType,
              nsINodeInfo* aNodeInfo)
{
  if (aElementType == kNameSpaceID_XHTML) {
    return NS_NewHTMLElement(aResult, aNodeInfo);
  }
#ifdef MOZ_XUL
  if (aElementType == kNameSpaceID_XUL) {
    return NS_NewXULElement(aResult, aNodeInfo);
  }
#endif
#ifdef MOZ_MATHML
  if (aElementType == kNameSpaceID_MathML) {
    return NS_NewMathMLElement(aResult, aNodeInfo);
  }
#endif
#ifdef MOZ_SVG
  if (aElementType == kNameSpaceID_SVG &&
      nsSVGUtils::SVGEnabled()) {
    return NS_NewSVGElement(aResult, aNodeInfo);
  }
#endif
  if (aElementType == kNameSpaceID_XMLEvents) {
    return NS_NewXMLEventsElement(aResult, aNodeInfo);
  }
#ifdef MOZ_XTF
  if (aElementType > kNameSpaceID_LastBuiltin) {
    nsIXTFService* xtfService = nsContentUtils::GetXTFService();
    NS_ASSERTION(xtfService, "could not get xtf service");
    if (xtfService &&
        NS_SUCCEEDED(xtfService->CreateElement(aResult, aNodeInfo)))
      return NS_OK;
  }
#endif
  return NS_NewXMLElement(aResult, aNodeInfo);
}

PRBool
NameSpaceManagerImpl::HasElementCreator(PRInt32 aNameSpaceID)
{
  return aNameSpaceID == kNameSpaceID_XHTML ||
#ifdef MOZ_XUL
         aNameSpaceID == kNameSpaceID_XUL ||
#endif
#ifdef MOZ_MATHML
         aNameSpaceID == kNameSpaceID_MathML ||
#endif
#ifdef MOZ_SVG
         aNameSpaceID == kNameSpaceID_SVG ||
#endif
         aNameSpaceID == kNameSpaceID_XMLEvents ||
         PR_FALSE;
}

PRBool
NameSpaceManagerImpl::HasNameSpaceURI(PRInt32 aNameSpaceID)
{
  return (aNameSpaceID > kNameSpaceID_None &&
          aNameSpaceID <= mURIArray.Count());
}

nsresult NameSpaceManagerImpl::AddNameSpace(const nsAString& aURI,
                                            const PRInt32 aNameSpaceID)
{
  NS_ASSERTION(aNameSpaceID - 1 == mURIArray.Count(),
               "BAD! AddNameSpace not called in right order!");

  if (!mURIArray.AppendString(aURI)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  const nsString* uri = mURIArray.StringAt(aNameSpaceID - 1);
  nsNameSpaceEntry* entry = mURIToIDTable.AddEntry(*uri);
  if (!entry) {
    mURIArray.RemoveStringAt(aNameSpaceID - 1);

    return NS_ERROR_OUT_OF_MEMORY;
  }

  entry->mNameSpaceID = aNameSpaceID;

  return NS_OK;
}

nsresult
NS_GetNameSpaceManager(nsINameSpaceManager** aInstancePtrResult)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  if (!gNameSpaceManager) {
    nsCOMPtr<NameSpaceManagerImpl> manager = new NameSpaceManagerImpl();
    if (manager) {
      nsresult rv = manager->Init();
      if (NS_SUCCEEDED(rv)) {
        manager.swap(gNameSpaceManager);
      }
    }
  }

  *aInstancePtrResult = gNameSpaceManager;
  NS_ENSURE_TRUE(gNameSpaceManager, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

void
NS_NameSpaceManagerShutdown()
{
  NS_IF_RELEASE(gNameSpaceManager);
}
