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
  a11y::role r = GetAccService()->MarkupRole(mContent);
  return r != roles::NOTHING ? r : roles::LIST;
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
  a11y::role r = GetAccService()->MarkupRole(mContent);
  return r != roles::NOTHING ? r : roles::LISTITEM;
}

uint64_t
HTMLLIAccessible::NativeState()
{
  return HyperTextAccessibleWrap::NativeState() | states::READONLY;
}

nsIntRect
HTMLLIAccessible::Bounds() const
{
  nsIntRect rect = AccessibleWrap::Bounds();
  if (rect.IsEmpty() || !mBullet || mBullet->IsInside())
    return rect;

  nsIntRect bulletRect = mBullet->Bounds();

  rect.width += rect.x - bulletRect.x;
  rect.x = bulletRect.x; // Move x coordinate of list item over to cover bullet as well
  return rect;
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

  RefPtr<AccReorderEvent> reorderEvent = new AccReorderEvent(this);
  AutoTreeMutation mut(this);

  DocAccessible* document = Document();
  if (aHasBullet) {
    mBullet = new HTMLListBulletAccessible(mContent, mDoc);
    document->BindToDocument(mBullet, nullptr);
    InsertChildAt(0, mBullet);

    RefPtr<AccShowEvent> event = new AccShowEvent(mBullet);
    mDoc->FireDelayedEvent(event);
    reorderEvent->AddSubMutationEvent(event);
  } else {
    RefPtr<AccHideEvent> event = new AccHideEvent(mBullet, mBullet->GetContent());
    mDoc->FireDelayedEvent(event);
    reorderEvent->AddSubMutationEvent(event);

    RemoveChild(mBullet);
    mBullet = nullptr;
  }

  mDoc->FireDelayedEvent(reorderEvent);
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
    blockFrame->GetSpokenBulletText(aName);
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
    blockFrame->GetSpokenBulletText(bulletText);

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
