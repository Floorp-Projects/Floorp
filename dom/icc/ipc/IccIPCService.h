/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_icc_IccIPCService_h
#define mozilla_dom_icc_IccIPCService_h

#include "nsCOMPtr.h"
#include "nsIIccService.h"

namespace mozilla {
namespace dom {
namespace icc {

class IccChild;

class IccIPCService final : public nsIIccService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIICCSERVICE

  IccIPCService();

private:
  ~IccIPCService();

  nsTArray<RefPtr<IccChild>> mIccs;
};

} // namespace icc
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_icc_IccIPCService_h