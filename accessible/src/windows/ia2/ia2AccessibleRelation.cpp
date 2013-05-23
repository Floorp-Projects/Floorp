/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleRelation.h"

#include "Relation.h"
#include "IUnknownImpl.h"
#include "nsIAccessibleRelation.h"
#include "nsID.h"

#include "AccessibleRelation_i.c"

using namespace mozilla::a11y;

ia2AccessibleRelation::ia2AccessibleRelation(uint32_t aType, Relation* aRel) :
  mType(aType), mReferences(0)
{
  Accessible* target = nullptr;
  while ((target = aRel->Next()))
    mTargets.AppendElement(target);
}

// IUnknown

STDMETHODIMP
ia2AccessibleRelation::QueryInterface(REFIID iid, void** ppv)
{
  if (!ppv)
    return E_INVALIDARG;

  *ppv = nullptr;

  if (IID_IAccessibleRelation == iid || IID_IUnknown == iid) {
    *ppv = static_cast<IAccessibleRelation*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE
ia2AccessibleRelation::AddRef()
{
  return mReferences++;
}

ULONG STDMETHODCALLTYPE 
ia2AccessibleRelation::Release()
{
  mReferences--;
  ULONG references = mReferences;
  if (!mReferences)
    delete this;

  return references;
}

// IAccessibleRelation

STDMETHODIMP
ia2AccessibleRelation::get_relationType(BSTR *aRelationType)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRelationType)
    return E_INVALIDARG;

  *aRelationType = nullptr;

  switch (mType) {
    case nsIAccessibleRelation::RELATION_CONTROLLED_BY:
      *aRelationType = ::SysAllocString(IA2_RELATION_CONTROLLED_BY);
      break;
    case nsIAccessibleRelation::RELATION_CONTROLLER_FOR:
      *aRelationType = ::SysAllocString(IA2_RELATION_CONTROLLER_FOR);
      break;
    case nsIAccessibleRelation::RELATION_DESCRIBED_BY:
      *aRelationType = ::SysAllocString(IA2_RELATION_DESCRIBED_BY);
      break;
    case nsIAccessibleRelation::RELATION_DESCRIPTION_FOR:
      *aRelationType = ::SysAllocString(IA2_RELATION_DESCRIPTION_FOR);
      break;
    case nsIAccessibleRelation::RELATION_EMBEDDED_BY:
      *aRelationType = ::SysAllocString(IA2_RELATION_EMBEDDED_BY);
      break;
    case nsIAccessibleRelation::RELATION_EMBEDS:
      *aRelationType = ::SysAllocString(IA2_RELATION_EMBEDS);
      break;
    case nsIAccessibleRelation::RELATION_FLOWS_FROM:
      *aRelationType = ::SysAllocString(IA2_RELATION_FLOWS_FROM);
      break;
    case nsIAccessibleRelation::RELATION_FLOWS_TO:
      *aRelationType = ::SysAllocString(IA2_RELATION_FLOWS_TO);
      break;
    case nsIAccessibleRelation::RELATION_LABEL_FOR:
      *aRelationType = ::SysAllocString(IA2_RELATION_LABEL_FOR);
      break;
    case nsIAccessibleRelation::RELATION_LABELLED_BY:
      *aRelationType = ::SysAllocString(IA2_RELATION_LABELED_BY);
      break;
    case nsIAccessibleRelation::RELATION_MEMBER_OF:
      *aRelationType = ::SysAllocString(IA2_RELATION_MEMBER_OF);
      break;
    case nsIAccessibleRelation::RELATION_NODE_CHILD_OF:
      *aRelationType = ::SysAllocString(IA2_RELATION_NODE_CHILD_OF);
      break;
    case nsIAccessibleRelation::RELATION_NODE_PARENT_OF:
      *aRelationType = ::SysAllocString(IA2_RELATION_NODE_PARENT_OF);
      break;
    case nsIAccessibleRelation::RELATION_PARENT_WINDOW_OF:
      *aRelationType = ::SysAllocString(IA2_RELATION_PARENT_WINDOW_OF);
      break;
    case nsIAccessibleRelation::RELATION_POPUP_FOR:
      *aRelationType = ::SysAllocString(IA2_RELATION_POPUP_FOR);
      break;
    case nsIAccessibleRelation::RELATION_SUBWINDOW_OF:
      *aRelationType = ::SysAllocString(IA2_RELATION_SUBWINDOW_OF);
      break;
    default:
      return E_FAIL;
  }

  return *aRelationType ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleRelation::get_localizedRelationType(BSTR *aLocalizedRelationType)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aLocalizedRelationType)
    return E_INVALIDARG;

  *aLocalizedRelationType = nullptr;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleRelation::get_nTargets(long *aNTargets)
{
  A11Y_TRYBLOCK_BEGIN

 if (!aNTargets)
   return E_INVALIDARG;

 *aNTargets = mTargets.Length();
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleRelation::get_target(long aTargetIndex, IUnknown **aTarget)
{
  A11Y_TRYBLOCK_BEGIN

  if (aTargetIndex < 0 || (uint32_t)aTargetIndex >= mTargets.Length() || !aTarget)
    return E_INVALIDARG;

  AccessibleWrap* target =
    static_cast<AccessibleWrap*>(mTargets[aTargetIndex].get());
  *aTarget = static_cast<IAccessible*>(target);
  (*aTarget)->AddRef();

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleRelation::get_targets(long aMaxTargets, IUnknown **aTargets,
                                   long *aNTargets)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNTargets || !aTargets)
    return E_INVALIDARG;

  *aNTargets = 0;
  long maxTargets = mTargets.Length();
  if (maxTargets > aMaxTargets)
    maxTargets = aMaxTargets;

  for (long idx = 0; idx < maxTargets; idx++)
    get_target(idx, aTargets + idx);

  *aNTargets = maxTargets;
  return S_OK;

  A11Y_TRYBLOCK_END
}

