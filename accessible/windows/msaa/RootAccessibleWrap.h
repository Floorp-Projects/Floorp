/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RootAccessibleWrap_h__
#define mozilla_a11y_RootAccessibleWrap_h__

#include "RootAccessible.h"

namespace mozilla {
namespace mscom {

struct IWeakReference;

} // namespace mscom

namespace a11y {

class RootAccessibleWrap : public RootAccessible
{
public:
  RootAccessibleWrap(nsIDocument* aDocument, nsIPresShell* aPresShell);
  virtual ~RootAccessibleWrap();

  // Accessible
  virtual void GetNativeInterface(void** aOutAccessible) override;

  // RootAccessible
  virtual void DocumentActivated(DocAccessible* aDocument);

private:
  RefPtr<mscom::IWeakReference> mInterceptor;
};

} // namespace a11y
} // namespace mozilla

#endif
