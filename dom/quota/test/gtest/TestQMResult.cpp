/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/QMResultInlines.h"

using namespace mozilla;

TEST(DOM_Quota_QMResult, Construct_Default)
{
  QMResult res;

  ASSERT_EQ(res.StackId(), 1u);
  ASSERT_EQ(res.FrameId(), 1u);
  ASSERT_EQ(res.NSResult(), NS_OK);
}

TEST(DOM_Quota_QMResult, Construct_FromNSResult)
{
  QMResult res(NS_ERROR_FAILURE);

  ASSERT_EQ(res.StackId(), 2u);
  ASSERT_EQ(res.FrameId(), 1u);
  ASSERT_EQ(res.NSResult(), NS_ERROR_FAILURE);
}

TEST(DOM_Quota_QMResult, Propagate)
{
  QMResult res1(NS_ERROR_FAILURE);

  ASSERT_EQ(res1.StackId(), 3u);
  ASSERT_EQ(res1.FrameId(), 1u);
  ASSERT_EQ(res1.NSResult(), NS_ERROR_FAILURE);

  QMResult res2 = res1.Propagate();

  ASSERT_EQ(res2.StackId(), 3u);
  ASSERT_EQ(res2.FrameId(), 2u);
  ASSERT_EQ(res2.NSResult(), NS_ERROR_FAILURE);
}

TEST(DOM_Quota_QMResult, ToQMResult)
{
  auto res = ToQMResult(NS_ERROR_FAILURE);

  ASSERT_EQ(res.StackId(), 4u);
  ASSERT_EQ(res.FrameId(), 1u);
  ASSERT_EQ(res.NSResult(), NS_ERROR_FAILURE);
}

TEST(DOM_Quota_QMResult, ToResult)
{
  // copy
  {
    const auto res = ToQMResult(NS_ERROR_FAILURE);
    auto valOrErr = ToResult(res);
    static_assert(std::is_same_v<decltype(valOrErr), Result<Ok, QMResult>>);
    ASSERT_TRUE(valOrErr.isErr());
    auto err = valOrErr.unwrapErr();
    ASSERT_EQ(err.StackId(), 5u);
    ASSERT_EQ(err.FrameId(), 1u);
    ASSERT_EQ(err.NSResult(), NS_ERROR_FAILURE);
  }

  // move
  {
    auto res = ToQMResult(NS_ERROR_FAILURE);
    auto valOrErr = ToResult(std::move(res));
    static_assert(std::is_same_v<decltype(valOrErr), Result<Ok, QMResult>>);
    ASSERT_TRUE(valOrErr.isErr());
    auto err = valOrErr.unwrapErr();
    ASSERT_EQ(err.StackId(), 6u);
    ASSERT_EQ(err.FrameId(), 1u);
    ASSERT_EQ(err.NSResult(), NS_ERROR_FAILURE);
  }
}
