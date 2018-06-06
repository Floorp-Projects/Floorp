/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTreeImageListener_h__
#define nsTreeImageListener_h__

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsTreeBodyFrame.h"
#include "mozilla/Attributes.h"

class nsTreeColumn;

// This class handles image load observation.
class nsTreeImageListener final : public imgINotificationObserver
{
public:
  explicit nsTreeImageListener(nsTreeBodyFrame *aTreeFrame);

  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  NS_IMETHOD ClearFrame();

  friend class nsTreeBodyFrame;

protected:
  ~nsTreeImageListener();

  void UnsuppressInvalidation() { mInvalidationSuppressed = false; }
  void Invalidate();
  void AddCell(int32_t aIndex, nsTreeColumn* aCol);

private:
  nsTreeBodyFrame* mTreeFrame;

  // A guard that prevents us from recursive painting.
  bool mInvalidationSuppressed;

  class InvalidationArea {
    public:
      explicit InvalidationArea(nsTreeColumn* aCol);
      ~InvalidationArea() { delete mNext; }

      friend class nsTreeImageListener;

    protected:
      void AddRow(int32_t aIndex);
      nsTreeColumn* GetCol() { return mCol.get(); }
      int32_t GetMin() { return mMin; }
      int32_t GetMax() { return mMax; }
      InvalidationArea* GetNext() { return mNext; }
      void SetNext(InvalidationArea* aNext) { mNext = aNext; }

    private:
      RefPtr<nsTreeColumn> mCol;
      int32_t                 mMin;
      int32_t                 mMax;
      InvalidationArea*       mNext;
  };

  InvalidationArea* mInvalidationArea;
};

#endif // nsTreeImageListener_h__
