/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/KeyValueStorage.h"

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"

#include "GMPTestMonitor.h"

using ::testing::Return;
using namespace mozilla;

TEST(TestKeyValueStorage, BasicPutGet)
{
  auto kvs = MakeRefPtr<KeyValueStorage>();

  nsCString name("database_name");
  nsCString key("key1");
  int32_t value = 100;

  GMPTestMonitor mon;

  kvs->Put(name, key, value)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [&](bool) { return kvs->Get(name, key); },
          [](nsresult rv) {
            EXPECT_TRUE(false) << "Put promise has been rejected";
            return KeyValueStorage::GetPromise::CreateAndReject(rv, __func__);
          })
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [&](int32_t aValue) {
            EXPECT_EQ(aValue, value) << "Values are the same";
            mon.SetFinished();
          },
          [&](nsresult rv) {
            EXPECT_TRUE(false) << "Get Promise has been rejected";
            mon.SetFinished();
          });

  mon.AwaitFinished();
}

TEST(TestKeyValueStorage, GetNonExistedKey)
{
  auto kvs = MakeRefPtr<KeyValueStorage>();

  nsCString name("database_name");
  nsCString key("NonExistedKey");

  GMPTestMonitor mon;

  kvs->Get(name, key)->Then(
      GetCurrentThreadSerialEventTarget(), __func__,
      [&mon](int32_t aValue) {
        EXPECT_EQ(aValue, -1) << "When key does not exist return -1";
        mon.SetFinished();
      },
      [&mon](nsresult rv) {
        EXPECT_TRUE(false) << "Get Promise has been rejected";
        mon.SetFinished();
      });

  mon.AwaitFinished();
}

TEST(TestKeyValueStorage, Clear)
{
  auto kvs = MakeRefPtr<KeyValueStorage>();

  nsCString name("database_name");
  nsCString key("key1");
  int32_t value = 100;

  GMPTestMonitor mon;

  kvs->Put(name, key, value)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [&](bool) { return kvs->Clear(name); },
          [](nsresult rv) {
            EXPECT_TRUE(false) << "Put promise has been rejected";
            return GenericPromise::CreateAndReject(rv, __func__);
          })
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [&](bool) { return kvs->Get(name, key); },
          [](nsresult rv) {
            EXPECT_TRUE(false) << "Clear promise has been rejected";
            return KeyValueStorage::GetPromise::CreateAndReject(rv, __func__);
          })
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [&](int32_t aValue) {
            EXPECT_EQ(aValue, -1) << "After clear the key does not exist";
            mon.SetFinished();
          },
          [&](nsresult rv) {
            EXPECT_TRUE(false) << "Get Promise has been rejected";
            mon.SetFinished();
          });

  mon.AwaitFinished();
}
