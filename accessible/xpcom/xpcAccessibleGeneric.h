/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleGeneric_h_
#define mozilla_a11y_xpcAccessibleGeneric_h_

#include "xpcAccessible.h"
#include "xpcAccessibleHyperLink.h"
#include "xpcAccessibleSelectable.h"
#include "xpcAccessibleValue.h"

#include "LocalAccessible.h"
#include "mozilla/a11y/Accessible.h"
#include "mozilla/a11y/RemoteAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * XPCOM wrapper around Accessible class.
 */
class xpcAccessibleGeneric : public xpcAccessible,
                             public xpcAccessibleHyperLink,
                             public xpcAccessibleSelectable,
                             public xpcAccessibleValue {
 public:
  explicit xpcAccessibleGeneric(Accessible* aInternal)
      : mIntl(aInternal), mSupportedIfaces(0) {
    if (aInternal->IsSelect()) mSupportedIfaces |= eSelectable;
    if (aInternal->HasNumericValue()) mSupportedIfaces |= eValue;
    if (aInternal->IsLink()) mSupportedIfaces |= eHyperLink;
  }

  NS_DECL_ISUPPORTS

  // nsIAccessible
  LocalAccessible* ToInternalAccessible() final;
  Accessible* ToInternalGeneric() final { return mIntl; }

  // xpcAccessibleGeneric
  virtual void Shutdown();

 protected:
  virtual ~xpcAccessibleGeneric();

  Accessible* mIntl;

  enum {
    eSelectable = 1 << 0,
    eValue = 1 << 1,
    eHyperLink = 1 << 2,
    eText = 1 << 3
  };
  uint8_t mSupportedIfaces;

 private:
  friend class LocalAccessible;
  friend class xpcAccessible;
  friend class xpcAccessibleHyperLink;
  friend class xpcAccessibleSelectable;
  friend class xpcAccessibleValue;

  xpcAccessibleGeneric(const xpcAccessibleGeneric&) = delete;
  xpcAccessibleGeneric& operator=(const xpcAccessibleGeneric&) = delete;
};

inline LocalAccessible* xpcAccessible::Intl() {
  if (!static_cast<xpcAccessibleGeneric*>(this)->mIntl) {
    return nullptr;
  }
  return static_cast<xpcAccessibleGeneric*>(this)->mIntl->AsLocal();
}

inline Accessible* xpcAccessible::IntlGeneric() {
  return static_cast<xpcAccessibleGeneric*>(this)->mIntl;
}

inline Accessible* xpcAccessibleHyperLink::Intl() {
  return static_cast<xpcAccessibleGeneric*>(this)->mIntl;
}

inline Accessible* xpcAccessibleSelectable::Intl() {
  return static_cast<xpcAccessibleGeneric*>(this)->mIntl;
}

inline Accessible* xpcAccessibleValue::Intl() {
  return static_cast<xpcAccessibleGeneric*>(this)->mIntl;
}

}  // namespace a11y
}  // namespace mozilla

#endif
