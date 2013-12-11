/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLListAccessible.h"

#include "DocAccessible.h"
#include "nsAccUtils.h"
#include "Role.h"
#include "States.h"

#include "nsBlockFrame.h"
#include "nsBulletFrame.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HTMLListAccessible
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(HTMLListAccessible, HyperTextAccessible)

role
HTMLListAccessible::NativeRole()
{
  if (mContent->Tag() == nsGkAtoms::dl)
    return roles::DEFINITION_LIST;

  return roles::LIST;
}

uint64_t
HTMLListAccessible::NativeState()
{
  return HyperTextAccessibleWrap::NativeState() | states::READONLY;
}


////////////////////////////////////////////////////////////////////////////////
// HTMLLIAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLLIAccessible::
  HTMLLIAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc), mBullet(nullptr)
{
  mType = eHTMLLiType;

  nsBlockFrame* blockFrame = do_QueryFrame(GetFrame());
  if (blockFrame && blockFrame->HasBullet()) {
    mBullet = new HTMLListBulletAccessible(mContent, mDoc);
    Document()->BindToDocument(mBullet, nullptr);
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(HTMLLIAccessible, HyperTextAccessible)

void
HTMLLIAccessible::Shutdown()
{
  mBullet = nullptr;

  HyperTextAccessibleWrap::Shutdown();
}

role
HTMLLIAccessible::NativeRole()
{
  if (mContent->Tag() == nsGkAtoms::dt)
    return roles::TERM;

  return roles::LISTITEM;
}

uint64_t
HTMLLIAccessible::NativeState()
{
  return HyperTextAccessibleWrap::NativeState() | states::READONLY;
}

NS_IMETHODIMP
HTMLLIAccessible::GetBounds(int32_t* aX, int32_t* aY,
                            int32_t* aWidth, int32_t* aHeight)
{
  nsresult rv = AccessibleWrap::GetBounds(aX, aY, aWidth, aHeight);
  if (NS_FAILED(rv) || !mBullet || mBullet->IsInside())
    return rv;

  int32_t bulletX = 0, bulletY = 0, bulletWidth = 0, bulletHeight = 0;
  rv = mBullet->GetBounds(&bulletX, &bulletY, &bulletWidth, &bulletHeight);
  NS_ENSURE_SUCCESS(rv, rv);

  *aWidth += *aX - bulletX;
  *aX = bulletX; // Move x coordinate of list item over to cover bullet as well
  return NS_OK;
}

int32_t
HTMLLIAccessible::FindOffset(int32_t aOffset, nsDirection aDirection,
                             nsSelectionAmount aAmount,
                             EWordMovementType aWordMovementType)
{
  Accessible* child = GetChildAtOffset(aOffset);
  if (!child)
    return -1;

  if (child != mBullet) {
    if (aDirection == eDirPrevious &&
        (aAmount == eSelectBeginLine || aAmount == eSelectLine))
      return 0;

    return HyperTextAccessible::FindOffset(aOffset, aDirection,
                                           aAmount, aWordMovementType);
  }

  if (aDirection == eDirPrevious)
    return 0;

  if (aAmount == eSelectEndLine || aAmount == eSelectLine)
    return CharacterCount();

  return nsAccUtils::TextLength(child);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLLIAccessible: public

void
HTMLLIAccessible::UpdateBullet(bool aHasBullet)
{
  if (aHasBullet == !!mBullet) {
    NS_NOTREACHED("Bullet and accessible are in sync already!");
    return;
  }

  DocAccessible* document = Document();
  if (aHasBullet) {
    mBullet = new HTMLListBulletAccessible(mContent, mDoc);
    document->BindToDocument(mBullet, nullptr);
    InsertChildAt(0, mBullet);
  } else {
    RemoveChild(mBullet);
    document->UnbindFromDocument(mBullet);
    mBullet = nullptr;
  }

  // XXXtodo: fire show/hide and reorder events. That's hard to make it
  // right now because coalescence happens by DOM node.
}

////////////////////////////////////////////////////////////////////////////////
// HTMLLIAccessible: Accessible protected

void
HTMLLIAccessible::CacheChildren()
{
  if (mBullet)
    AppendChild(mBullet);

  // Cache children from subtree.
  AccessibleWrap::CacheChildren();
}

////////////////////////////////////////////////////////////////////////////////
// HTMLListBulletAccessible
////////////////////////////////////////////////////////////////////////////////
HTMLListBulletAccessible::
  HTMLListBulletAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  LeafAccessible(aContent, aDoc)
{
  mStateFlags |= eSharedNode;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLListBulletAccessible: Accessible

nsIFrame*
HTMLListBulletAccessible::GetFrame() const
{
  nsBlockFrame* blockFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  return blockFrame ? blockFrame->GetBullet() : nullptr;
}

ENameValueFlag
HTMLListBulletAccessible::Name(nsString &aName)
{
  aName.Truncate();

  // Native anonymous content, ARIA can't be used. Get list bullet text.
  nsBlockFrame* blockFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (blockFrame) {
    blockFrame->GetBulletText(aName);

    // Append space otherwise bullets are jammed up against list text.
    aName.Append(' ');
  }

  return eNameOK;
}

role
HTMLListBulletAccessible::NativeRole()
{
  return roles::STATICTEXT;
}

uint64_t
HTMLListBulletAccessible::NativeState()
{
  return LeafAccessible::NativeState() | states::READONLY;
}

void
HTMLListBulletAccessible::AppendTextTo(nsAString& aText, uint32_t aStartOffset,
                                       uint32_t aLength)
{
  nsAutoString bulletText;
  nsBlockFrame* blockFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (blockFrame)
    blockFrame->GetBulletText(bulletText);

  aText.Append(Substring(bulletText, aStartOffset, aLength));
}

////////////////////////////////////////////////////////////////////////////////
// HTMLListBulletAccessible: public

bool
HTMLListBulletAccessible::IsInside() const
{
  nsBlockFrame* blockFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  return blockFrame ? blockFrame->HasInsideBullet() : false;
}
