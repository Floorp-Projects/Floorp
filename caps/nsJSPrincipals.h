/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* describes principals by their orginating uris*/

#ifndef nsJSPrincipals_h__
#define nsJSPrincipals_h__
#include "jsapi.h"
#include "nsIPrincipal.h"

namespace mozilla {
namespace ipc {
class PrincipalInfo;
}  // namespace ipc
}  // namespace mozilla

class nsJSPrincipals : public nsIPrincipal, public JSPrincipals {
 public:
  /* SpiderMonkey security callbacks. */
  static bool Subsume(JSPrincipals* jsprin, JSPrincipals* other);
  static void Destroy(JSPrincipals* jsprin);

  /* JSReadPrincipalsOp for nsJSPrincipals */
  static bool ReadPrincipals(JSContext* aCx, JSStructuredCloneReader* aReader,
                             JSPrincipals** aOutPrincipals);

  static bool ReadKnownPrincipalType(JSContext* aCx,
                                     JSStructuredCloneReader* aReader,
                                     uint32_t aTag,
                                     JSPrincipals** aOutPrincipals);

  /* For write() implementations of off-main-thread JSPrincipals. */
  static bool WritePrincipalInfo(JSStructuredCloneWriter* aWriter,
                                 const mozilla::ipc::PrincipalInfo& aInfo);
  // This class is used on the main thread to specify which principal to use
  // when reading principals data that was set on a DOM worker thread.
  // DOM workers do not use principals from Gecko's point of view, and any
  // JSPrincipals used internally will be a shared singleton object. When that
  // singleton is written out and later read on the main thread, we substitute
  // the principal specified with this class.
  struct MOZ_RAII AutoSetActiveWorkerPrincipal {
    explicit AutoSetActiveWorkerPrincipal(nsIPrincipal* aPrincipal);
    ~AutoSetActiveWorkerPrincipal();
  };

  bool write(JSContext* aCx, JSStructuredCloneWriter* aWriter) final;

  /*
   * Get a weak reference to nsIPrincipal associated with the given JS
   * principal, and vice-versa.
   */
  static nsJSPrincipals* get(JSPrincipals* principals) {
    nsJSPrincipals* self = static_cast<nsJSPrincipals*>(principals);
    MOZ_ASSERT_IF(self, self->debugToken == DEBUG_TOKEN);
    return self;
  }
  static nsJSPrincipals* get(nsIPrincipal* principal) {
    nsJSPrincipals* self = static_cast<nsJSPrincipals*>(principal);
    MOZ_ASSERT_IF(self, self->debugToken == DEBUG_TOKEN);
    return self;
  }

  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) override;
  NS_IMETHOD_(MozExternalRefCountType) Release(void) override;

  nsJSPrincipals() {
    refcount = 0;
    setDebugToken(DEBUG_TOKEN);
  }

  /**
   * Return a string that can be used as JS script filename in error reports.
   */
  virtual nsresult GetScriptLocation(nsACString& aStr) = 0;
  static const uint32_t DEBUG_TOKEN = 0x0bf41760;

 protected:
  virtual ~nsJSPrincipals() { setDebugToken(0); }
};

#endif /* nsJSPrincipals_h__ */
