/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_AccessibleWrap_h_
#define mozilla_a11y_AccessibleWrap_h_

#include "Accessible.h"
#include "mozilla/a11y/ProxyAccessible.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace a11y {

class AccessibleWrap : public Accessible
{
public:
  AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~AccessibleWrap();

  virtual void Shutdown() override;

  int32_t VirtualViewID() { return mID; }

  static const int32_t kNoID = -1;

protected:
  // IDs should be a positive 32bit integer.
  static int32_t AcquireID();
  static void ReleaseID(int32_t aID);

  int32_t mID;
};

static inline AccessibleWrap*
WrapperFor(const ProxyAccessible* aProxy)
{
  return reinterpret_cast<AccessibleWrap*>(aProxy->GetWrapper());
}

} // namespace a11y
} // namespace mozilla

#endif
