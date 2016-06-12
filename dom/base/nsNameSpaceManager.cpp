/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class for managing namespace IDs and mapping back and forth
 * between namespace IDs and namespace URIs.
 */

#include "nsNameSpaceManager.h"

#include "nscore.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsCOMArray.h"
#include "nsContentCreatorFunctions.h"
#include "nsGkAtoms.h"
#include "nsString.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/XBLChildrenElement.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::dom;

StaticAutoPtr<nsNameSpaceManager> nsNameSpaceManager::sInstance;

/* static */ nsNameSpaceManager*
nsNameSpaceManager::GetInstance() {
  if (!sInstance) {
    sInstance = new nsNameSpaceManager();
    if (sInstance->Init()) {
      ClearOnShutdown(&sInstance);
    } else {
      delete sInstance;
      sInstance = nullptr;
    }
  }

  return sInstance;
}

bool nsNameSpaceManager::Init()
{
  nsresult rv;
#define REGISTER_NAMESPACE(uri, id) \
  rv = AddNameSpace(dont_AddRef(uri), id); \
  NS_ENSURE_SUCCESS(rv, false)

  // Need to be ordered according to ID.
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_xmlns, kNameSpaceID_XMLNS);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_xml, kNameSpaceID_XML);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_xhtml, kNameSpaceID_XHTML);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_xlink, kNameSpaceID_XLink);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_xslt, kNameSpaceID_XSLT);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_xbl, kNameSpaceID_XBL);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_mathml, kNameSpaceID_MathML);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_rdf, kNameSpaceID_RDF);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_xul, kNameSpaceID_XUL);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_svg, kNameSpaceID_SVG);

#undef REGISTER_NAMESPACE

  return true;
}

nsresult
nsNameSpaceManager::RegisterNameSpace(const nsAString& aURI,
                                      int32_t& aNameSpaceID)
{
  if (aURI.IsEmpty()) {
    aNameSpaceID = kNameSpaceID_None; // xmlns="", see bug 75700 for details

    return NS_OK;
  }

  nsCOMPtr<nsIAtom> atom = NS_Atomize(aURI);
  nsresult rv = NS_OK;
  if (!mURIToIDTable.Get(atom, &aNameSpaceID)) {
    aNameSpaceID = mURIArray.Length() + 1; // id is index + 1

    rv = AddNameSpace(atom.forget(), aNameSpaceID);
    if (NS_FAILED(rv)) {
      aNameSpaceID = kNameSpaceID_Unknown;
    }
  }

  NS_POSTCONDITION(aNameSpaceID >= -1, "Bogus namespace ID");
  
  return rv;
}

nsresult
nsNameSpaceManager::GetNameSpaceURI(int32_t aNameSpaceID, nsAString& aURI)
{
  NS_PRECONDITION(aNameSpaceID >= 0, "Bogus namespace ID");
  
  int32_t index = aNameSpaceID - 1; // id is index + 1
  if (index < 0 || index >= int32_t(mURIArray.Length())) {
    aURI.Truncate();

    return NS_ERROR_ILLEGAL_VALUE;
  }

  mURIArray.ElementAt(index)->ToString(aURI);

  return NS_OK;
}

int32_t
nsNameSpaceManager::GetNameSpaceID(const nsAString& aURI)
{
  if (aURI.IsEmpty()) {
    return kNameSpaceID_None; // xmlns="", see bug 75700 for details
  }

  int32_t nameSpaceID;

  nsCOMPtr<nsIAtom> atom = NS_Atomize(aURI);
  if (mURIToIDTable.Get(atom, &nameSpaceID)) {
    NS_POSTCONDITION(nameSpaceID >= 0, "Bogus namespace ID");
    return nameSpaceID;
  }

  return kNameSpaceID_Unknown;
}

nsresult
NS_NewElement(Element** aResult,
              already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
              FromParser aFromParser)
{
  RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  int32_t ns = ni->NamespaceID();
  if (ns == kNameSpaceID_XHTML) {
    return NS_NewHTMLElement(aResult, ni.forget(), aFromParser);
  }
#ifdef MOZ_XUL
  if (ns == kNameSpaceID_XUL) {
    return NS_NewXULElement(aResult, ni.forget());
  }
#endif
  if (ns == kNameSpaceID_MathML) {
    return NS_NewMathMLElement(aResult, ni.forget());
  }
  if (ns == kNameSpaceID_SVG) {
    return NS_NewSVGElement(aResult, ni.forget(), aFromParser);
  }
  if (ns == kNameSpaceID_XBL && ni->Equals(nsGkAtoms::children)) {
    NS_ADDREF(*aResult = new XBLChildrenElement(ni.forget()));
    return NS_OK;
  }

  return NS_NewXMLElement(aResult, ni.forget());
}

bool
nsNameSpaceManager::HasElementCreator(int32_t aNameSpaceID)
{
  return aNameSpaceID == kNameSpaceID_XHTML ||
#ifdef MOZ_XUL
         aNameSpaceID == kNameSpaceID_XUL ||
#endif
         aNameSpaceID == kNameSpaceID_MathML ||
         aNameSpaceID == kNameSpaceID_SVG ||
         false;
}

nsresult nsNameSpaceManager::AddNameSpace(already_AddRefed<nsIAtom> aURI,
                                          const int32_t aNameSpaceID)
{
  nsCOMPtr<nsIAtom> uri = aURI;
  if (aNameSpaceID < 0) {
    // We've wrapped...  Can't do anything else here; just bail.
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  NS_ASSERTION(aNameSpaceID - 1 == (int32_t) mURIArray.Length(),
               "BAD! AddNameSpace not called in right order!");

  mURIArray.AppendElement(uri.forget());
  mURIToIDTable.Put(mURIArray.LastElement(), aNameSpaceID);

  return NS_OK;
}
