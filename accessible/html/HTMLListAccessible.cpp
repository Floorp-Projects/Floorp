/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLListAccessible.h"

#include "DocAccessible.h"
#include "EventTree.h"
#include "nsAccUtils.h"
#include "nsPersistentProperties.h"
#include "nsTextEquivUtils.h"
#include "Role.h"
#include "States.h"

#include "nsBulletFrame.h"
#include "nsLayoutUtils.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HTMLListAccessible
////////////////////////////////////////////////////////////////////////////////

role HTMLListAccessible::NativeRole() const {
  a11y::role r = GetAccService()->MarkupRole(mContent);
  return r != roles::NOTHING ? r : roles::LIST;
}

uint64_t HTMLListAccessible::NativeState() const {
  return HyperTextAccessibleWrap::NativeState() | states::READONLY;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLLIAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLLIAccessible::HTMLLIAccessible(nsIContent* aContent, DocAccessible* aDoc)
    : HyperTextAccessibleWrap(aContent, aDoc) {
  mType = eHTMLLiType;
}

role HTMLLIAccessible::NativeRole() const {
  a11y::role r = GetAccService()->MarkupRole(mContent);
  return r != roles::NOTHING ? r : roles::LISTITEM;
}

uint64_t HTMLLIAccessible::NativeState() const {
  return HyperTextAccessibleWrap::NativeState() | states::READONLY;
}

nsRect HTMLLIAccessible::BoundsInAppUnits() const {
  nsRect rect = AccessibleWrap::BoundsInAppUnits();

  LocalAccessible* bullet = Bullet();
  if (bullet && GetFrame()->StyleList()->mListStylePosition !=
                    NS_STYLE_LIST_STYLE_POSITION_INSIDE) {
    nsRect bulletRect = bullet->BoundsInAppUnits();
    return rect.Union(bulletRect);
  }

  return rect;
}

LocalAccessible* HTMLLIAccessible::Bullet() const {
  LocalAccessible* firstChild = LocalFirstChild();
  if (firstChild && firstChild->NativeRole() == roles::LISTITEM_MARKER) {
    return firstChild;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLListBulletAccessible
////////////////////////////////////////////////////////////////////////////////
HTMLListBulletAccessible::HTMLListBulletAccessible(nsIContent* aContent,
                                                   DocAccessible* aDoc)
    : LeafAccessible(aContent, aDoc) {
  mGenericTypes |= eText;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLListBulletAccessible: LocalAccessible

ENameValueFlag HTMLListBulletAccessible::Name(nsString& aName) const {
  aName.Truncate();

  // Native anonymous content, ARIA can't be used. Get list bullet text.
  if (nsBulletFrame* frame = do_QueryFrame(GetFrame())) {
    if (!frame->StyleList()->mListStyleImage.IsNone()) {
      // Bullet is an image, so use default bullet character.
      const char16_t kDiscCharacter = 0x2022;
      aName.Assign(kDiscCharacter);
      aName.Append(' ');
      return eNameOK;
    }
    frame->GetSpokenText(aName);
  } else {
    // If marker is not a bullet frame but instead has content
    nsTextEquivUtils::AppendFromDOMChildren(mContent, &aName);
    aName.CompressWhitespace();
  }

  return eNameOK;
}

role HTMLListBulletAccessible::NativeRole() const {
  return roles::LISTITEM_MARKER;
}

uint64_t HTMLListBulletAccessible::NativeState() const {
  return LeafAccessible::NativeState() | states::READONLY;
}

already_AddRefed<nsIPersistentProperties>
HTMLListBulletAccessible::NativeAttributes() {
  RefPtr<nsPersistentProperties> attributes = new nsPersistentProperties();
  return attributes.forget();
}

void HTMLListBulletAccessible::AppendTextTo(nsAString& aText,
                                            uint32_t aStartOffset,
                                            uint32_t aLength) {
  nsAutoString bulletText;
  Name(bulletText);
  aText.Append(Substring(bulletText, aStartOffset, aLength));
}
