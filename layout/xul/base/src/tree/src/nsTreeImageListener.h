/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTreeImageListener_h__
#define nsTreeImageListener_h__

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsITreeColumns.h"
#include "nsTreeBodyFrame.h"
#include "mozilla/Attributes.h"

// This class handles image load observation.
class nsTreeImageListener MOZ_FINAL : public imgINotificationObserver
{
public:
  nsTreeImageListener(nsTreeBodyFrame *aTreeFrame);
  ~nsTreeImageListener();

  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  NS_IMETHOD ClearFrame();

  friend class nsTreeBodyFrame;

protected:
  void UnsuppressInvalidation() { mInvalidationSuppressed = false; }
  void Invalidate();
  void AddCell(int32_t aIndex, nsITreeColumn* aCol);

private:
  nsTreeBodyFrame* mTreeFrame;

  // A guard that prevents us from recursive painting.
  bool mInvalidationSuppressed;

  class InvalidationArea {
    public:
      InvalidationArea(nsITreeColumn* aCol);
      ~InvalidationArea() { delete mNext; }

      friend class nsTreeImageListener;

    protected:
      void AddRow(int32_t aIndex);
      nsITreeColumn* GetCol() { return mCol.get(); }
      int32_t GetMin() { return mMin; }
      int32_t GetMax() { return mMax; }
      InvalidationArea* GetNext() { return mNext; }
      void SetNext(InvalidationArea* aNext) { mNext = aNext; }

    private:
      nsCOMPtr<nsITreeColumn> mCol;
      int32_t                 mMin;
      int32_t                 mMax;
      InvalidationArea*       mNext;
  };

  InvalidationArea* mInvalidationArea;
};

#endif // nsTreeImageListener_h__
