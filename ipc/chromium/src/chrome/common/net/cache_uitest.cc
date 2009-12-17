// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/platform_thread.h"
#include "base/string_util.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "net/url_request/url_request_unittest.h"

// The CacheTest class extends the UITest class and provides functions to
// get a new tab and to run the tests on a particular path
//
// Typical usage:
//
//   1. Provide this class as the TestCase for TEST_F macro
//   2. Then run the cache test on a specific path using the function
//      RunCacheTest
//
// For example:
//
//  TEST_F(CacheTest, NoCacheMaxAge) {
//    RunCacheTest(L"nocachetime/maxage", false, false);
//  }
//
// Note that delays used in running the test is initialized to defaults
class CacheTest : public UITest {
  protected:

    // Runs the cache test for the specified path.
    // Can specify the test to check if a new tab is loaded from the cache
    // and also if a delayed reload is required. A true value passed to the
    // third parameter causes a delayed reload of the path in a new tab.
    // The amount of delay is set by a class constant.
    void RunCacheTest(const std::wstring &url,
                      bool expect_new_tab_cached,
                      bool expect_delayed_reload);

  private:
    // Class constants
    static const int kWaitForCacheUpdateMsec = 2000;
    static const int kCacheWaitMultiplier = 4;  // Used to increase delay

    // Appends a new tab to the test chrome window and loads the specified
    // URL. The new tab will try to get the URL from the cache before requesting
    // the server for it.
    void GetNewTab(AutomationProxy* automationProxy, const GURL& tab_url);
};

// Runs the cache test for the specified path.
void CacheTest::RunCacheTest(const std::wstring &url,
                             bool expect_new_tab_cached,
                             bool expect_delayed_reload) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data", NULL);
  ASSERT_TRUE(NULL != server.get());
  GURL test_page(server->TestServerPageW(url));

  NavigateToURL(test_page);
  std::wstring original_time = GetActiveTabTitle();

  PlatformThread::Sleep(kWaitForCacheUpdateMsec);

  GetNewTab(automation(), test_page);
  std::wstring revisit_time = GetActiveTabTitle();

  if (expect_new_tab_cached) {
    EXPECT_EQ(original_time, revisit_time);
  }else {
    EXPECT_NE(original_time, revisit_time);
  }

  PlatformThread::Sleep(kWaitForCacheUpdateMsec);

  // Force reload, overriding the caching behavior
  NavigateToURL(test_page);
  std::wstring reload_time = GetActiveTabTitle();

  EXPECT_NE(revisit_time, reload_time);

  if (expect_delayed_reload) {
    PlatformThread::Sleep(kWaitForCacheUpdateMsec * kCacheWaitMultiplier);

    GetNewTab(automation(), test_page);
    revisit_time = GetActiveTabTitle();

    EXPECT_NE(reload_time, revisit_time);
  }
}

// Appends a new tab to the test chrome window and loads the specified URL.
void CacheTest::GetNewTab(AutomationProxy* automationProxy,
                          const GURL& tab_url) {
  scoped_ptr<BrowserProxy> window_proxy(automationProxy->GetBrowserWindow(0));
  ASSERT_TRUE(window_proxy.get());
  ASSERT_TRUE(window_proxy->AppendTab(tab_url));
}

// Tests that a cached copy of the page is not used when max-age=0 headers
// are specified.
TEST_F(CacheTest, NoCacheMaxAge) {
  RunCacheTest(L"nocachetime/maxage", false, false);
}

// Tests that a cached copy of the page is not used when no-cache header
// is specified.
TEST_F(CacheTest, NoCache) {
  RunCacheTest(L"nocachetime", false, false);
}

// Tests that a cached copy of a page is used when its headers specify
// that it should be cached for 60 seconds.
TEST_F(CacheTest, Cache) {
  RunCacheTest(L"cachetime", true, false);
}

// Tests that a cached copy of the page is used when expires header
// specifies that the page has not yet expired.
TEST_F(CacheTest, Expires) {
  RunCacheTest(L"cache/expires", true, false);
}

// Tests that a cached copy of the page is used when proxy-revalidate header
// is specified and the page has not yet expired.
TEST_F(CacheTest, ProxyRevalidate) {
  RunCacheTest(L"cache/proxy-revalidate", true, false);
}

// Tests that a cached copy of the page is used when private header
// is specified and the page has not yet expired.
TEST_F(CacheTest, Private) {
  RunCacheTest(L"cache/private", true, true);
}

// Tests that a cached copy of the page is used when public header
// is specified and the page has not yet expired.
TEST_F(CacheTest, Public) {
  RunCacheTest(L"cache/public", true, true);
}

// Tests that a cached copy of the page is not used when s-maxage header
// is specified.
TEST_F(CacheTest, SMaxAge) {
  RunCacheTest(L"cache/s-maxage", false, false);
}

// Tests that a cached copy of the page is not used when must-revalidate header
// is specified.
TEST_F(CacheTest, MustRevalidate) {
  RunCacheTest(L"cache/must-revalidate", false, false);
}

// Tests that a cached copy of the page is not used when must-revalidate header
// is specified, even though the page has not yet expired.
TEST_F(CacheTest, MustRevalidateMaxAge) {
  RunCacheTest(L"cache/must-revalidate/max-age", false, false);
}

// Tests that a cached copy of the page is not used when no-store header
// is specified.
TEST_F(CacheTest, NoStore) {
  RunCacheTest(L"cache/no-store", false, false);
}

// Tests that a cached copy of the page is not used when no-store header
// is specified, even though the page has not yet expired.
TEST_F(CacheTest, NoStoreMaxAge) {
  RunCacheTest(L"cache/no-store/max-age", false, false);
}

// Tests that a cached copy of the page is not transformed when no-transform
// header is specified.
TEST_F(CacheTest, NoTransform) {
  RunCacheTest(L"cache/no-transform", false, false);
}
