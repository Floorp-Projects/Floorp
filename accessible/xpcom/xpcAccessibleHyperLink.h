/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleHyperLink_h_
#define mozilla_a11y_xpcAccessibleHyperLink_h_

#include "nsIAccessibleHyperLink.h"

class nsIAccessible;

namespace mozilla {
namespace a11y {

class Accessible;

/**
 * XPCOM nsIAccessibleHyperLink implementation, used by xpcAccessibleGeneric
 * class.
 */
class xpcAccessibleHyperLink : public nsIAccessibleHyperLink
{
public:
  NS_IMETHOD GetAnchorCount(int32_t* aAnchorCount) MOZ_FINAL;
  NS_IMETHOD GetStartIndex(int32_t* aStartIndex) MOZ_FINAL;
  NS_IMETHOD GetEndIndex(int32_t* aEndIndex) MOZ_FINAL;
  NS_IMETHOD GetURI(int32_t aIndex, nsIURI** aURI) MOZ_FINAL;
  NS_IMETHOD GetAnchor(int32_t aIndex, nsIAccessible** aAccessible) MOZ_FINAL;
  NS_IMETHOD GetValid(bool* aValid) MOZ_FINAL;

protected:
  xpcAccessibleHyperLink() { }
  virtual ~xpcAccessibleHyperLink() {}

private:
  xpcAccessibleHyperLink(const xpcAccessibleHyperLink&) MOZ_DELETE;
  xpcAccessibleHyperLink& operator =(const xpcAccessibleHyperLink&) MOZ_DELETE;

  Accessible* Intl();
};

} // namespace a11y
} // namespace mozilla

#endif
