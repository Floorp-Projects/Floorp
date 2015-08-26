/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "CopyOnWrite.h"

using namespace mozilla;
using namespace mozilla::image;

struct ValueStats
{
  int32_t mCopies = 0;
  int32_t mFrees = 0;
  int32_t mCalls = 0;
  int32_t mConstCalls = 0;
  int32_t mSerial = 0;
};

struct Value
{
  NS_INLINE_DECL_REFCOUNTING(Value)

  explicit Value(ValueStats& aStats)
    : mStats(aStats)
    , mSerial(mStats.mSerial++)
  { }

  Value(const Value& aOther)
    : mStats(aOther.mStats)
    , mSerial(mStats.mSerial++)
  {
    mStats.mCopies++;
  }

  void Go() { mStats.mCalls++; }
  void Go() const { mStats.mConstCalls++; }

  int32_t Serial() const { return mSerial; }

protected:
  ~Value() { mStats.mFrees++; }

private:
  ValueStats& mStats;
  int32_t mSerial;
};

TEST(ImageCopyOnWrite, Read)
{
  ValueStats stats;

  {
    CopyOnWrite<Value> cow(new Value(stats));

    EXPECT_EQ(0, stats.mCopies);
    EXPECT_EQ(0, stats.mFrees);
    EXPECT_TRUE(cow.CanRead());

    cow.Read([&](const Value* aValue) {
      EXPECT_EQ(0, stats.mCopies);
      EXPECT_EQ(0, stats.mFrees);
      EXPECT_EQ(0, aValue->Serial());
      EXPECT_TRUE(cow.CanRead());
      EXPECT_TRUE(cow.CanWrite());

      aValue->Go();

      EXPECT_EQ(0, stats.mCalls);
      EXPECT_EQ(1, stats.mConstCalls);
    });

    EXPECT_EQ(0, stats.mCopies);
    EXPECT_EQ(0, stats.mFrees);
    EXPECT_EQ(0, stats.mCalls);
    EXPECT_EQ(1, stats.mConstCalls);
  }

  EXPECT_EQ(0, stats.mCopies);
  EXPECT_EQ(1, stats.mFrees);
}

TEST(ImageCopyOnWrite, RecursiveRead)
{
  ValueStats stats;

  {
    CopyOnWrite<Value> cow(new Value(stats));

    EXPECT_EQ(0, stats.mCopies);
    EXPECT_EQ(0, stats.mFrees);
    EXPECT_TRUE(cow.CanRead());

    cow.Read([&](const Value* aValue) {
      EXPECT_EQ(0, stats.mCopies);
      EXPECT_EQ(0, stats.mFrees);
      EXPECT_EQ(0, aValue->Serial());
      EXPECT_TRUE(cow.CanRead());
      EXPECT_TRUE(cow.CanWrite());

      // Make sure that Read() inside a Read() succeeds.
      cow.Read([&](const Value* aValue) {
        EXPECT_EQ(0, stats.mCopies);
        EXPECT_EQ(0, stats.mFrees);
        EXPECT_EQ(0, aValue->Serial());
        EXPECT_TRUE(cow.CanRead());
        EXPECT_TRUE(cow.CanWrite());

        aValue->Go();

        EXPECT_EQ(0, stats.mCalls);
        EXPECT_EQ(1, stats.mConstCalls);
      }, []() {
        // This gets called if we can't read. We shouldn't get here.
        EXPECT_TRUE(false);
      });
    });

    EXPECT_EQ(0, stats.mCopies);
    EXPECT_EQ(0, stats.mFrees);
    EXPECT_EQ(0, stats.mCalls);
    EXPECT_EQ(1, stats.mConstCalls);
  }

  EXPECT_EQ(0, stats.mCopies);
  EXPECT_EQ(1, stats.mFrees);
}

TEST(ImageCopyOnWrite, Write)
{
  ValueStats stats;

  {
    CopyOnWrite<Value> cow(new Value(stats));

    EXPECT_EQ(0, stats.mCopies);
    EXPECT_EQ(0, stats.mFrees);
    EXPECT_TRUE(cow.CanRead());
    EXPECT_TRUE(cow.CanWrite());

    cow.Write([&](Value* aValue) {
      EXPECT_EQ(0, stats.mCopies);
      EXPECT_EQ(0, stats.mFrees);
      EXPECT_EQ(0, aValue->Serial());
      EXPECT_TRUE(!cow.CanRead());
      EXPECT_TRUE(!cow.CanWrite());

      aValue->Go();

      EXPECT_EQ(1, stats.mCalls);
      EXPECT_EQ(0, stats.mConstCalls);
    });

    EXPECT_EQ(0, stats.mCopies);
    EXPECT_EQ(0, stats.mFrees);
    EXPECT_EQ(1, stats.mCalls);
    EXPECT_EQ(0, stats.mConstCalls);
  }

  EXPECT_EQ(0, stats.mCopies);
  EXPECT_EQ(1, stats.mFrees);
}

TEST(ImageCopyOnWrite, WriteRecursive)
{
  ValueStats stats;

  {
    CopyOnWrite<Value> cow(new Value(stats));

    EXPECT_EQ(0, stats.mCopies);
    EXPECT_EQ(0, stats.mFrees);
    EXPECT_TRUE(cow.CanRead());
    EXPECT_TRUE(cow.CanWrite());

    cow.Read([&](const Value* aValue) {
      EXPECT_EQ(0, stats.mCopies);
      EXPECT_EQ(0, stats.mFrees);
      EXPECT_EQ(0, aValue->Serial());
      EXPECT_TRUE(cow.CanRead());
      EXPECT_TRUE(cow.CanWrite());

      // Make sure Write() inside a Read() succeeds.
      cow.Write([&](Value* aValue) {
        EXPECT_EQ(1, stats.mCopies);
        EXPECT_EQ(0, stats.mFrees);
        EXPECT_EQ(1, aValue->Serial());
        EXPECT_TRUE(!cow.CanRead());
        EXPECT_TRUE(!cow.CanWrite());

        aValue->Go();

        EXPECT_EQ(1, stats.mCalls);
        EXPECT_EQ(0, stats.mConstCalls);

        // Make sure Read() inside a Write() fails.
        cow.Read([](const Value* aValue) {
          // This gets called if we can read. We shouldn't get here.
          EXPECT_TRUE(false);
        }, []() {
          // This gets called if we can't read. We *should* get here.
          EXPECT_TRUE(true);
        });

        // Make sure Write() inside a Write() fails.
        cow.Write([](Value* aValue) {
          // This gets called if we can write. We shouldn't get here.
          EXPECT_TRUE(false);
        }, []() {
          // This gets called if we can't write. We *should* get here.
          EXPECT_TRUE(true);
        });
      }, []() {
        // This gets called if we can't write. We shouldn't get here.
        EXPECT_TRUE(false);
      });

      aValue->Go();

      EXPECT_EQ(1, stats.mCopies);
      EXPECT_EQ(0, stats.mFrees);
      EXPECT_EQ(1, stats.mCalls);
      EXPECT_EQ(1, stats.mConstCalls);
    });

    EXPECT_EQ(1, stats.mCopies);
    EXPECT_EQ(1, stats.mFrees);
    EXPECT_EQ(1, stats.mCalls);
    EXPECT_EQ(1, stats.mConstCalls);
  }

  EXPECT_EQ(1, stats.mCopies);
  EXPECT_EQ(2, stats.mFrees);
}
