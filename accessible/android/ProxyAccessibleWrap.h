/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. *
 */

#ifndef MOZILLA_A11Y_ProxyAccessibleWrap_h
#define MOZILLA_A11Y_ProxyAccessibleWrap_h

#include "AccessibleWrap.h"
#include "DocAccessibleParent.h"

namespace mozilla {
namespace a11y {

class ProxyAccessibleWrap : public AccessibleWrap
{
public:
  explicit ProxyAccessibleWrap(ProxyAccessible* aProxy);

  virtual void Shutdown() override;
};

class DocProxyAccessibleWrap : public ProxyAccessibleWrap
{
public:
  explicit DocProxyAccessibleWrap(DocAccessibleParent* aProxy)
    : ProxyAccessibleWrap(aProxy)
  {
    mGenericTypes |= eDocument;

    if (auto parent = ParentDocument()) {
      mID = AcquireID();
      parent->AddID(mID, this);
    } else {
      // top level
      mID = kNoID;
    }
  }

  virtual void Shutdown() override
  {
    if (mID) {
      auto parent = ParentDocument();
      if (parent) {
        MOZ_ASSERT(mID != kNoID, "A non root accessible always has a parent");
        parent->RemoveID(mID);
        ReleaseID(mID);
      }
    }
    mID = 0;
    mBits.proxy = nullptr;
    mStateFlags |= eIsDefunct;
  }

  DocProxyAccessibleWrap* ParentDocument()
  {
    DocAccessibleParent* proxy = static_cast<DocAccessibleParent*>(Proxy());
    MOZ_ASSERT(proxy);
    if (DocAccessibleParent* parent = proxy->ParentDoc()) {
      return reinterpret_cast<DocProxyAccessibleWrap*>(parent->GetWrapper());
    }

    return nullptr;
  }

  DocProxyAccessibleWrap* GetChildDocumentAt(uint32_t aIndex)
  {
    auto doc = Proxy()->AsDoc();
    if (doc && doc->ChildDocCount() > aIndex) {
      return reinterpret_cast<DocProxyAccessibleWrap*>(
        doc->ChildDocAt(aIndex)->GetWrapper());
    }

    return nullptr;
  }

  void AddID(uint32_t aID, AccessibleWrap* aAcc)
  {
    mIDToAccessibleMap.Put(aID, aAcc);
  }
  void RemoveID(uint32_t aID) { mIDToAccessibleMap.Remove(aID); }
  AccessibleWrap* GetAccessibleByID(uint32_t aID) const
  {
    return mIDToAccessibleMap.Get(aID);
  }

private:
  /*
   * This provides a mapping from 32 bit id to accessible objects.
   */
  nsDataHashtable<nsUint32HashKey, AccessibleWrap*> mIDToAccessibleMap;
};
}
}

#endif
