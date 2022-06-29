/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_TEST_GTEST_TESTHELPERS_H_
#define DOM_FS_TEST_GTEST_TESTHELPERS_H_

#include "gtest/gtest.h"

#include "ErrorList.h"
#include "mozilla/ErrorNames.h"

#define ASSERT_NSEQ(lhs, rhs) \
  ASSERT_STREQ(GetStaticErrorName((lhs)), GetStaticErrorName((rhs)))

#endif  // DOM_FS_TEST_GTEST_TESTHELPERS_H_
