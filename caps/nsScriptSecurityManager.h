/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScriptSecurityManager_h__
#define nsScriptSecurityManager_h__

#include "nsIScriptSecurityManager.h"

#include "mozilla/Maybe.h"
#include "nsIPrincipal.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsStringFwd.h"
#include "js/TypeDecls.h"

#include <stdint.h>

class nsIIOService;
class nsIStringBundle;

namespace mozilla {
class OriginAttributes;
class SystemPrincipal;
}  // namespace mozilla

namespace JS {
enum class RuntimeCode;
}  // namespace JS

/////////////////////////////
// nsScriptSecurityManager //
/////////////////////////////
#define NS_SCRIPTSECURITYMANAGER_CID                 \
  {                                                  \
    0x7ee2a4c0, 0x4b93, 0x17d3, {                    \
      0xba, 0x18, 0x00, 0x60, 0xb0, 0xf1, 0x99, 0xa2 \
    }                                                \
  }

class nsScriptSecurityManager final : public nsIScriptSecurityManager {
 public:
  static void Shutdown();

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_SCRIPTSECURITYMANAGER_CID)

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTSECURITYMANAGER

  static nsScriptSecurityManager* GetScriptSecurityManager();

  // Invoked exactly once, by XPConnect.
  static void InitStatics();

  void InitJSCallbacks(JSContext* aCx);

  // This has to be static because it is called after gScriptSecMan is cleared.
  static void ClearJSCallbacks(JSContext* aCx);

  static already_AddRefed<mozilla::SystemPrincipal>
  SystemPrincipalSingletonConstructor();

  /**
   * Utility method for comparing two URIs.  For security purposes, two URIs
   * are equivalent if their schemes, hosts, and ports (if any) match.  This
   * method returns true if aSubjectURI and aObjectURI have the same origin,
   * false otherwise.
   */
  static bool SecurityCompareURIs(nsIURI* aSourceURI, nsIURI* aTargetURI);
  static uint32_t SecurityHashURI(nsIURI* aURI);

  static nsresult ReportError(const char* aMessageTag, nsIURI* aSource,
                              nsIURI* aTarget, bool aFromPrivateWindow,
                              uint64_t aInnerWindowID = 0);
  static nsresult ReportError(const char* aMessageTag,
                              const nsACString& sourceSpec,
                              const nsACString& targetSpec,
                              bool aFromPrivateWindow,
                              uint64_t aInnerWindowID = 0);

  static uint32_t HashPrincipalByOrigin(nsIPrincipal* aPrincipal);

  static bool GetStrictFileOriginPolicy() { return sStrictFileOriginPolicy; }

  void DeactivateDomainPolicy();

 private:
  // GetScriptSecurityManager is the only call that can make one
  nsScriptSecurityManager();
  virtual ~nsScriptSecurityManager();

  // Decides, based on CSP, whether or not eval() and stuff can be executed.
  static bool ContentSecurityPolicyPermitsJSAction(JSContext* cx,
                                                   JS::RuntimeCode kind,
                                                   JS::Handle<JSString*> aCode);

  static bool JSPrincipalsSubsume(JSPrincipals* first, JSPrincipals* second);

  nsresult Init();

  nsresult InitPrefs();

  static void ScriptSecurityPrefChanged(const char* aPref, void* aSelf);
  void ScriptSecurityPrefChanged(const char* aPref = nullptr);

  inline void AddSitesToFileURIAllowlist(const nsCString& aSiteList);

  nsresult GetChannelResultPrincipal(nsIChannel* aChannel,
                                     nsIPrincipal** aPrincipal,
                                     bool aIgnoreSandboxing);

  nsresult CheckLoadURIFlags(nsIURI* aSourceURI, nsIURI* aTargetURI,
                             nsIURI* aSourceBaseURI, nsIURI* aTargetBaseURI,
                             uint32_t aFlags, bool aFromPrivateWindow,
                             uint64_t aInnerWindowID);

  // Returns the file URI allowlist, initializing it if it has not been
  // initialized.
  const nsTArray<nsCOMPtr<nsIURI>>& EnsureFileURIAllowlist();

  nsCOMPtr<nsIPrincipal> mSystemPrincipal;
  bool mPrefInitialized;
  bool mIsJavaScriptEnabled;

  // List of URIs whose domains and sub-domains are allowlisted to allow
  // access to file: URIs.  Lazily initialized; isNothing() when not yet
  // initialized.
  mozilla::Maybe<nsTArray<nsCOMPtr<nsIURI>>> mFileURIAllowlist;

  // This machinery controls new-style domain policies. The old-style
  // policy machinery will be removed soon.
  nsCOMPtr<nsIDomainPolicy> mDomainPolicy;

  static std::atomic<bool> sStrictFileOriginPolicy;

  static mozilla::StaticRefPtr<nsIIOService> sIOService;
  static nsIStringBundle* sStrBundle;
};

#endif  // nsScriptSecurityManager_h__
