/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsGlobalWindow.h"

struct dialog_test {
  const char* input;
  const char* output;
};

void runTokenizeTest(dialog_test& test)
{
  NS_ConvertUTF8toUTF16 input(test.input);

  nsAString::const_iterator end;
  input.EndReading(end);

  nsAString::const_iterator iter;
  input.BeginReading(iter);

  nsAutoString result;
  nsAutoString token;

  while (nsGlobalWindow::TokenizeDialogOptions(token, iter, end)) {
    if (!result.IsEmpty()) {
      result.Append(',');
    }

    result.Append(token);
  }

  ASSERT_STREQ(test.output, NS_ConvertUTF16toUTF8(result).get()) << "Testing " << test.input;
}

void runTest(dialog_test& test)
{
  NS_ConvertUTF8toUTF16 input(test.input);

  nsAutoString result;
  nsGlobalWindow::ConvertDialogOptions(input, result);

  ASSERT_STREQ(test.output, NS_ConvertUTF16toUTF8(result).get()) << "Testing " << test.input;
}

TEST(GlobalWindowDialogOptions, TestDialogTokenize)
{
  dialog_test tests[] = {
    /// Empty strings
    { "", "" },
    { " ", "" },
    { "   ", "" },

    // 1 token
    { "a", "a" },
    { " a", "a" },
    { "  a  ", "a" },
    { "aa", "aa" },
    { " aa", "aa" },
    { "  aa  ", "aa" },
    { ";", ";" },
    { ":", ":" },
    { "=", "=" },

    // 2 tokens
    { "a=", "a,=" },
    { "  a=  ", "a,=" },
    { "  a  =  ", "a,=" },
    { "aa=", "aa,=" },
    { "  aa=  ", "aa,=" },
    { "  aa  =  ", "aa,=" },
    { ";= ", ";,=" },
    { "==", "=,=" },
    { "::", ":,:" },

    // 3 tokens
    { "a=2", "a,=,2" },
    { "===", "=,=,=" },
    { ";:=", ";,:,=" },

    // more
    { "aaa;bbb:ccc", "aaa,;,bbb,:,ccc" },

    // sentinel
    { nullptr, nullptr }
  };

  for (uint32_t i = 0; tests[i].input; ++i) {
    runTokenizeTest(tests[i]);
  }
}
TEST(GlobalWindowDialogOptions, TestDialogOptions)
{
  dialog_test tests[] = {
    /// Empty strings
    { "", "" },
    { " ", "" },
    { "   ", "" },

    // Name without params
    { "a", "" },
    { " a", "" },
    { "  a  ", "" },
    { "a=", "" },
    { "  a=  ", "" },
    { "  a  =  ", "" },

    // 1 unknown value
    { "a=2", "" },
    { " a=2 ", "" },
    { "  a  =  2 ", "" },
    { "a:2", "" },
    { " a:2 ", "" },
    { "  a  :  2 ", "" },

    // 1 known value, wrong value
    { "center=2", "" },
    { "center:2", "" },

    // 1 known value, good value
    { "center=on", ",centerscreen=1" },
    { "center:on", ",centerscreen=1" },
    { " center : on ", ",centerscreen=1" },

    // nonsense stuff
    { " ; ", "" },

    // sentinel
    { nullptr, nullptr }
  };

  for (uint32_t i = 0; tests[i].input; ++i) {
    runTest(tests[i]);
  }
}
