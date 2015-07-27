/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_icc_IccIPCUtils_h
#define mozilla_dom_icc_IccIPCUtils_h

class nsIIccContact;
class nsIIccInfo;

namespace mozilla {
namespace dom {
namespace icc {

class IccInfoData;
class IccContactData;

class IccIPCUtils
{
public:
  static void GetIccInfoDataFromIccInfo(nsIIccInfo* aInInfo,
                                        IccInfoData& aOutData);
  static void GetIccContactDataFromIccContact(nsIIccContact* aContact,
                                              IccContactData& aOutData);

private:
  IccIPCUtils() {}
  virtual ~IccIPCUtils() {}
};

} // namespace icc
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_icc_IccIPCUtils_h