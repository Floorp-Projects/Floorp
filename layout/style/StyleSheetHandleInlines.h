/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSheetHandleInlines_h
#define mozilla_StyleSheetHandleInlines_h

#include "mozilla/CSSStyleSheet.h"
#include "mozilla/ServoStyleSheet.h"

#define FORWARD_CONCRETE(method_, geckoargs_, servoargs_) \
  if (IsGecko()) { \
    return AsGecko()->method_ geckoargs_; \
  } else { \
    return AsServo()->method_ servoargs_; \
  }

#define FORWARD(method_, args_) FORWARD_CONCRETE(method_, args_, args_)

namespace mozilla {

StyleSheet*
StyleSheetHandle::Ptr::AsStyleSheet()
{
  if (IsServo()) {
    return AsServo();
  }
  return AsGecko();
}

const StyleSheet*
StyleSheetHandle::Ptr::AsStyleSheet() const
{
  return const_cast<Ptr*>(this)->AsStyleSheet();
}

MozExternalRefCountType
StyleSheetHandle::Ptr::AddRef()
{
  FORWARD(AddRef, ());
}

MozExternalRefCountType
StyleSheetHandle::Ptr::Release()
{
  FORWARD(Release, ());
}

bool
StyleSheetHandle::Ptr::IsInline() const
{
  FORWARD(IsInline, ());
}

nsIURI*
StyleSheetHandle::Ptr::GetSheetURI() const
{
  FORWARD(GetSheetURI, ());
}

nsIURI*
StyleSheetHandle::Ptr::GetOriginalURI() const
{
  FORWARD(GetOriginalURI, ());
}

nsIURI*
StyleSheetHandle::Ptr::GetBaseURI() const
{
  FORWARD(GetBaseURI, ());
}

void
StyleSheetHandle::Ptr::SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI, nsIURI* aBaseURI)
{
  FORWARD(SetURIs, (aSheetURI, aOriginalSheetURI, aBaseURI));
}

bool
StyleSheetHandle::Ptr::IsApplicable() const
{
  FORWARD(IsApplicable, ());
}

bool
StyleSheetHandle::Ptr::HasRules() const
{
  FORWARD(HasRules, ());
}

nsIDocument*
StyleSheetHandle::Ptr::GetOwningDocument() const
{
  FORWARD(GetOwningDocument, ());
}

void
StyleSheetHandle::Ptr::SetOwningDocument(nsIDocument* aDocument)
{
  FORWARD(SetOwningDocument, (aDocument));
}

nsINode*
StyleSheetHandle::Ptr::GetOwnerNode() const
{
  FORWARD(GetOwnerNode, ());
}

void
StyleSheetHandle::Ptr::SetOwningNode(nsINode* aNode)
{
  FORWARD(SetOwningNode, (aNode));
}

StyleSheetHandle
StyleSheetHandle::Ptr::GetParentSheet() const
{
  FORWARD(GetParentSheet, ());
}

void
StyleSheetHandle::Ptr::AppendStyleSheet(StyleSheetHandle aSheet)
{
  FORWARD_CONCRETE(AppendStyleSheet, (aSheet->AsGecko()), (aSheet->AsServo()));
}

nsIPrincipal*
StyleSheetHandle::Ptr::Principal() const
{
  FORWARD(Principal, ());
}

void
StyleSheetHandle::Ptr::SetPrincipal(nsIPrincipal* aPrincipal)
{
  FORWARD(SetPrincipal, (aPrincipal));
}

mozilla::CORSMode
StyleSheetHandle::Ptr::GetCORSMode() const
{
  FORWARD(GetCORSMode, ());
}

mozilla::net::ReferrerPolicy
StyleSheetHandle::Ptr::GetReferrerPolicy() const
{
  FORWARD(GetReferrerPolicy, ());
}

void
StyleSheetHandle::Ptr::GetIntegrity(dom::SRIMetadata& aResult) const
{
  FORWARD(GetIntegrity, (aResult));
}

size_t
StyleSheetHandle::Ptr::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  FORWARD(SizeOfIncludingThis, (aMallocSizeOf));
}

#ifdef DEBUG
void
StyleSheetHandle::Ptr::List(FILE* aOut, int32_t aIndex) const
{
  FORWARD(List, (aOut, aIndex));
}
#endif

#undef FORWARD

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            StyleSheetHandle& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  if (aField && aField->IsGecko()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCallback, aName);
    aCallback.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIDOMCSSStyleSheet*, aField->AsGecko()));
  }
}

inline void
ImplCycleCollectionUnlink(StyleSheetHandle& aField)
{
  aField = nullptr;
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            StyleSheetHandle::RefPtr& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  if (aField && aField->IsGecko()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCallback, aName);
    aCallback.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIDOMCSSStyleSheet*, aField->AsGecko()));
  }
}

inline void
ImplCycleCollectionUnlink(StyleSheetHandle::RefPtr& aField)
{
  aField = nullptr;
}

} // namespace mozilla

#endif // mozilla_StyleSheetHandleInlines_h
