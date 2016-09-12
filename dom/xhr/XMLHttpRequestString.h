/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLHttpRequestString_h
#define mozilla_dom_XMLHttpRequestString_h

#include "nsString.h"

namespace mozilla {
namespace dom {

class XMLHttpRequestStringBuffer;
class XMLHttpRequestStringWriterHelper;

class XMLHttpRequestString final
{
  friend class XMLHttpRequestStringWriterHelper;

public:
  XMLHttpRequestString();
  ~XMLHttpRequestString();

  void Truncate();

  uint32_t Length() const;

  void Append(const nsAString& aString);

  void GetAsString(nsAString& aString) const;

  size_t SizeOfThis(MallocSizeOf aMallocSizeOf) const;

  const char16_t* Data() const;

  bool IsEmpty() const;

private:
  RefPtr<XMLHttpRequestStringBuffer> mBuffer;
};

class XMLHttpRequestStringWriterHelper final
{
public:
  explicit XMLHttpRequestStringWriterHelper(XMLHttpRequestString& aString);

  bool
  AddCapacity(int32_t aCapacity);

  char16_t*
  EndOfExistingData();

  void
  AddLength(int32_t aLength);

private:
  RefPtr<XMLHttpRequestStringBuffer> mBuffer;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_XMLHttpRequestString_h
