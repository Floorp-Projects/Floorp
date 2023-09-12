/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RECORDINGTYPES_H_
#define MOZILLA_GFX_RECORDINGTYPES_H_

#include <ostream>
#include <vector>

#include "Logging.h"

namespace mozilla {
namespace gfx {

template <class S, class T>
struct ElementStreamFormat {
  static void Write(S& aStream, const T& aElement) {
    aStream.write(reinterpret_cast<const char*>(&aElement), sizeof(T));
  }
  static void Read(S& aStream, T& aElement) {
    aStream.read(reinterpret_cast<char*>(&aElement), sizeof(T));
  }
};
template <class S>
struct ElementStreamFormat<S, bool> {
  static void Write(S& aStream, const bool& aElement) {
    char boolChar = aElement ? '\x01' : '\x00';
    aStream.write(&boolChar, sizeof(boolChar));
  }
  static void Read(S& aStream, bool& aElement) {
    char boolChar;
    aStream.read(&boolChar, sizeof(boolChar));
    switch (boolChar) {
      case '\x00':
        aElement = false;
        break;
      case '\x01':
        aElement = true;
        break;
      default:
        aStream.SetIsBad();
        break;
    }
  }
};

template <class S, class T>
void WriteElement(S& aStream, const T& aElement) {
  ElementStreamFormat<S, T>::Write(aStream, aElement);
}
template <class S, class T>
void WriteVector(S& aStream, const std::vector<T>& aVector) {
  size_t size = aVector.size();
  WriteElement(aStream, size);
  if (size) {
    aStream.write(reinterpret_cast<const char*>(aVector.data()),
                  sizeof(T) * size);
  }
}

// ReadElement is disabled for enum types. Use ReadElementConstrained instead.
template <class S, class T,
          typename = typename std::enable_if<!std::is_enum<T>::value>::type>
void ReadElement(S& aStream, T& aElement) {
  ElementStreamFormat<S, T>::Read(aStream, aElement);
}
template <class S, class T>
void ReadElementConstrained(S& aStream, T& aElement, const T& aMinValue,
                            const T& aMaxValue) {
  ElementStreamFormat<S, T>::Read(aStream, aElement);
  if (aElement < aMinValue || aElement > aMaxValue) {
    aStream.SetIsBad();
  }
}
template <class S, class T>
void ReadVector(S& aStream, std::vector<T>& aVector) {
  size_t size;
  ReadElement(aStream, size);
  if (size && aStream.good()) {
    aVector.resize(size);
    aStream.read(reinterpret_cast<char*>(aVector.data()), sizeof(T) * size);
  } else {
    aVector.clear();
  }
}

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_RECORDINGTYPES_H_ */
