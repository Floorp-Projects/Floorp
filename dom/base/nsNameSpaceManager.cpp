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
#include "nsAtom.h"
#include "nsCOMArray.h"
#include "nsContentCreatorFunctions.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/Document.h"
#include "nsString.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;

static constexpr const char* kPrefSVGDisabled = "svg.disabled";
static constexpr const char* kPrefMathMLDisabled = "mathml.disabled";
static constexpr const char* kObservedNSPrefs[] = {kPrefMathMLDisabled,
                                                   kPrefSVGDisabled, nullptr};

StaticRefPtr<nsNameSpaceManager> nsNameSpaceManager::sInstance;

/* static */
nsNameSpaceManager* nsNameSpaceManager::GetInstance() {
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

bool nsNameSpaceManager::Init() {
  nsresult rv;
#define REGISTER_NAMESPACE(uri, id)        \
  rv = AddNameSpace(dont_AddRef(uri), id); \
  NS_ENSURE_SUCCESS(rv, false)

#define REGISTER_DISABLED_NAMESPACE(uri, id)       \
  rv = AddDisabledNameSpace(dont_AddRef(uri), id); \
  NS_ENSURE_SUCCESS(rv, false)

  mozilla::Preferences::RegisterCallbacks(nsNameSpaceManager::PrefChanged,
                                          kObservedNSPrefs, this);

  PrefChanged(nullptr);

  // Need to be ordered according to ID.
  MOZ_ASSERT(mURIArray.IsEmpty());
  REGISTER_NAMESPACE(nsGkAtoms::_empty, kNameSpaceID_None);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_xmlns, kNameSpaceID_XMLNS);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_xml, kNameSpaceID_XML);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_xhtml, kNameSpaceID_XHTML);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_xlink, kNameSpaceID_XLink);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_xslt, kNameSpaceID_XSLT);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_mathml, kNameSpaceID_MathML);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_rdf, kNameSpaceID_RDF);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_xul, kNameSpaceID_XUL);
  REGISTER_NAMESPACE(nsGkAtoms::nsuri_svg, kNameSpaceID_SVG);
  REGISTER_DISABLED_NAMESPACE(nsGkAtoms::nsuri_mathml,
                              kNameSpaceID_disabled_MathML);
  REGISTER_DISABLED_NAMESPACE(nsGkAtoms::nsuri_svg, kNameSpaceID_disabled_SVG);

#undef REGISTER_NAMESPACE
#undef REGISTER_DISABLED_NAMESPACE

  return true;
}

nsresult nsNameSpaceManager::RegisterNameSpace(const nsAString& aURI,
                                               int32_t& aNameSpaceID) {
  RefPtr<nsAtom> atom = NS_Atomize(aURI);
  return RegisterNameSpace(atom.forget(), aNameSpaceID);
}

nsresult nsNameSpaceManager::RegisterNameSpace(already_AddRefed<nsAtom> aURI,
                                               int32_t& aNameSpaceID) {
  RefPtr<nsAtom> atom = aURI;
  nsresult rv = NS_OK;
  if (atom == nsGkAtoms::_empty) {
    aNameSpaceID = kNameSpaceID_None;  // xmlns="", see bug 75700 for details
    return NS_OK;
  }

  if (!mURIToIDTable.Get(atom, &aNameSpaceID)) {
    aNameSpaceID = mURIArray.Length();

    rv = AddNameSpace(atom.forget(), aNameSpaceID);
    if (NS_FAILED(rv)) {
      aNameSpaceID = kNameSpaceID_Unknown;
    }
  }

  MOZ_ASSERT(aNameSpaceID >= -1, "Bogus namespace ID");

  return rv;
}

nsresult nsNameSpaceManager::GetNameSpaceURI(int32_t aNameSpaceID,
                                             nsAString& aURI) {
  MOZ_ASSERT(aNameSpaceID >= 0, "Bogus namespace ID");

  // We have historically treated GetNameSpaceURI calls for kNameSpaceID_None
  // as erroneous.
  if (aNameSpaceID <= 0 || aNameSpaceID >= int32_t(mURIArray.Length())) {
    aURI.Truncate();

    return NS_ERROR_ILLEGAL_VALUE;
  }

  mURIArray.ElementAt(aNameSpaceID)->ToString(aURI);

  return NS_OK;
}

int32_t nsNameSpaceManager::GetNameSpaceID(const nsAString& aURI,
                                           bool aInChromeDoc) {
  if (aURI.IsEmpty()) {
    return kNameSpaceID_None;  // xmlns="", see bug 75700 for details
  }

  RefPtr<nsAtom> atom = NS_Atomize(aURI);
  return GetNameSpaceID(atom, aInChromeDoc);
}

int32_t nsNameSpaceManager::GetNameSpaceID(nsAtom* aURI, bool aInChromeDoc) {
  if (aURI == nsGkAtoms::_empty) {
    return kNameSpaceID_None;  // xmlns="", see bug 75700 for details
  }

  int32_t nameSpaceID;
  if (!aInChromeDoc && (mMathMLDisabled || mSVGDisabled) &&
      mDisabledURIToIDTable.Get(aURI, &nameSpaceID) &&
      ((mMathMLDisabled && kNameSpaceID_disabled_MathML == nameSpaceID) ||
       (mSVGDisabled && kNameSpaceID_disabled_SVG == nameSpaceID))) {
    MOZ_ASSERT(nameSpaceID >= 0, "Bogus namespace ID");
    return nameSpaceID;
  }
  if (mURIToIDTable.Get(aURI, &nameSpaceID)) {
    MOZ_ASSERT(nameSpaceID >= 0, "Bogus namespace ID");
    return nameSpaceID;
  }

  return kNameSpaceID_Unknown;
}

// static
const char* nsNameSpaceManager::GetNameSpaceDisplayName(uint32_t aNameSpaceID) {
  static const char* kNSURIs[] = {"([none])", "(xmlns)", "(xml)",    "(xhtml)",
                                  "(XLink)",  "(XSLT)",  "(MathML)", "(RDF)",
                                  "(XUL)",    "(SVG)"};
  if (aNameSpaceID < ArrayLength(kNSURIs)) {
    return kNSURIs[aNameSpaceID];
  }
  return "";
}

nsresult NS_NewElement(Element** aResult,
                       already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                       FromParser aFromParser, const nsAString* aIs) {
  RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  int32_t ns = ni->NamespaceID();
  RefPtr<nsAtom> isAtom = aIs ? NS_AtomizeMainThread(*aIs) : nullptr;
  if (ns == kNameSpaceID_XHTML) {
    return NS_NewHTMLElement(aResult, ni.forget(), aFromParser, isAtom);
  }
  if (ns == kNameSpaceID_XUL) {
    return NS_NewXULElement(aResult, ni.forget(), aFromParser, isAtom);
  }
  if (ns == kNameSpaceID_MathML) {
    // If the mathml.disabled pref. is true, convert all MathML nodes into
    // disabled MathML nodes by swapping the namespace.
    if (ni->NodeInfoManager()->MathMLEnabled()) {
      return NS_NewMathMLElement(aResult, ni.forget());
    }

    RefPtr<mozilla::dom::NodeInfo> genericXMLNI =
        ni->NodeInfoManager()->GetNodeInfo(ni->NameAtom(), ni->GetPrefixAtom(),
                                           kNameSpaceID_disabled_MathML,
                                           ni->NodeType(), ni->GetExtraName());
    return NS_NewXMLElement(aResult, genericXMLNI.forget());
  }
  if (ns == kNameSpaceID_SVG) {
    // If the svg.disabled pref. is true, convert all SVG nodes into
    // disabled SVG nodes by swapping the namespace.
    if (ni->NodeInfoManager()->SVGEnabled()) {
      return NS_NewSVGElement(aResult, ni.forget(), aFromParser);
    }
    RefPtr<mozilla::dom::NodeInfo> genericXMLNI =
        ni->NodeInfoManager()->GetNodeInfo(ni->NameAtom(), ni->GetPrefixAtom(),
                                           kNameSpaceID_disabled_SVG,
                                           ni->NodeType(), ni->GetExtraName());
    return NS_NewXMLElement(aResult, genericXMLNI.forget());
  }

  return NS_NewXMLElement(aResult, ni.forget());
}

bool nsNameSpaceManager::HasElementCreator(int32_t aNameSpaceID) {
  return aNameSpaceID == kNameSpaceID_XHTML ||
         aNameSpaceID == kNameSpaceID_XUL ||
         aNameSpaceID == kNameSpaceID_MathML ||
         aNameSpaceID == kNameSpaceID_SVG || false;
}

nsresult nsNameSpaceManager::AddNameSpace(already_AddRefed<nsAtom> aURI,
                                          const int32_t aNameSpaceID) {
  RefPtr<nsAtom> uri = aURI;
  if (aNameSpaceID < 0) {
    // We've wrapped...  Can't do anything else here; just bail.
    return NS_ERROR_OUT_OF_MEMORY;
  }

  MOZ_ASSERT(aNameSpaceID == (int32_t)mURIArray.Length());
  mURIArray.AppendElement(uri.forget());
  mURIToIDTable.InsertOrUpdate(mURIArray.LastElement(), aNameSpaceID);

  return NS_OK;
}

nsresult nsNameSpaceManager::AddDisabledNameSpace(already_AddRefed<nsAtom> aURI,
                                                  const int32_t aNameSpaceID) {
  RefPtr<nsAtom> uri = aURI;
  if (aNameSpaceID < 0) {
    // We've wrapped...  Can't do anything else here; just bail.
    return NS_ERROR_OUT_OF_MEMORY;
  }

  MOZ_ASSERT(aNameSpaceID == (int32_t)mURIArray.Length());
  mURIArray.AppendElement(uri.forget());
  mDisabledURIToIDTable.InsertOrUpdate(mURIArray.LastElement(), aNameSpaceID);

  return NS_OK;
}

// static
void nsNameSpaceManager::PrefChanged(const char* aPref, void* aSelf) {
  static_cast<nsNameSpaceManager*>(aSelf)->PrefChanged(aPref);
}

void nsNameSpaceManager::PrefChanged(const char* aPref) {
  mMathMLDisabled = mozilla::Preferences::GetBool(kPrefMathMLDisabled);
  mSVGDisabled = mozilla::Preferences::GetBool(kPrefSVGDisabled);
}
