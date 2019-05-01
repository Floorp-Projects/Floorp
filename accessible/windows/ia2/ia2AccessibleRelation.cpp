/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleRelation.h"

#include "Relation.h"
#include "nsID.h"

#include "AccessibleRelation_i.c"

using namespace mozilla::a11y;

ia2AccessibleRelation::ia2AccessibleRelation(RelationType aType, Relation* aRel)
    : mType(aType) {
  Accessible* target = nullptr;
  while ((target = aRel->Next())) mTargets.AppendElement(target);
}

// IUnknown

IMPL_IUNKNOWN_QUERY_HEAD(ia2AccessibleRelation)
IMPL_IUNKNOWN_QUERY_IFACE(IAccessibleRelation)
IMPL_IUNKNOWN_QUERY_IFACE(IUnknown)
IMPL_IUNKNOWN_QUERY_TAIL

// IAccessibleRelation

STDMETHODIMP
ia2AccessibleRelation::get_relationType(BSTR* aRelationType) {
  if (!aRelationType) return E_INVALIDARG;

  *aRelationType = nullptr;

#define RELATIONTYPE(geckoType, geckoTypeName, atkType, msaaType, ia2Type) \
  case RelationType::geckoType:                                            \
    *aRelationType = ::SysAllocString(ia2Type);                            \
    break;

  switch (mType) {
#include "RelationTypeMap.h"
  }

  return *aRelationType ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
ia2AccessibleRelation::get_localizedRelationType(BSTR* aLocalizedRelationType) {
  if (!aLocalizedRelationType) return E_INVALIDARG;

  *aLocalizedRelationType = nullptr;
  return E_NOTIMPL;
}

STDMETHODIMP
ia2AccessibleRelation::get_nTargets(long* aNTargets) {
  if (!aNTargets) return E_INVALIDARG;

  *aNTargets = mTargets.Length();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleRelation::get_target(long aTargetIndex, IUnknown** aTarget) {
  if (aTargetIndex < 0 || (uint32_t)aTargetIndex >= mTargets.Length() ||
      !aTarget)
    return E_INVALIDARG;

  AccessibleWrap* target =
      static_cast<AccessibleWrap*>(mTargets[aTargetIndex].get());
  *aTarget = static_cast<IAccessible*>(target);
  (*aTarget)->AddRef();

  return S_OK;
}

STDMETHODIMP
ia2AccessibleRelation::get_targets(long aMaxTargets, IUnknown** aTargets,
                                   long* aNTargets) {
  if (!aNTargets || !aTargets) return E_INVALIDARG;

  *aNTargets = 0;
  long maxTargets = mTargets.Length();
  if (maxTargets > aMaxTargets) maxTargets = aMaxTargets;

  for (long idx = 0; idx < maxTargets; idx++) get_target(idx, aTargets + idx);

  *aNTargets = maxTargets;
  return S_OK;
}
