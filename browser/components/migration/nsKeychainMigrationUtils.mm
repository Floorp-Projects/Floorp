/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsKeychainMigrationUtils.h"

#include <Security/Security.h>

#include "mozilla/Logging.h"

#include "nsCocoaUtils.h"
#include "nsString.h"

using namespace mozilla;

LazyLogModule gKeychainUtilsLog("keychainmigrationutils");

NS_IMPL_ISUPPORTS(nsKeychainMigrationUtils, nsIKeychainMigrationUtils)

NS_IMETHODIMP
nsKeychainMigrationUtils::GetGenericPassword(const nsACString& aServiceName,
                                             const nsACString& aAccountName,
                                             nsACString& aKey) {
  // To retrieve a secret, we create a CFDictionary of the form:
  // { class: generic password,
  //   service: the given service name
  //   account: the given account name,
  //   match limit: match one,
  //   return attributes: true,
  //   return data: true }
  // This searches for and returns the attributes and data for the secret
  // matching the given service and account names. We then extract the data
  // (i.e. the secret) and return it.
  NSDictionary* searchDictionary = @{
    (__bridge NSString*)
    kSecClass : (__bridge NSString*)kSecClassGenericPassword,
    (__bridge NSString*)
    kSecAttrService : nsCocoaUtils::ToNSString(aServiceName),
    (__bridge NSString*)
    kSecAttrAccount : nsCocoaUtils::ToNSString(aAccountName),
    (__bridge NSString*)kSecMatchLimit : (__bridge NSString*)kSecMatchLimitOne,
    (__bridge NSString*)kSecReturnAttributes : @YES,
    (__bridge NSString*)kSecReturnData : @YES
  };

  CFTypeRef item;
  // https://developer.apple.com/documentation/security/1398306-secitemcopymatching
  OSStatus rv =
      SecItemCopyMatching((__bridge CFDictionaryRef)searchDictionary, &item);
  if (rv != errSecSuccess) {
    MOZ_LOG(gKeychainUtilsLog, LogLevel::Debug,
            ("SecItemCopyMatching failed: %d", rv));
    return NS_ERROR_FAILURE;
  }
  NSDictionary* resultDict = [(__bridge NSDictionary*)item autorelease];
  NSData* secret = [resultDict objectForKey:(__bridge NSString*)kSecValueData];
  if (!secret) {
    MOZ_LOG(gKeychainUtilsLog, LogLevel::Debug, ("objectForKey failed"));
    return NS_ERROR_FAILURE;
  }
  if ([secret length] != 0) {
    // We assume that the data is UTF-8 encoded since that seems to be common
    // and Keychain Access shows it with that encoding.
    aKey.Assign(reinterpret_cast<const char*>([secret bytes]), [secret length]);
  }

  return NS_OK;
}
