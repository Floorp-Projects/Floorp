/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleWrap_h__
#define mozilla_a11y_DocAccessibleWrap_h__

#include "DocAccessible.h"
#include "nsITimer.h"

namespace mozilla {

class PresShell;

namespace a11y {

class DocAccessibleWrap : public DocAccessible {
 public:
  DocAccessibleWrap(Document* aDocument, PresShell* aPresShell);
  virtual ~DocAccessibleWrap();

  virtual nsresult HandleAccEvent(AccEvent* aEvent) override;

  /**
   * Manage the mapping from id to Accessible.
   */
  void AddID(uint32_t aID, AccessibleWrap* aAcc) {
    mIDToAccessibleMap.Put(aID, aAcc);
  }
  void RemoveID(uint32_t aID) { mIDToAccessibleMap.Remove(aID); }
  AccessibleWrap* GetAccessibleByID(int32_t aID) const;

  DocAccessibleWrap* GetTopLevelContentDoc(AccessibleWrap* aAccessible);

  void CacheFocusPath(AccessibleWrap* aAccessible);

  void CacheViewport();

  enum {
    eBatch_Viewport = 0,
    eBatch_FocusPath = 1,
    eBatch_BoundsUpdate = 2,
  };

 protected:
  /*
   * This provides a mapping from 32 bit id to accessible objects.
   */
  nsDataHashtable<nsUint32HashKey, AccessibleWrap*> mIDToAccessibleMap;

  virtual void DoInitialUpdate() override;

 private:
  void UpdateFocusPathBounds();

  static void CacheViewportCallback(nsITimer* aTimer, void* aDocAccParam);

  nsCOMPtr<nsITimer> mCacheRefreshTimer;

  AccessibleHashtable mFocusPath;
};

}  // namespace a11y
}  // namespace mozilla

#endif
