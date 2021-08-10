/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/QMResult.h"

#include "gtest/gtest.h"

#include "mozilla/dom/QMResultInlines.h"

using namespace mozilla;

static_assert(std::is_same_v<OkOrErr, Result<Ok, QMResult>>);

#ifdef QM_ERROR_STACKS_ENABLED
TEST(DOM_Quota_QMResult, Construct_Default)
{
  QMResult res;

  ASSERT_EQ(res.StackId(), 1u);
  ASSERT_EQ(res.FrameId(), 1u);
  ASSERT_EQ(res.NSResult(), NS_OK);
}
#endif

TEST(DOM_Quota_QMResult, Construct_FromNSResult)
{
  QMResult res(NS_ERROR_FAILURE);

#ifdef QM_ERROR_STACKS_ENABLED
  ASSERT_EQ(res.StackId(), 2u);
  ASSERT_EQ(res.FrameId(), 1u);
  ASSERT_EQ(res.NSResult(), NS_ERROR_FAILURE);
#else
  ASSERT_EQ(res, NS_ERROR_FAILURE);
#endif
}

#ifdef QM_ERROR_STACKS_ENABLED
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
#endif

TEST(DOM_Quota_QMResult, ToQMResult)
{
  auto res = ToQMResult(NS_ERROR_FAILURE);

#ifdef QM_ERROR_STACKS_ENABLED
  ASSERT_EQ(res.StackId(), 4u);
  ASSERT_EQ(res.FrameId(), 1u);
  ASSERT_EQ(res.NSResult(), NS_ERROR_FAILURE);
#else
  ASSERT_EQ(res, NS_ERROR_FAILURE);
#endif
}

TEST(DOM_Quota_QMResult, ToResult)
{
  // copy
  {
    const auto res = ToQMResult(NS_ERROR_FAILURE);
    auto okOrErr = ToResult(res);
    static_assert(std::is_same_v<decltype(okOrErr), OkOrErr>);
    ASSERT_TRUE(okOrErr.isErr());
    auto err = okOrErr.unwrapErr();
#ifdef QM_ERROR_STACKS_ENABLED
    ASSERT_EQ(err.StackId(), 5u);
    ASSERT_EQ(err.FrameId(), 1u);
    ASSERT_EQ(err.NSResult(), NS_ERROR_FAILURE);
#else
    ASSERT_EQ(err, NS_ERROR_FAILURE);
#endif
  }

  // move
  {
    auto res = ToQMResult(NS_ERROR_FAILURE);
    auto okOrErr = ToResult(std::move(res));
    static_assert(std::is_same_v<decltype(okOrErr), OkOrErr>);
    ASSERT_TRUE(okOrErr.isErr());
    auto err = okOrErr.unwrapErr();
#ifdef QM_ERROR_STACKS_ENABLED
    ASSERT_EQ(err.StackId(), 6u);
    ASSERT_EQ(err.FrameId(), 1u);
    ASSERT_EQ(err.NSResult(), NS_ERROR_FAILURE);
#else
    ASSERT_EQ(err, NS_ERROR_FAILURE);
#endif
  }
}

TEST(DOM_Quota_QMResult, ErrorPropagation)
{
  OkOrErr okOrErr1 = ToResult(ToQMResult(NS_ERROR_FAILURE));
  const auto& err1 = okOrErr1.inspectErr();
#ifdef QM_ERROR_STACKS_ENABLED
  ASSERT_EQ(err1.StackId(), 7u);
  ASSERT_EQ(err1.FrameId(), 1u);
  ASSERT_EQ(err1.NSResult(), NS_ERROR_FAILURE);
#else
  ASSERT_EQ(err1, NS_ERROR_FAILURE);
#endif

  OkOrErr okOrErr2 = okOrErr1.propagateErr();
  const auto& err2 = okOrErr2.inspectErr();
#ifdef QM_ERROR_STACKS_ENABLED
  ASSERT_EQ(err2.StackId(), 7u);
  ASSERT_EQ(err2.FrameId(), 2u);
  ASSERT_EQ(err2.NSResult(), NS_ERROR_FAILURE);
#else
  ASSERT_EQ(err2, NS_ERROR_FAILURE);
#endif

  OkOrErr okOrErr3 = okOrErr2.propagateErr();
  const auto& err3 = okOrErr3.inspectErr();
#ifdef QM_ERROR_STACKS_ENABLED
  ASSERT_EQ(err3.StackId(), 7u);
  ASSERT_EQ(err3.FrameId(), 3u);
  ASSERT_EQ(err3.NSResult(), NS_ERROR_FAILURE);
#else
  ASSERT_EQ(err3, NS_ERROR_FAILURE);
#endif
}
