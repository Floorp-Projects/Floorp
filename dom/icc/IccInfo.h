/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IccInfo_h
#define mozilla_dom_IccInfo_h

#include "MozIccInfoBinding.h"
#include "nsIIccInfo.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

namespace icc {
class IccInfoData;
} // namespace icc

class IccInfo : public nsIIccInfo
              , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IccInfo)
  NS_DECL_NSIICCINFO

  explicit IccInfo(nsPIDOMWindowInner* aWindow);
  explicit IccInfo(const icc::IccInfoData& aData);

  void
  Update(nsIIccInfo* aInfo);

  nsPIDOMWindowInner*
  GetParentObject() const
  {
    return mWindow;
  }

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

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
  virtual ~IccInfo() {}

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  // To prevent compiling error in OS_WIN in auto-generated UnifiedBindingsXX.cpp,
  // we have all data fields expended here instead of having a data member of
  // |IccInfoData| defined in PIccTypes.h which indirectly includes "windows.h"
  // See 925382 for the restriction of including "windows.h" in UnifiedBindings.cpp.
  nsString mIccType;
  nsString mIccid;
  nsString mMcc;
  nsString mMnc;
  nsString mSpn;
  // The following booleans shall be initialized either in the constructor or in Update
  MOZ_INIT_OUTSIDE_CTOR bool mIsDisplayNetworkNameRequired;
  MOZ_INIT_OUTSIDE_CTOR bool mIsDisplaySpnRequired;
};

class GsmIccInfo final : public IccInfo
                       , public nsIGsmIccInfo
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIICCINFO(IccInfo::)
  NS_DECL_NSIGSMICCINFO

  explicit GsmIccInfo(nsPIDOMWindowInner* aWindow);
  explicit GsmIccInfo(const icc::IccInfoData& aData);

  void
  Update(nsIGsmIccInfo* aInfo);

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // MozCdmaIccInfo WebIDL
  void
  GetMsisdn(nsAString& aMsisdn) const;

private:
  ~GsmIccInfo() {}

  nsString mPhoneNumber;
};

class CdmaIccInfo final : public IccInfo
                        , public nsICdmaIccInfo
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIICCINFO(IccInfo::)
  NS_DECL_NSICDMAICCINFO

  explicit CdmaIccInfo(nsPIDOMWindowInner* aWindow);
  explicit CdmaIccInfo(const icc::IccInfoData& aData);

  void
  Update(nsICdmaIccInfo* aInfo);

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // MozCdmaIccInfo WebIDL
  void
  GetMdn(nsAString& aMdn) const;

  int32_t
  PrlVersion() const;

private:
  ~CdmaIccInfo() {}

  nsString mPhoneNumber;
  // The following integer shall be initialized either in the constructor or in Update
  MOZ_INIT_OUTSIDE_CTOR int32_t mPrlVersion;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_IccInfo_h

