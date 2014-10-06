/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IccInfo_h
#define mozilla_dom_IccInfo_h

#include "MozIccInfoBinding.h"
#include "nsIIccInfo.h"
#include "nsWrapperCache.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class IccInfo : public nsISupports
              , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IccInfo)

  explicit IccInfo(nsPIDOMWindow* aWindow);

  void
  Update(nsIIccInfo* aInfo);

  nsPIDOMWindow*
  GetParentObject() const
  {
    return mWindow;
  }

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL interface
  Nullable<IccType>
  GetIccType() const;

  void
  GetIccid(nsAString& aIccId) const;

  void
  GetMcc(nsAString& aMcc) const;

  void
  GetMnc(nsAString& aMnc) const;

  void
  GetSpn(nsAString& aSpn) const;

  bool
  IsDisplayNetworkNameRequired() const;

  bool
  IsDisplaySpnRequired() const;

protected:
  ~IccInfo() {}

protected:
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsCOMPtr<nsIIccInfo> mIccInfo;
};

class GsmIccInfo MOZ_FINAL : public IccInfo
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(GsmIccInfo, IccInfo)

  explicit GsmIccInfo(nsPIDOMWindow* aWindow);

  void
  Update(nsIGsmIccInfo* aInfo);

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // MozCdmaIccInfo WebIDL
  void
  GetMsisdn(nsAString& aMsisdn) const;

private:
  ~GsmIccInfo() {}

private:
  nsCOMPtr<nsIGsmIccInfo> mGsmIccInfo;
};

class CdmaIccInfo MOZ_FINAL : public IccInfo
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CdmaIccInfo, IccInfo)

  explicit CdmaIccInfo(nsPIDOMWindow* aWindow);

  void
  Update(nsICdmaIccInfo* aInfo);

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // MozCdmaIccInfo WebIDL
  void
  GetMdn(nsAString& aMdn) const;

  int32_t
  PrlVersion() const;

private:
  ~CdmaIccInfo() {}

private:
  nsCOMPtr<nsICdmaIccInfo> mCdmaIccInfo;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_IccInfo_h

