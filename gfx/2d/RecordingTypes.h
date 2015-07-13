/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RECORDINGTYPES_H_
#define MOZILLA_GFX_RECORDINGTYPES_H_

#include <ostream>

namespace mozilla {
namespace gfx {

template<class T>
struct ElementStreamFormat
{
  static void Write(std::ostream &aStream, const T &aElement)
  {
    aStream.write(reinterpret_cast<const char*>(&aElement), sizeof(T));
  }
  static void Read(std::istream &aStream, T &aElement)
  {
    aStream.read(reinterpret_cast<char *>(&aElement), sizeof(T));
  }
};

template<class T>
void WriteElement(std::ostream &aStream, const T &aElement)
{
  ElementStreamFormat<T>::Write(aStream, aElement);
}
template<class T>
void ReadElement(std::istream &aStream, T &aElement)
{
  ElementStreamFormat<T>::Read(aStream, aElement);
}

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_RECORDINGTYPES_H_ */
