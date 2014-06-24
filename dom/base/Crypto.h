/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_Crypto_h
#define mozilla_dom_Crypto_h

#ifdef MOZ_DISABLE_CRYPTOLEGACY
#include "nsIDOMCrypto.h"
#else
#include "nsIDOMCryptoLegacy.h"
namespace mozilla {
namespace dom {
class CRMFObject;
}
}
#endif

#include "mozilla/dom/SubtleCrypto.h"
#include "nsPIDOMWindow.h"

#include "nsWrapperCache.h"
#include "mozilla/dom/TypedArray.h"
#define NS_DOMCRYPTO_CID \
  {0x929d9320, 0x251e, 0x11d4, { 0x8a, 0x7c, 0x00, 0x60, 0x08, 0xc8, 0x44, 0xc3} }

namespace mozilla {

class ErrorResult;

namespace dom {

class Crypto : public nsIDOMCrypto,
               public nsWrapperCache
{
protected:
  virtual ~Crypto();

public:
  Crypto();

  NS_DECL_NSIDOMCRYPTO

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Crypto)

  void
  GetRandomValues(JSContext* aCx, const ArrayBufferView& aArray,
		  JS::MutableHandle<JSObject*> aRetval,
		  ErrorResult& aRv);

  SubtleCrypto*
  Subtle();

#ifndef MOZ_DISABLE_CRYPTOLEGACY
  virtual bool EnableSmartCardEvents();
  virtual void SetEnableSmartCardEvents(bool aEnable, ErrorResult& aRv);

  virtual void GetVersion(nsString& aVersion);

  virtual mozilla::dom::CRMFObject*
  GenerateCRMFRequest(JSContext* aContext,
                      const nsCString& aReqDN,
                      const nsCString& aRegToken,
                      const nsCString& aAuthenticator,
                      const nsCString& aEaCert,
                      const nsCString& aJsCallback,
                      const Sequence<JS::Value>& aArgs,
                      ErrorResult& aRv);

  virtual void ImportUserCertificates(const nsAString& aNickname,
                                      const nsAString& aCmmfResponse,
                                      bool aDoForcedBackup,
                                      nsAString& aReturn,
                                      ErrorResult& aRv);

  virtual void SignText(JSContext* aContext,
                        const nsAString& aStringToSign,
                        const nsAString& aCaOption,
                        const Sequence<nsCString>& aArgs,
                        nsAString& aReturn);

  virtual void Logout(ErrorResult& aRv);

#endif

  // WebIDL

  nsPIDOMWindow*
  GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static uint8_t*
  GetRandomValues(uint32_t aLength);

private:
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsRefPtr<SubtleCrypto> mSubtle;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Crypto_h
