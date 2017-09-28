/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidevineUtils.h"
#include "GMPLog.h"
#include "gmp-api/gmp-errors.h"
#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>

namespace mozilla {

WidevineBuffer::WidevineBuffer(size_t aSize)
{
  GMP_LOG("WidevineBuffer(size=%zu) created", aSize);
  mBuffer.SetLength(aSize);
}

WidevineBuffer::~WidevineBuffer()
{
  GMP_LOG("WidevineBuffer(size=%" PRIu32 ") destroyed", Size());
}

void
WidevineBuffer::Destroy()
{
  delete this;
}

uint32_t
WidevineBuffer::Capacity() const
{
  return mBuffer.Length();
}

uint8_t*
WidevineBuffer::Data()
{
  return mBuffer.Elements();
}

void
WidevineBuffer::SetSize(uint32_t aSize)
{
  mBuffer.SetLength(aSize);
}

uint32_t
WidevineBuffer::Size() const
{
  return mBuffer.Length();
}

nsTArray<uint8_t>
WidevineBuffer::ExtractBuffer() {
  nsTArray<uint8_t> out;
  out.SwapElements(mBuffer);
  return out;
}

WidevineDecryptedBlock::WidevineDecryptedBlock()
  : mBuffer(nullptr)
  , mTimestamp(0)
{
}

WidevineDecryptedBlock::~WidevineDecryptedBlock()
{
  if (mBuffer) {
    mBuffer->Destroy();
    mBuffer = nullptr;
  }
}

void
WidevineDecryptedBlock::SetDecryptedBuffer(cdm::Buffer* aBuffer)
{
  mBuffer = aBuffer;
}

cdm::Buffer*
WidevineDecryptedBlock::DecryptedBuffer()
{
  return mBuffer;
}

void
WidevineDecryptedBlock::SetTimestamp(int64_t aTimestamp)
{
  mTimestamp = aTimestamp;
}

int64_t
WidevineDecryptedBlock::Timestamp() const
{
  return mTimestamp;
}

} // namespace mozilla
