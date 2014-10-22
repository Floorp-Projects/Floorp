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

#include "Accessible.h"

namespace mozilla {
namespace a11y {

/**
 * XPCOM wrapper around Accessible class.
 */
class xpcAccessibleGeneric : public xpcAccessible,
                             public xpcAccessibleHyperLink,
                             public xpcAccessibleSelectable,
                             public xpcAccessibleValue
{
public:
  xpcAccessibleGeneric(Accessible* aInternal) :
    mIntl(aInternal), mSupportedIfaces(0)
  {
    if (mIntl->IsSelect())
      mSupportedIfaces |= eSelectable;
    if (mIntl->HasNumericValue())
      mSupportedIfaces |= eValue;
    if (mIntl->IsLink())
      mSupportedIfaces |= eHyperLink;
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(xpcAccessibleGeneric, nsIAccessible)

  // nsIAccessible
  virtual Accessible* ToInternalAccessible() const MOZ_FINAL;

  // xpcAccessibleGeneric
  virtual void Shutdown();

protected:
  virtual ~xpcAccessibleGeneric() {}

  Accessible* mIntl;

  enum {
    eSelectable = 1 << 0,
    eValue = 1 << 1,
    eHyperLink = 1 << 2,
    eText = 1 << 3
  };
  uint8_t mSupportedIfaces;

private:
  friend class Accessible;
  friend class xpcAccessible;
  friend class xpcAccessibleHyperLink;
  friend class xpcAccessibleSelectable;
  friend class xpcAccessibleValue;

  xpcAccessibleGeneric(const xpcAccessibleGeneric&) MOZ_DELETE;
  xpcAccessibleGeneric& operator =(const xpcAccessibleGeneric&) MOZ_DELETE;
};

inline Accessible*
xpcAccessible::Intl()
{
  return static_cast<xpcAccessibleGeneric*>(this)->mIntl;
}

inline Accessible*
xpcAccessibleHyperLink::Intl()
{
  return static_cast<xpcAccessibleGeneric*>(this)->mIntl;
}

inline Accessible*
xpcAccessibleSelectable::Intl()
{
  return static_cast<xpcAccessibleGeneric*>(this)->mIntl;
}

inline Accessible*
xpcAccessibleValue::Intl()
{
  return static_cast<xpcAccessibleGeneric*>(this)->mIntl;
}

} // namespace a11y
} // namespace mozilla

#endif
