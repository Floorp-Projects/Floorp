/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"

#include "WebGLElementArrayCache.cpp"

#include <cstdlib>
#include <iostream>
#include "nscore.h"
#include "nsTArray.h"

using namespace mozilla;

int gTestsPassed = 0;

void VerifyImplFunction(bool condition, const char* file, int line)
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

void MakeRandomVector(nsTArray<uint8_t>& a, size_t size) {
  a.SetLength(size);
  // only the most-significant bits of rand() are reasonably random.
  // RAND_MAX can be as low as 0x7fff, and we need 8 bits for the result, so we can only
  // ignore the 7 least significant bits.
  for (size_t i = 0; i < size; i++)
    a[i] = static_cast<uint8_t>((unsigned int)(rand()) >> 7);
}

template<typename T>
T RandomInteger(T a, T b)
{
  T result(a + rand() % (b - a + 1));
  return result;
}

template<typename T>
GLenum GLType()
{
  switch (sizeof(T))
  {
    case 4:  return LOCAL_GL_UNSIGNED_INT;
    case 2:  return LOCAL_GL_UNSIGNED_SHORT;
    case 1:  return LOCAL_GL_UNSIGNED_BYTE;
    default:
      VERIFY(false);
      return 0;
  }
}

template<typename T>
void CheckValidateOneType(WebGLElementArrayCache& c, size_t firstByte, size_t countBytes)
{
  size_t first = firstByte / sizeof(T);
  size_t count = countBytes / sizeof(T);

  GLenum type = GLType<T>();

  T max = 0;
  for (size_t i = 0; i < count; i++)
    if (c.Element<T>(first + i) > max)
      max = c.Element<T>(first + i);

  VERIFY(c.Validate(type, max, first, count));
  VERIFY(c.Validate(type, T(-1), first, count));
  if (T(max + 1)) VERIFY(c.Validate(type, T(max + 1), first, count));
  if (max > 0) {
    VERIFY(!c.Validate(type, max - 1, first, count));
    VERIFY(!c.Validate(type, 0, first, count));
  }
}

void CheckValidate(WebGLElementArrayCache& c, size_t firstByte, size_t countBytes)
{
  CheckValidateOneType<uint8_t>(c, firstByte, countBytes);
  CheckValidateOneType<uint16_t>(c, firstByte, countBytes);
  CheckValidateOneType<uint32_t>(c, firstByte, countBytes);
}

template<typename T>
void CheckSanity()
{
  const size_t numElems = 64; // should be significantly larger than tree leaf size to
                        // ensure we exercise some nontrivial tree-walking
  T data[numElems] = {1,0,3,1,2,6,5,4}; // intentionally specify only 8 elements for now
  size_t numBytes = numElems * sizeof(T);
  MOZ_ASSERT(numBytes == sizeof(data));

  GLenum type = GLType<T>();

  WebGLElementArrayCache c;
  c.BufferData(data, numBytes);
  VERIFY( c.Validate(type, 6, 0, 8));
  VERIFY(!c.Validate(type, 5, 0, 8));
  VERIFY( c.Validate(type, 3, 0, 3));
  VERIFY(!c.Validate(type, 2, 0, 3));
  VERIFY( c.Validate(type, 6, 2, 4));
  VERIFY(!c.Validate(type, 5, 2, 4));

  c.BufferSubData(5*sizeof(T), data, sizeof(T));
  VERIFY( c.Validate(type, 5, 0, 8));
  VERIFY(!c.Validate(type, 4, 0, 8));

  // now test a somewhat larger size to ensure we exceed the size of a tree leaf
  for(size_t i = 0; i < numElems; i++)
    data[i] = numElems - i;
  c.BufferData(data, numBytes);
  VERIFY( c.Validate(type, numElems,     0, numElems));
  VERIFY(!c.Validate(type, numElems - 1, 0, numElems));

  MOZ_ASSERT(numElems > 10);
  VERIFY( c.Validate(type, numElems - 10, 10, numElems - 10));
  VERIFY(!c.Validate(type, numElems - 11, 10, numElems - 10));
}

template<typename T>
void CheckUintOverflow()
{
  // This test is only for integer types smaller than uint32_t
  static_assert(sizeof(T) < sizeof(uint32_t), "This test is only for integer types \
                smaller than uint32_t");

  const size_t numElems = 64; // should be significantly larger than tree leaf size to
                              // ensure we exercise some nontrivial tree-walking
  T data[numElems];
  size_t numBytes = numElems * sizeof(T);
  MOZ_ASSERT(numBytes == sizeof(data));

  GLenum type = GLType<T>();

  WebGLElementArrayCache c;

  for(size_t i = 0; i < numElems; i++)
    data[i] = numElems - i;
  c.BufferData(data, numBytes);

  // bug 825205
  uint32_t bigValWrappingToZero = uint32_t(T(-1)) + 1;
  VERIFY( c.Validate(type, bigValWrappingToZero,     0, numElems));
  VERIFY( c.Validate(type, bigValWrappingToZero - 1, 0, numElems));
  VERIFY(!c.Validate(type,                        0, 0, numElems));
}

int main(int argc, char *argv[])
{
  srand(0); // do not want a random seed here.

  CheckSanity<uint8_t>();
  CheckSanity<uint16_t>();
  CheckSanity<uint32_t>();

  CheckUintOverflow<uint8_t>();
  CheckUintOverflow<uint16_t>();

  nsTArray<uint8_t> v, vsub;
  WebGLElementArrayCache b;

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
      CheckValidate(b, 0, size);

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
              CheckValidate(b, offset, subsize);
            }
          } // validateCalls
        } // bufferSubDataCalls
      } // j
    } // i
  } // maxBufferSize

  std::cerr << argv[0] << ": all " << gTestsPassed << " tests passed" << std::endl;
  return 0;
}
