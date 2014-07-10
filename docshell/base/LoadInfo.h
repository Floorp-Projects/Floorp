/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LoadInfo_h
#define mozilla_LoadInfo_h

#include "nsIPrincipal.h"
#include "nsILoadInfo.h"

namespace mozilla {

/**
 * Class that provides an nsILoadInfo implementation.
 */
class LoadInfo MOZ_FINAL : public nsILoadInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOADINFO

  enum InheritType
  {
    eInheritPrincipal,
    eDontInheritPrincipal
  };

  enum SandboxType
  {
    eSandboxed,
    eNotSandboxed
  };

  // aPrincipal MUST NOT BE NULL.  If aSandboxed is eSandboxed, the
  // value passed for aInheritPrincipal will be ignored and
  // eDontInheritPrincipal will be used instead.
  LoadInfo(nsIPrincipal* aPrincipal,
           InheritType aInheritPrincipal,
           SandboxType aSandboxed);

private:
  ~LoadInfo();

  nsCOMPtr<nsIPrincipal> mPrincipal;
  bool mInheritPrincipal;
  bool mSandboxed;
};

} // namespace mozilla

#endif // mozilla_LoadInfo_h

