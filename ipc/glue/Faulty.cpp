/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/ipc/Faulty.h"
#include <cerrno>
#include <prinrval.h>
#include "nsXULAppAPI.h"
#include "base/string_util.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_channel.h"
#include "prenv.h"
#include "mozilla/TypeTraits.h"
#include <cmath>
#include <climits>

namespace mozilla {
namespace ipc {

const unsigned int Faulty::sDefaultProbability = Faulty::DefaultProbability();
const bool Faulty::sIsLoggingEnabled = Faulty::Logging();

/**
 * RandomNumericValue generates negative and positive integrals.
 */
template <typename T>
T RandomIntegral()
{
  static_assert(mozilla::IsIntegral<T>::value == true,
                "T must be an integral type");
  double r = static_cast<double>(random() % ((sizeof(T) * CHAR_BIT) + 1));
  T x = static_cast<T>(pow(2.0, r)) - 1;
  if (std::numeric_limits<T>::is_signed && random() % 2 == 0) {
    return (x * -1) - 1;
  }
  return x;
}

/**
 * RandomNumericLimit returns either the min or max limit of an arithmetic
 * data type.
 */
template <typename T>
T RandomNumericLimit() {
  static_assert(mozilla::IsArithmetic<T>::value == true,
                "T must be an arithmetic type");
  return random() % 2 == 0 ? std::numeric_limits<T>::min()
                           : std::numeric_limits<T>::max();
}

/**
 * RandomIntegerRange returns a random integral within a user defined range.
 */
template <typename T>
T RandomIntegerRange(T min, T max)
{
  static_assert(mozilla::IsIntegral<T>::value == true,
                "T must be an integral type");
  MOZ_ASSERT(min < max);
  return static_cast<T>(random() % (max - min) + min);
}

/**
 * RandomFloatingPointRange returns a random floating-point number within a
 * user defined range.
 */
template <typename T>
T RandomFloatingPointRange(T min, T max)
{
  static_assert(mozilla::IsFloatingPoint<T>::value == true,
                "T must be a floating point type");
  MOZ_ASSERT(min < max);
  T x = static_cast<T>(random()) / static_cast<T>(RAND_MAX);
  return min + x * (max - min);
}

/**
 * RandomFloatingPoint returns a random floating-point number.
 */
template <typename T>
T RandomFloatingPoint()
{
  static_assert(mozilla::IsFloatingPoint<T>::value == true,
                "T must be a floating point type");
  int radix = RandomIntegerRange<int>(std::numeric_limits<T>::min_exponent,
                                      std::numeric_limits<T>::max_exponent);
  T x = static_cast<T>(pow(2.0, static_cast<double>(radix)));
  return x * RandomFloatingPointRange<T>(1.0, 2.0);
}

/**
 * FuzzIntegralType mutates an incercepted integral type of a pickled message.
 */
template <typename T>
void FuzzIntegralType(T* v, bool largeValues)
{
  static_assert(mozilla::IsIntegral<T>::value == true,
                "T must be an integral type");
  switch (random() % 6) {
    case 0:
      if (largeValues) {
        (*v) = RandomIntegral<T>();
        break;
      }
      // Fall through
    case 1:
      if (largeValues) {
        (*v) = RandomNumericLimit<T>();
        break;
      }
      // Fall through
    case 2:
      if (largeValues) {
        (*v) = RandomIntegerRange<T>(std::numeric_limits<T>::min(),
                                     std::numeric_limits<T>::max());
        break;
      }
      // Fall through
    default:
      switch(random() % 2) {
        case 0:
          // Prevent underflow
          if (*v != std::numeric_limits<T>::min()) {
            (*v)--;
            break;
          }
          // Fall through
        case 1:
          // Prevent overflow
          if (*v != std::numeric_limits<T>::max()) {
            (*v)++;
            break;
          }
      }
  }
}

/**
 * FuzzFloatingPointType mutates an incercepted floating-point type of a
 * pickled message.
 */
template <typename T>
void FuzzFloatingPointType(T* v, bool largeValues)
{
  static_assert(mozilla::IsFloatingPoint<T>::value == true,
                "T must be a floating point type");
  switch (random() % 6) {
    case 0:
      if (largeValues) {
        (*v) = RandomNumericLimit<T>();
        break;
    }
    // Fall through
    case 1:
      if (largeValues) {
        (*v) = RandomFloatingPointRange<T>(std::numeric_limits<T>::min(),
                                           std::numeric_limits<T>::max());
        break;
      }
    // Fall through
    default:
      (*v) = RandomFloatingPoint<T>();
  }
}

/**
 * FuzzStringType mutates an incercepted string type of a pickled message.
 */
template <typename T>
void FuzzStringType(T& v, const T& literal1, const T& literal2)
{
  switch (random() % 5) {
    case 4:
      v = v + v;
      // Fall through
    case 3:
      v = v + v;
      // Fall through
    case 2:
      v = v + v;
      break;
    case 1:
      v += literal1;
      break;
    case 0:
      v = literal2;
      break;
  }
}


Faulty::Faulty()
  // Enables the strategy for fuzzing pipes.
  : mFuzzPipes(!!PR_GetEnv("FAULTY_PIPE"))
  // Enables the strategy for fuzzing pickled messages.
  , mFuzzPickle(!!PR_GetEnv("FAULTY_PICKLE"))
  // Uses very large values while fuzzing pickled messages.
  // This may cause a high amount of malloc_abort() / NS_ABORT_OOM crashes.
  , mUseLargeValues(!!PR_GetEnv("FAULTY_LARGE_VALUES"))
  // Sets up our target process.
  , mIsValidProcessType(IsValidProcessType())
{
  FAULTY_LOG("Initializing.");

  const char* userSeed = PR_GetEnv("FAULTY_SEED");
  unsigned long randomSeed = static_cast<unsigned long>(PR_IntervalNow());
  if (userSeed) {
    long n = std::strtol(userSeed, nullptr, 10);
    if (n != 0) {
      randomSeed = static_cast<unsigned long>(n);
    }
  }
  srandom(randomSeed);

  FAULTY_LOG("Fuzz probability = %u", sDefaultProbability);
  FAULTY_LOG("Random seed      = %lu", randomSeed);
  FAULTY_LOG("Strategy: pickle = %s", mFuzzPickle ? "enabled" : "disabled");
  FAULTY_LOG("Strategy: pipe   = %s", mFuzzPipes ? "enabled" : "disabled");
}

// static
bool
Faulty::IsValidProcessType(void)
{
  bool isValidProcessType;
  const bool targetChildren = !!PR_GetEnv("FAULTY_CHILDREN");
  const bool targetParent = !!PR_GetEnv("FAULTY_PARENT");

  if (targetChildren && !targetParent) {
    // Fuzz every process type but not the content process.
    isValidProcessType = XRE_GetProcessType() != GeckoProcessType_Content;
  } else if (targetChildren && targetParent) {
    // Fuzz every process type.
    isValidProcessType = true;
  } else {
    // Fuzz the content process only.
    isValidProcessType = XRE_GetProcessType() == GeckoProcessType_Content;
  }

  // Parent and children are different threads in the same process on
  // desktop builds.
  if (!isValidProcessType) {
    FAULTY_LOG("Invalid process type for pid=%d", getpid());
  }

  return isValidProcessType;
}

// static
unsigned int
Faulty::DefaultProbability(void)
{
  // Defines the likelihood of fuzzing a message.
  const char* probability = PR_GetEnv("FAULTY_PROBABILITY");
  if (probability) {
    long n = std::strtol(probability, nullptr, 10);
    if (n != 0) {
      return n;
    }
  }
  return FAULTY_DEFAULT_PROBABILITY;
}

// static
bool
Faulty::Logging(void)
{
  // Enables logging of sendmsg() calls even in optimized builds.
  return !!PR_GetEnv("FAULTY_ENABLE_LOGGING");
}

unsigned int
Faulty::Random(unsigned int aMax)
{
  MOZ_ASSERT(aMax > 0);
  return static_cast<unsigned int>(random() % aMax);
}

bool
Faulty::GetChance(unsigned int aProbability)
{
  return Random(aProbability) == 0;
}

//
// Strategy: Pipes
//

void
Faulty::MaybeCollectAndClosePipe(int aPipe, unsigned int aProbability)
{
  if (!mFuzzPipes) {
    return;
  }

  if (aPipe > -1) {
    FAULTY_LOG("collecting pipe %d to bucket of pipes (count: %ld)",
               aPipe, mFds.size());
    mFds.insert(aPipe);
  }

  if (mFds.size() > 0 && GetChance(aProbability)) {
    std::set<int>::iterator it(mFds.begin());
    std::advance(it, Random(mFds.size()));
    FAULTY_LOG("trying to close collected pipe: %d", *it);
    errno = 0;
    while ((close(*it) == -1 && (errno == EINTR))) {
      ;
    }
    FAULTY_LOG("pipe status after attempt to close: %d", errno);
    mFds.erase(it);
  }
}

//
// Strategy: Pickle
//

void
Faulty::MutateBool(bool* aValue)
{
  *aValue = !(*aValue);
}

void
Faulty::FuzzBool(bool* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      bool oldValue = *aValue;
      MutateBool(aValue);
      FAULTY_LOG("pickle field {bool} of value: %d changed to: %d",
                 (int)oldValue, (int)*aValue);
    }
  }
}

void
Faulty::MutateChar(char* aValue)
{
  FuzzIntegralType<char>(aValue, true);
}

void
Faulty::FuzzChar(char* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      char oldValue = *aValue;
      MutateChar(aValue);
      FAULTY_LOG("pickle field {char} of value: %c changed to: %c",
                 oldValue, *aValue);
    }
  }
}

void
Faulty::MutateUChar(unsigned char* aValue)
{
  FuzzIntegralType<unsigned char>(aValue, true);
}

void
Faulty::FuzzUChar(unsigned char* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      unsigned char oldValue = *aValue;
      MutateUChar(aValue);
      FAULTY_LOG("pickle field {unsigned char} of value: %u changed to: %u",
                 oldValue, *aValue);
    }
  }
}

void
Faulty::MutateInt16(int16_t* aValue)
{
  FuzzIntegralType<int16_t>(aValue, true);
}

void
Faulty::FuzzInt16(int16_t* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      int16_t oldValue = *aValue;
      MutateInt16(aValue);
      FAULTY_LOG("pickle field {Int16} of value: %d changed to: %d",
                 oldValue, *aValue);
    }
  }
}

void
Faulty::MutateUInt16(uint16_t* aValue)
{
  FuzzIntegralType<uint16_t>(aValue, true);
}

void
Faulty::FuzzUInt16(uint16_t* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      uint16_t oldValue = *aValue;
      MutateUInt16(aValue);
      FAULTY_LOG("pickle field {UInt16} of value: %d changed to: %d",
                 oldValue, *aValue);
    }
  }
}

void
Faulty::MutateInt(int* aValue)
{
  FuzzIntegralType<int>(aValue, mUseLargeValues);
}

void
Faulty::FuzzInt(int* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      int oldValue = *aValue;
      MutateInt(aValue);
      FAULTY_LOG("pickle field {int} of value: %d changed to: %d",
                 oldValue, *aValue);
    }
  }
}

void
Faulty::MutateUInt32(uint32_t* aValue)
{
  FuzzIntegralType<uint32_t>(aValue, mUseLargeValues);
}

void
Faulty::FuzzUInt32(uint32_t* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      uint32_t oldValue = *aValue;
      MutateUInt32(aValue);
      FAULTY_LOG("pickle field {UInt32} of value: %u changed to: %u",
                 oldValue, *aValue);
    }
  }
}

void
Faulty::MutateLong(long* aValue)
{
  FuzzIntegralType<long>(aValue, mUseLargeValues);
}

void
Faulty::FuzzLong(long* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      long oldValue = *aValue;
      MutateLong(aValue);
      FAULTY_LOG("pickle field {long} of value: %ld changed to: %ld",
                 oldValue, *aValue);
    }
  }
}

void
Faulty::MutateULong(unsigned long* aValue)
{
  FuzzIntegralType<unsigned long>(aValue, mUseLargeValues);
}

void
Faulty::FuzzULong(unsigned long* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      unsigned long oldValue = *aValue;
      MutateULong(aValue);
      FAULTY_LOG("pickle field {unsigned long} of value: %lu changed to: %lu",
                 oldValue, *aValue);
    }
  }
}

void
Faulty::MutateSize(size_t* aValue)
{
  FuzzIntegralType<size_t>(aValue, mUseLargeValues);
}

void
Faulty::FuzzSize(size_t* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      size_t oldValue = *aValue;
      MutateSize(aValue);
      FAULTY_LOG("pickle field {size_t} of value: %zu changed to: %zu",
                 oldValue, *aValue);
    }
  }
}

void
Faulty::MutateUInt64(uint64_t* aValue)
{
  FuzzIntegralType<uint64_t>(aValue, mUseLargeValues);
}

void
Faulty::FuzzUInt64(uint64_t* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      uint64_t oldValue = *aValue;
      MutateUInt64(aValue);
      FAULTY_LOG("pickle field {UInt64} of value: %llu changed to: %llu",
                 oldValue, *aValue);
    }
  }
}

void
Faulty::MutateInt64(int64_t* aValue)
{
  FuzzIntegralType<int64_t>(aValue, mUseLargeValues);
}

void
Faulty::FuzzInt64(int64_t* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      int64_t oldValue = *aValue;
      MutateInt64(aValue);
      FAULTY_LOG("pickle field {Int64} of value: %lld changed to: %lld",
                 oldValue, *aValue);
    }
  }
}

void
Faulty::MutateDouble(double* aValue)
{
  FuzzFloatingPointType<double>(aValue, mUseLargeValues);
}

void
Faulty::FuzzDouble(double* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      double oldValue = *aValue;
      MutateDouble(aValue);
      FAULTY_LOG("pickle field {double} of value: %f changed to: %f",
                 oldValue, *aValue);
    }
  }
}

void
Faulty::MutateFloat(float* aValue)
{
  FuzzFloatingPointType<float>(aValue, mUseLargeValues);
}

void
Faulty::FuzzFloat(float* aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      float oldValue = *aValue;
      MutateFloat(aValue);
      FAULTY_LOG("pickle field {float} of value: %f changed to: %f",
                 oldValue, *aValue);
    }
  }
}

void
Faulty::FuzzString(std::string& aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      std::string oldValue = aValue;
      FuzzStringType<std::string>(aValue, "xoferiF", std::string());
      FAULTY_LOG("pickle field {string} of value: %s changed to: %s",
                 oldValue.c_str(), aValue.c_str());
    }
  }
}

void
Faulty::FuzzWString(std::wstring& aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      std::wstring oldValue = aValue;
      FAULTY_LOG("pickle field {wstring}");
      FuzzStringType<std::wstring>(aValue, L"xoferiF", std::wstring());
    }
  }
}

void
Faulty::FuzzString16(string16& aValue, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      string16 oldValue = aValue;
      FAULTY_LOG("pickle field {string16}");
      FuzzStringType<string16>(aValue,
        string16(ASCIIToUTF16(std::string("xoferiF"))),
        string16(ASCIIToUTF16(std::string())));
    }
  }
}

void
Faulty::FuzzBytes(void* aData, int aLength, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      FAULTY_LOG("pickle field {bytes}");
      // Too destructive. |WriteBytes| is used in many of the above data
      // types as base function.
      //FuzzData(static_cast<char*>(aData), aLength);
    }
  }
}

void
Faulty::FuzzData(std::string& aValue, int aLength, unsigned int aProbability)
{
  if (mIsValidProcessType) {
    if (mFuzzPickle && GetChance(aProbability)) {
      FAULTY_LOG("pickle field {data}");
      for (int i = 0; i < aLength; ++i) {
        if (GetChance(aProbability)) {
          FuzzIntegralType<char>(&aValue[i], true);
        }
      }
    }
  }
}

} // namespace ipc
} // namespace mozilla
