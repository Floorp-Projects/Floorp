/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_backgroundutils_h__
#define mozilla_ipc_backgroundutils_h__

#include "ipc/IPCMessageUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/BasePrincipal.h"
#include "nsCOMPtr.h"
#include "nscore.h"

class nsILoadInfo;
class nsIPrincipal;

namespace IPC {

namespace detail {
template<class ParamType>
struct OriginAttributesParamTraits
{
  typedef ParamType paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    nsAutoCString suffix;
    aParam.CreateSuffix(suffix);
    WriteParam(aMsg, suffix);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    nsAutoCString suffix;
    return ReadParam(aMsg, aIter, &suffix) &&
           aResult->PopulateFromSuffix(suffix);
  }
};
} // namespace detail

template<>
struct ParamTraits<mozilla::OriginAttributes>
  : public detail::OriginAttributesParamTraits<mozilla::OriginAttributes> {};

} // namespace IPC

namespace mozilla {
namespace net {
class OptionalLoadInfoArgs;
} // namespace net

using namespace mozilla::net;

namespace ipc {

class PrincipalInfo;

/**
 * Convert a PrincipalInfo to an nsIPrincipal.
 *
 * MUST be called on the main thread only.
 */
already_AddRefed<nsIPrincipal>
PrincipalInfoToPrincipal(const PrincipalInfo& aPrincipalInfo,
                         nsresult* aOptionalResult = nullptr);

/**
 * Convert an nsIPrincipal to a PrincipalInfo.
 *
 * MUST be called on the main thread only.
 */
nsresult
PrincipalToPrincipalInfo(nsIPrincipal* aPrincipal,
                         PrincipalInfo* aPrincipalInfo);

/**
 * Convert a LoadInfo to LoadInfoArgs struct.
 */
nsresult
LoadInfoToLoadInfoArgs(nsILoadInfo *aLoadInfo,
                       OptionalLoadInfoArgs* outOptionalLoadInfoArgs);

/**
 * Convert LoadInfoArgs to a LoadInfo.
 */
nsresult
LoadInfoArgsToLoadInfo(const OptionalLoadInfoArgs& aOptionalLoadInfoArgs,
                       nsILoadInfo** outLoadInfo);

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_backgroundutils_h__
