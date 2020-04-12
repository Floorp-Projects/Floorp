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

class nsIContentSecurityPolicy;
class nsILoadInfo;
class nsINode;
class nsIPrincipal;
class nsIRedirectHistoryEntry;

namespace IPC {

namespace detail {
template <class ParamType>
struct OriginAttributesParamTraits {
  typedef ParamType paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    nsAutoCString suffix;
    aParam.CreateSuffix(suffix);
    WriteParam(aMsg, suffix);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    nsAutoCString suffix;
    return ReadParam(aMsg, aIter, &suffix) &&
           aResult->PopulateFromSuffix(suffix);
  }
};
}  // namespace detail

template <>
struct ParamTraits<mozilla::OriginAttributes>
    : public detail::OriginAttributesParamTraits<mozilla::OriginAttributes> {};

}  // namespace IPC

namespace mozilla {
namespace net {
class ChildLoadInfoForwarderArgs;
class LoadInfoArgs;
class LoadInfo;
class ParentLoadInfoForwarderArgs;
class RedirectHistoryEntryInfo;
}  // namespace net

namespace ipc {

class ContentSecurityPolicy;
class CSPInfo;
class PrincipalInfo;

/**
 * Convert a PrincipalInfo to an nsIPrincipal.
 *
 * MUST be called on the main thread only.
 */
already_AddRefed<nsIPrincipal> PrincipalInfoToPrincipal(
    const PrincipalInfo& aPrincipalInfo, nsresult* aOptionalResult = nullptr);

/**
 * Convert an nsIPrincipal to a PrincipalInfo.
 *
 * MUST be called on the main thread only.
 */
nsresult PrincipalToPrincipalInfo(nsIPrincipal* aPrincipal,
                                  PrincipalInfo* aPrincipalInfo,
                                  bool aSkipBaseDomain = false);

/**
 * Convert a CSPInfo to an nsIContentSecurityPolicy.
 *
 * MUST be called on the main thread only.
 *
 * If possible, provide a requesting doc, so policy violation events can
 * be dispatched correctly. If aRequestingDoc is null, then the CSPInfo holds
 * the necessary fallback information, like a serialized requestPrincipal,
 * to generate a valid nsIContentSecurityPolicy.
 */
already_AddRefed<nsIContentSecurityPolicy> CSPInfoToCSP(
    const CSPInfo& aCSPInfo, mozilla::dom::Document* aRequestingDoc,
    nsresult* aOptionalResult = nullptr);

/**
 * Convert an nsIContentSecurityPolicy to a CSPInfo.
 *
 * MUST be called on the main thread only.
 */
nsresult CSPToCSPInfo(nsIContentSecurityPolicy* aCSP, CSPInfo* aCSPInfo);

/**
 * Return true if this PrincipalInfo is a content principal and it has
 * a privateBrowsing id in its OriginAttributes
 */
bool IsPrincipalInfoPrivate(const PrincipalInfo& aPrincipalInfo);

/**
 * Convert an RedirectHistoryEntryInfo to a nsIRedirectHistoryEntry.
 */

already_AddRefed<nsIRedirectHistoryEntry> RHEntryInfoToRHEntry(
    const mozilla::net::RedirectHistoryEntryInfo& aRHEntryInfo);

/**
 * Convert an nsIRedirectHistoryEntry to a RedirectHistoryEntryInfo.
 */

nsresult RHEntryToRHEntryInfo(
    nsIRedirectHistoryEntry* aRHEntry,
    mozilla::net::RedirectHistoryEntryInfo* aRHEntryInfo);

/**
 * Convert a LoadInfo to LoadInfoArgs struct.
 */
nsresult LoadInfoToLoadInfoArgs(
    nsILoadInfo* aLoadInfo,
    Maybe<mozilla::net::LoadInfoArgs>* outOptionalLoadInfoArgs);

/**
 * Convert LoadInfoArgs to a LoadInfo.
 */
nsresult LoadInfoArgsToLoadInfo(
    const Maybe<mozilla::net::LoadInfoArgs>& aOptionalLoadInfoArgs,
    nsILoadInfo** outLoadInfo);
nsresult LoadInfoArgsToLoadInfo(
    const Maybe<mozilla::net::LoadInfoArgs>& aOptionalLoadInfoArgs,
    nsINode* aCspToInheritLoadingContext, nsILoadInfo** outLoadInfo);
nsresult LoadInfoArgsToLoadInfo(
    const Maybe<net::LoadInfoArgs>& aOptionalLoadInfoArgs,
    mozilla::net::LoadInfo** outLoadInfo);
nsresult LoadInfoArgsToLoadInfo(
    const Maybe<net::LoadInfoArgs>& aOptionalLoadInfoArgs,
    nsINode* aCspToInheritLoadingContext, mozilla::net::LoadInfo** outLoadInfo);

/**
 * Fills ParentLoadInfoForwarderArgs with properties we want to carry to child
 * processes.
 */
void LoadInfoToParentLoadInfoForwarder(
    nsILoadInfo* aLoadInfo,
    mozilla::net::ParentLoadInfoForwarderArgs* aForwarderArgsOut);

/**
 * Merges (replaces) properties of an existing LoadInfo on a child process
 * with properties carried down through ParentLoadInfoForwarderArgs.
 */
nsresult MergeParentLoadInfoForwarder(
    mozilla::net::ParentLoadInfoForwarderArgs const& aForwarderArgs,
    nsILoadInfo* aLoadInfo);

/**
 * Fills ChildLoadInfoForwarderArgs with properties we want to carry to the
 * parent process after the initial channel creation.
 */
void LoadInfoToChildLoadInfoForwarder(
    nsILoadInfo* aLoadInfo,
    mozilla::net::ChildLoadInfoForwarderArgs* aForwarderArgsOut);

/**
 * Merges (replaces) properties of an existing LoadInfo on the parent process
 * with properties contained in a ChildLoadInfoForwarderArgs.
 */
nsresult MergeChildLoadInfoForwarder(
    const mozilla::net::ChildLoadInfoForwarderArgs& aForwardArgs,
    nsILoadInfo* aLoadInfo);

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_backgroundutils_h__
