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

  virtual void Shutdown() override;

  virtual nsresult HandleAccEvent(AccEvent* aEvent) override;

  DocAccessibleWrap* GetTopLevelContentDoc(AccessibleWrap* aAccessible);

  bool IsTopLevelContentDoc();

  void CacheFocusPath(AccessibleWrap* aAccessible);

  void CacheViewport(bool aCachePivotBoundaries);

  enum {
    eBatch_Viewport = 0,
    eBatch_FocusPath = 1,
    eBatch_BoundsUpdate = 2,
  };

 protected:
  virtual void DoInitialUpdate() override;

 private:
  void UpdateFocusPathBounds();

  static void CacheViewportCallback(nsITimer* aTimer, void* aDocAccParam);

  nsCOMPtr<nsITimer> mCacheRefreshTimer;

  bool mCachePivotBoundaries;

  AccessibleHashtable mFocusPath;
};

}  // namespace a11y
}  // namespace mozilla

#endif
