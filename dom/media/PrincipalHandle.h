/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PRINCIPALHANDLE_H_
#define DOM_MEDIA_PRINCIPALHANDLE_H_

#include "nsIPrincipal.h"
#include "nsProxyRelease.h"

namespace mozilla {
/**
 * The level of privacy of a principal as considered by RTCPeerConnection.
 */
enum class PrincipalPrivacy : uint8_t { Private, NonPrivate };

/**
 * We pass the principal through the MediaTrackGraph by wrapping it in a thread
 * safe nsMainThreadPtrHandle, since it cannot be used directly off the main
 * thread. We can compare two PrincipalHandles to each other on any thread, but
 * they can only be created and converted back to nsIPrincipal* on main thread.
 */
typedef nsMainThreadPtrHandle<nsIPrincipal> PrincipalHandle;

inline PrincipalHandle MakePrincipalHandle(nsIPrincipal* aPrincipal) {
  RefPtr<nsMainThreadPtrHolder<nsIPrincipal>> holder =
      new nsMainThreadPtrHolder<nsIPrincipal>(
          "MakePrincipalHandle::nsIPrincipal", aPrincipal);
  return PrincipalHandle(holder);
}

#define PRINCIPAL_HANDLE_NONE nullptr

inline nsIPrincipal* GetPrincipalFromHandle(
    const PrincipalHandle& aPrincipalHandle) {
  MOZ_ASSERT(NS_IsMainThread());
  return aPrincipalHandle.get();
}

inline bool PrincipalHandleMatches(const PrincipalHandle& aPrincipalHandle,
                                   nsIPrincipal* aOther) {
  if (!aOther) {
    return false;
  }

  nsIPrincipal* principal = GetPrincipalFromHandle(aPrincipalHandle);
  if (!principal) {
    return false;
  }

  bool result;
  if (NS_FAILED(principal->Equals(aOther, &result))) {
    NS_ERROR("Principal check failed");
    return false;
  }

  return result;
}
}  // namespace mozilla

#endif  // DOM_MEDIA_PRINCIPALHANDLE_H_
