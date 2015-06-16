/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_icc_IccContact_h
#define mozilla_dom_icc_IccContact_h

#include "nsIIccContact.h"

namespace mozilla {
namespace dom {
class mozContact;
namespace icc {

class IccContact : public nsIIccContact
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIICCCONTACT

  static nsresult
  Create(mozContact& aMozContact,
         nsIIccContact** aIccContact);

  static nsresult
  Create(const nsAString& aId,
         const nsTArray<nsString>& aNames,
         const nsTArray<nsString>& aNumbers,
         const nsTArray<nsString>& aEmails,
         nsIIccContact** aIccContact);

private:
  IccContact(const nsAString& aId,
             const nsTArray<nsString>& aNames,
             const nsTArray<nsString>& aNumbers,
             const nsTArray<nsString>& aEmails);
  virtual ~IccContact() {}

  nsString mId;
  nsTArray<nsString> mNames;
  nsTArray<nsString> mNumbers;
  nsTArray<nsString> mEmails;
};

} // namespace icc
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_icc_IccContact_h

