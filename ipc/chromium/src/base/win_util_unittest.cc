// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/win_util.h"

// The test is somewhat silly, because the Vista bots some have UAC enabled
// and some have it disabled. At least we check that it does not crash.
TEST(BaseWinUtilTest, TestIsUACEnabled) {
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA) {
    win_util::UserAccountControlIsEnabled();
  } else {
    EXPECT_TRUE(win_util::UserAccountControlIsEnabled());
  }
}

TEST(BaseWinUtilTest, TestGetUserSidString) {
  std::wstring user_sid;
  EXPECT_TRUE(win_util::GetUserSidString(&user_sid));
  EXPECT_TRUE(!user_sid.empty());
}

TEST(BaseWinUtilTest, TestGetNonClientMetrics) {
  NONCLIENTMETRICS metrics = {0};
  win_util::GetNonClientMetrics(&metrics);
  EXPECT_TRUE(metrics.cbSize > 0);
  EXPECT_TRUE(metrics.iScrollWidth > 0);
  EXPECT_TRUE(metrics.iScrollHeight > 0);
}

namespace {

// Saves the current thread's locale ID when initialized, and restores it when
// the instance is going out of scope.
class ThreadLocaleSaver {
 public:
  ThreadLocaleSaver() : original_locale_id_(GetThreadLocale()) {}
  ~ThreadLocaleSaver() { SetThreadLocale(original_locale_id_); }

 private:
  LCID original_locale_id_;

  DISALLOW_COPY_AND_ASSIGN(ThreadLocaleSaver);
};

}  // namespace

TEST(BaseWinUtilTest, FormatMessage) {
  // Because we cannot write tests of every language, we only test the message
  // of en-US locale. Here, we change the current locale temporarily.
  ThreadLocaleSaver thread_locale_saver;
  WORD language_id = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
  LCID locale_id = MAKELCID(language_id, SORT_DEFAULT);
  ASSERT_TRUE(SetThreadLocale(locale_id));

  const int kAccessDeniedErrorCode = 5;
  SetLastError(kAccessDeniedErrorCode);
  ASSERT_EQ(GetLastError(), kAccessDeniedErrorCode);
  std::wstring value;
  TrimWhitespace(win_util::FormatLastWin32Error(), TRIM_ALL, &value);
  EXPECT_EQ(std::wstring(L"Access is denied."), value);

  // Manually call the OS function
  wchar_t * string_buffer = NULL;
  unsigned string_length =
      ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      kAccessDeniedErrorCode, 0,
                      reinterpret_cast<wchar_t *>(&string_buffer), 0, NULL);

  // Verify the call succeeded
  ASSERT_TRUE(string_length);
  ASSERT_TRUE(string_buffer);

  // Verify the string is the same by different calls
  EXPECT_EQ(win_util::FormatLastWin32Error(), std::wstring(string_buffer));
  EXPECT_EQ(win_util::FormatMessage(kAccessDeniedErrorCode),
            std::wstring(string_buffer));

  // Done with the buffer allocated by ::FormatMessage()
  LocalFree(string_buffer);
}
