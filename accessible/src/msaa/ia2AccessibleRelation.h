/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NS_ACCESSIBLE_RELATION_WRAP_H
#define _NS_ACCESSIBLE_RELATION_WRAP_H

#include "Accessible.h"

#include "nsTArray.h"

#include "AccessibleRelation.h"

namespace mozilla {
namespace a11y {

class ia2AccessibleRelation : public IAccessibleRelation
{
public:
  ia2AccessibleRelation(PRUint32 aType, Relation* aRel);
  virtual ~ia2AccessibleRelation() { }

  // IUnknown
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID aIID, void** aOutPtr);
  virtual ULONG STDMETHODCALLTYPE AddRef();
  virtual ULONG STDMETHODCALLTYPE Release();

  // IAccessibleRelation
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_relationType(
      /* [retval][out] */ BSTR *relationType);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_localizedRelationType(
      /* [retval][out] */ BSTR *localizedRelationType);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nTargets(
      /* [retval][out] */ long *nTargets);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_target(
      /* [in] */ long targetIndex,
      /* [retval][out] */ IUnknown **target);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_targets(
      /* [in] */ long maxTargets,
      /* [length_is][size_is][out] */ IUnknown **target,
      /* [retval][out] */ long *nTargets);

  inline bool HasTargets() const
    { return mTargets.Length(); }

private:
  ia2AccessibleRelation();
  ia2AccessibleRelation(const ia2AccessibleRelation&);
  ia2AccessibleRelation& operator = (const ia2AccessibleRelation&);

  PRUint32 mType;
  nsTArray<nsRefPtr<Accessible> > mTargets;
  ULONG mReferences;
};

} // namespace a11y
} // namespace mozilla

#endif

