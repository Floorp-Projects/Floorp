/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsScriptSecurityManager.h"

namespace mozilla {

TEST(ScriptSecurityManager, IsHttpOrHttpsAndCrossOrigin)
{
  nsCOMPtr<nsIURI> uriA;
  NS_NewURI(getter_AddRefs(uriA), "https://apple.com");
  nsCOMPtr<nsIURI> uriB;
  NS_NewURI(getter_AddRefs(uriB), "https://google.com");
  nsCOMPtr<nsIURI> uriB_http;
  NS_NewURI(getter_AddRefs(uriB_http), "http://google.com");
  nsCOMPtr<nsIURI> aboutBlank;
  NS_NewURI(getter_AddRefs(aboutBlank), "about:blank");
  nsCOMPtr<nsIURI> aboutConfig;
  NS_NewURI(getter_AddRefs(aboutConfig), "about:config");
  nsCOMPtr<nsIURI> example_com;
  NS_NewURI(getter_AddRefs(example_com), "https://example.com");
  nsCOMPtr<nsIURI> example_com_with_path;
  NS_NewURI(getter_AddRefs(example_com_with_path),
            "https://example.com/test/1/2/3");
  nsCOMPtr<nsIURI> nullURI = nullptr;

  ASSERT_TRUE(nsScriptSecurityManager::IsHttpOrHttpsAndCrossOrigin(uriA, uriB));
  ASSERT_TRUE(
      nsScriptSecurityManager::IsHttpOrHttpsAndCrossOrigin(uriB, uriB_http));

  ASSERT_FALSE(nsScriptSecurityManager::IsHttpOrHttpsAndCrossOrigin(
      aboutBlank, aboutConfig));
  ASSERT_FALSE(nsScriptSecurityManager::IsHttpOrHttpsAndCrossOrigin(
      aboutBlank, aboutBlank));
  ASSERT_FALSE(
      nsScriptSecurityManager::IsHttpOrHttpsAndCrossOrigin(uriB, aboutConfig));
  ASSERT_FALSE(nsScriptSecurityManager::IsHttpOrHttpsAndCrossOrigin(
      example_com, example_com_with_path));
  ASSERT_FALSE(
      nsScriptSecurityManager::IsHttpOrHttpsAndCrossOrigin(uriB_http, nullURI));
  ASSERT_FALSE(
      nsScriptSecurityManager::IsHttpOrHttpsAndCrossOrigin(nullURI, uriB_http));
}

}  // namespace mozilla
