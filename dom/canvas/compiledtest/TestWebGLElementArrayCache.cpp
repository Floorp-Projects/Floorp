/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"

#include "WebGLElementArrayCache.cpp"

#include <cstdlib>
#include <iostream>
#include "nscore.h"
#include "nsTArray.h"

int gTestsPassed = 0;

void
VerifyImplFunction(bool condition, const char* file, int line)
{
  if (condition) {
    gTestsPassed++;
  } else {
    std::cerr << "Test failed at " << file << ":" << line << std::endl;
    abort();
  }
}

#define VERIFY(condition) \
    VerifyImplFunction((condition), __FILE__, __LINE__)

void
MakeRandomVector(nsTArray<uint8_t>& a, size_t size)
{
  a.SetLength(size);
  // only the most-significant bits of rand() are reasonably random.
  // RAND_MAX can be as low as 0x7fff, and we need 8 bits for the result, so we can only
  // ignore the 7 least significant bits.
  for (size_t i = 0; i < size; i++)
    a[i] = static_cast<uint8_t>((unsigned int)(rand()) >> 7);
}

template<typename T>
T
RandomInteger(T a, T b)
{
  T result(a + rand() % (b - a + 1));
  return result;
}

template<typename T>
GLenum
GLType()
{
  switch (sizeof(T)) {
  case 4:  return LOCAL_GL_UNSIGNED_INT;
  case 2:  return LOCAL_GL_UNSIGNED_SHORT;
  case 1:  return LOCAL_GL_UNSIGNED_BYTE;
  default:
    VERIFY(false);
    return 0;
  }
}

void
CheckValidate(bool expectSuccess, mozilla::WebGLElementArrayCache& c, GLenum type,
              uint32_t maxAllowed, size_t first, size_t count)
{
  uint32_t out_upperBound = 0;
  const bool success = c.Validate(type, maxAllowed, first, count, &out_upperBound);
  VERIFY(success == expectSuccess);
  if (success) {
    VERIFY(out_upperBound <= maxAllowed);
  } else {
    VERIFY(out_upperBound > maxAllowed);
  }
}

template<typename T>
void
CheckValidateOneTypeVariousBounds(mozilla::WebGLElementArrayCache& c, size_t firstByte,
                                  size_t countBytes)
{
  size_t first = firstByte / sizeof(T);
  size_t count = countBytes / sizeof(T);

  GLenum type = GLType<T>();

  T max = 0;
  for (size_t i = 0; i < count; i++)
    if (c.Element<T>(first + i) > max)
      max = c.Element<T>(first + i);

  CheckValidate(true, c, type, max, first, count);
  CheckValidate(true, c, type, T(-1), first, count);
  if (T(max + 1)) CheckValidate(true, c, type, T(max + 1), first, count);
  if (max > 0) {
    CheckValidate(false, c, type, max - 1, first, count);
    CheckValidate(false, c, type, 0, first, count);
  }
}

void CheckValidateAllTypes(mozilla::WebGLElementArrayCache& c, size_t firstByte,
                           size_t countBytes)
{
  CheckValidateOneTypeVariousBounds<uint8_t>(c, firstByte, countBytes);
  CheckValidateOneTypeVariousBounds<uint16_t>(c, firstByte, countBytes);
  CheckValidateOneTypeVariousBounds<uint32_t>(c, firstByte, countBytes);
}

template<typename T>
void
CheckSanity()
{
  const size_t numElems = 64; // should be significantly larger than tree leaf size to
                        // ensure we exercise some nontrivial tree-walking
  T data[numElems] = {1,0,3,1,2,6,5,4}; // intentionally specify only 8 elements for now
  size_t numBytes = numElems * sizeof(T);
  MOZ_RELEASE_ASSERT(numBytes == sizeof(data));

  GLenum type = GLType<T>();

  mozilla::WebGLElementArrayCache c;
  c.BufferData(data, numBytes);
  CheckValidate(true,  c, type, 6, 0, 8);
  CheckValidate(false, c, type, 5, 0, 8);
  CheckValidate(true,  c, type, 3, 0, 3);
  CheckValidate(false, c, type, 2, 0, 3);
  CheckValidate(true,  c, type, 6, 2, 4);
  CheckValidate(false, c, type, 5, 2, 4);

  c.BufferSubData(5*sizeof(T), data, sizeof(T));
  CheckValidate(true,  c, type, 5, 0, 8);
  CheckValidate(false, c, type, 4, 0, 8);

  // now test a somewhat larger size to ensure we exceed the size of a tree leaf
  for(size_t i = 0; i < numElems; i++)
    data[i] = numElems - i;
  c.BufferData(data, numBytes);
  CheckValidate(true,  c, type, numElems,     0, numElems);
  CheckValidate(false, c, type, numElems - 1, 0, numElems);

  MOZ_RELEASE_ASSERT(numElems > 10);
  CheckValidate(true,  c, type, numElems - 10, 10, numElems - 10);
  CheckValidate(false, c, type, numElems - 11, 10, numElems - 10);
}

template<typename T>
void
CheckUintOverflow()
{
  // This test is only for integer types smaller than uint32_t
  static_assert(sizeof(T) < sizeof(uint32_t), "This test is only for integer types \
                smaller than uint32_t");

  const size_t numElems = 64; // should be significantly larger than tree leaf size to
                              // ensure we exercise some nontrivial tree-walking
  T data[numElems];
  size_t numBytes = numElems * sizeof(T);
  MOZ_RELEASE_ASSERT(numBytes == sizeof(data));

  GLenum type = GLType<T>();

  mozilla::WebGLElementArrayCache c;

  for(size_t i = 0; i < numElems; i++)
    data[i] = numElems - i;
  c.BufferData(data, numBytes);

  // bug 825205
  uint32_t bigValWrappingToZero = uint32_t(T(-1)) + 1;
  CheckValidate(true,  c, type, bigValWrappingToZero,     0, numElems);
  CheckValidate(true,  c, type, bigValWrappingToZero - 1, 0, numElems);
  CheckValidate(false, c, type,                        0, 0, numElems);
}

int
main(int argc, char* argv[])
{
  srand(0); // do not want a random seed here.

  CheckSanity<uint8_t>();
  CheckSanity<uint16_t>();
  CheckSanity<uint32_t>();

  CheckUintOverflow<uint8_t>();
  CheckUintOverflow<uint16_t>();

  nsTArray<uint8_t> v, vsub;
  mozilla::WebGLElementArrayCache b;

  for (int maxBufferSize = 1; maxBufferSize <= 4096; maxBufferSize *= 2) {
    // See bug 800612. We originally had | repeat = min(maxBufferSize, 20) |
    // and a real bug was only caught on Windows and not on Linux due to rand()
    // producing different values. In that case, the minimum value by which to replace
    // this 20 to reproduce the bug on Linux, was 25. Replacing it with 64 should give
    // us some comfort margin.
    int repeat = std::min(maxBufferSize, 64);
    for (int i = 0; i < repeat; i++) {
      size_t size = RandomInteger<size_t>(0, maxBufferSize);
      MakeRandomVector(v, size);
      b.BufferData(v.Elements(), size);
      CheckValidateAllTypes(b, 0, size);

      for (int j = 0; j < 16; j++) {
        for (int bufferSubDataCalls = 1; bufferSubDataCalls <= 8; bufferSubDataCalls *= 2) {
          for (int validateCalls = 1; validateCalls <= 8; validateCalls *= 2) {

            size_t offset = 0, subsize = 0;

            for (int k = 0; k < bufferSubDataCalls; k++) {
              offset = RandomInteger<size_t>(0, size);
              subsize = RandomInteger<size_t>(0, size - offset);
              MakeRandomVector(vsub, subsize);
              b.BufferSubData(offset, vsub.Elements(), subsize);
            }

            for (int k = 0; k < validateCalls; k++) {
              offset = RandomInteger<size_t>(0, size);
              subsize = RandomInteger<size_t>(0, size - offset);
              CheckValidateAllTypes(b, offset, subsize);
            }
          } // validateCalls
        } // bufferSubDataCalls
      } // j
    } // i
  } // maxBufferSize

  std::cerr << argv[0] << ": all " << gTestsPassed << " tests passed" << std::endl;
  return 0;
}

