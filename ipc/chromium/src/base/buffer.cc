/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "buffer.h"
#include "nsDebug.h"

Buffer::Buffer()
 : mBuffer(nullptr),
   mSize(0),
   mReserved(0)
{
}

Buffer::~Buffer()
{
  if (mBuffer) {
    free(mBuffer);
  }
}

bool
Buffer::empty() const
{
  return mSize == 0;
}

size_t
Buffer::size() const
{
  return mSize;
}

const char*
Buffer::data() const
{
  return mBuffer;
}

void
Buffer::clear()
{
  free(mBuffer);
  mBuffer = nullptr;
  mSize = 0;
  mReserved = 0;
}

void
Buffer::try_realloc(size_t newlength)
{
  char* buffer = (char*)realloc(mBuffer, newlength);
  if (buffer || !newlength) {
    mBuffer = buffer;
    mReserved = newlength;
    return;
  }

  // If we're growing the buffer, crash. If we're shrinking, then we continue to
  // use the old (larger) buffer.
  if (newlength > mReserved) {
    NS_ABORT_OOM(newlength);
  }
}

void
Buffer::append(const char* bytes, size_t length)
{
  if (mSize + length > mReserved) {
    try_realloc(mSize + length);
  }

  memcpy(mBuffer + mSize, bytes, length);
  mSize += length;
}

void
Buffer::assign(const char* bytes, size_t length)
{
  if (bytes >= mBuffer && bytes < mBuffer + mReserved) {
    MOZ_RELEASE_ASSERT(bytes + length <= mBuffer + mSize);
    memmove(mBuffer, bytes, length);
    mSize = length;
    try_realloc(length);
  } else {
    try_realloc(length);
    mSize = length;
    memcpy(mBuffer, bytes, length);
  }
}

void
Buffer::erase(size_t start, size_t count)
{
  mSize -= count;
  memmove(mBuffer + start, mBuffer + start + count, mSize - start);
  try_realloc(mSize);
}

void
Buffer::reserve(size_t size)
{
  if (mReserved < size) {
    try_realloc(size);
  }
}

char*
Buffer::trade_bytes(size_t count)
{
  MOZ_RELEASE_ASSERT(count);

  char* result = mBuffer;
  mSize = mReserved = mSize - count;
  mBuffer = mReserved ? (char*)malloc(mReserved) : nullptr;
  MOZ_RELEASE_ASSERT(!mReserved || mBuffer);
  if (mSize) {
    memcpy(mBuffer, result + count, mSize);
  }

  // Try to resize the buffer down, but ignore failure. This can cause extra
  // copies, but so be it.
  char* resized = (char*)realloc(result, count);
  if (resized) {
    return resized;
  }
  return result;
}
