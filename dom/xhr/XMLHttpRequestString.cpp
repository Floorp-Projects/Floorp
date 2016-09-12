/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequestString.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

class XMLHttpRequestStringBuffer final
{
  friend class XMLHttpRequestStringWriterHelper;

public:
  NS_INLINE_DECL_REFCOUNTING(XMLHttpRequestStringBuffer)

  uint32_t
  Length()
  {
    return mData.Length();
  }

  void
  Append(const nsAString& aString)
  {
    mData.Append(aString);
  }

  void
  GetAsString(nsAString& aString)
  {
    aString = mData;
  }

  size_t
  SizeOfThis(MallocSizeOf aMallocSizeOf) const
  {
    return mData.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

private:
  ~XMLHttpRequestStringBuffer()
  {}

  nsString& Data()
  {
    return mData;
  }

  nsString mData;
};

// ---------------------------------------------------------------------------
// XMLHttpRequestString

XMLHttpRequestString::XMLHttpRequestString()
  : mBuffer(new XMLHttpRequestStringBuffer())
{
}

XMLHttpRequestString::~XMLHttpRequestString()
{
}

void
XMLHttpRequestString::Truncate()
{
  mBuffer = new XMLHttpRequestStringBuffer();
}

uint32_t
XMLHttpRequestString::Length() const
{
  return mBuffer->Length();
}

void
XMLHttpRequestString::Append(const nsAString& aString)
{
  mBuffer->Append(aString);
}

void
XMLHttpRequestString::GetAsString(nsAString& aString) const
{
  return mBuffer->GetAsString(aString);
}

size_t
XMLHttpRequestString::SizeOfThis(MallocSizeOf aMallocSizeOf) const
{
  return mBuffer->SizeOfThis(aMallocSizeOf);
}

bool
XMLHttpRequestString::IsEmpty() const
{
  return !mBuffer->Length();
}

// ---------------------------------------------------------------------------
// XMLHttpRequestStringWriterHelper

XMLHttpRequestStringWriterHelper::XMLHttpRequestStringWriterHelper(XMLHttpRequestString& aString)
  : mBuffer(aString.mBuffer)
{
}

bool
XMLHttpRequestStringWriterHelper::AddCapacity(int32_t aCapacity)
{
  return mBuffer->Data().SetCapacity(mBuffer->Length() + aCapacity, fallible);
}

char16_t*
XMLHttpRequestStringWriterHelper::EndOfExistingData()
{
  return mBuffer->Data().BeginWriting() + mBuffer->Length();
}

void
XMLHttpRequestStringWriterHelper::AddLength(int32_t aLength)
{
  mBuffer->Data().SetLength(mBuffer->Length() + aLength);
}

} // dom namespace
} // mozilla namespace
