/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Data from presentational HTML attributes and style attributes */

#include "mozilla/AttributeStyles.h"

#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Element.h"
#include "nsStyleConsts.h"
#include "nsError.h"
#include "nsHashKeys.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/OperatorNewExtensions.h"
#include "mozilla/PresShell.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleSet.h"

using namespace mozilla::dom;

namespace mozilla {

// -----------------------------------------------------------

AttributeStyles::AttributeStyles(Document* aDocument) : mDocument(aDocument) {
  MOZ_ASSERT(aDocument);
}

void AttributeStyles::SetOwningDocument(Document* aDocument) {
  mDocument = aDocument;  // not refcounted
}

void AttributeStyles::Reset() {
  mServoUnvisitedLinkDecl = nullptr;
  mServoVisitedLinkDecl = nullptr;
  mServoActiveLinkDecl = nullptr;
}

nsresult AttributeStyles::ImplLinkColorSetter(
    RefPtr<StyleLockedDeclarationBlock>& aDecl, nscolor aColor) {
  if (!mDocument || !mDocument->GetPresShell()) {
    return NS_OK;
  }

  MOZ_ASSERT(!ServoStyleSet::IsInServoTraversal());
  aDecl = Servo_DeclarationBlock_CreateEmpty().Consume();
  Servo_DeclarationBlock_SetColorValue(aDecl.get(), eCSSProperty_color, aColor);

  // Now make sure we restyle any links that might need it.  This
  // shouldn't happen often, so just rebuilding everything is ok.
  if (Element* root = mDocument->GetRootElement()) {
    RestyleManager* rm = mDocument->GetPresContext()->RestyleManager();
    rm->PostRestyleEvent(root, RestyleHint::RestyleSubtree(), nsChangeHint(0));
  }
  return NS_OK;
}

nsresult AttributeStyles::SetLinkColor(nscolor aColor) {
  return ImplLinkColorSetter(mServoUnvisitedLinkDecl, aColor);
}

nsresult AttributeStyles::SetActiveLinkColor(nscolor aColor) {
  return ImplLinkColorSetter(mServoActiveLinkDecl, aColor);
}

nsresult AttributeStyles::SetVisitedLinkColor(nscolor aColor) {
  return ImplLinkColorSetter(mServoVisitedLinkDecl, aColor);
}

size_t AttributeStyles::DOMSizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);
  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mServoUnvisitedLinkDecl;
  // - mServoVisitedLinkDecl;
  // - mServoActiveLinkDecl;
  //
  // The following members are not measured:
  // - mDocument, because it's non-owning
  return n;
}

AttributeStyles::~AttributeStyles() {
  // We may go away before all of our cached style attributes do, so clean up
  // any that are left.
  for (auto iter = mCachedStyleAttrs.Iter(); !iter.Done(); iter.Next()) {
    MiscContainer*& value = iter.Data();

    // Ideally we'd just call MiscContainer::Evict, but we can't do that since
    // we're iterating the hashtable.
    if (value->mType == nsAttrValue::eCSSDeclaration) {
      DeclarationBlock* declaration = value->mValue.mCSSDeclaration;
      declaration->SetAttributeStyles(nullptr);
    } else {
      MOZ_ASSERT_UNREACHABLE("unexpected cached nsAttrValue type");
    }

    value->mValue.mCached = 0;
    iter.Remove();
  }
}

}  // namespace mozilla
