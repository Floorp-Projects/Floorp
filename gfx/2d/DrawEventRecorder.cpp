/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawEventRecorder.h"
#include "PathRecording.h"
#include "RecordingTypes.h"

namespace mozilla {
namespace gfx {

using namespace std;

DrawEventRecorderPrivate::DrawEventRecorderPrivate() : mExternalFonts(false)
{
}

void
DrawEventRecorderFile::RecordEvent(const RecordedEvent &aEvent)
{
  WriteElement(mOutputStream, aEvent.mType);

  aEvent.RecordToStream(mOutputStream);

  Flush();
}

void
DrawEventRecorderMemory::RecordEvent(const RecordedEvent &aEvent)
{
  WriteElement(mOutputStream, aEvent.mType);

  aEvent.RecordToStream(mOutputStream);
}

DrawEventRecorderFile::DrawEventRecorderFile(const char_type* aFilename)
  : mOutputStream(aFilename, ofstream::binary)
{
  WriteHeader(mOutputStream);
}

DrawEventRecorderFile::~DrawEventRecorderFile()
{
  mOutputStream.close();
}

void
DrawEventRecorderFile::Flush()
{
  mOutputStream.flush();
}

bool
DrawEventRecorderFile::IsOpen()
{
  return mOutputStream.is_open();
}

void
DrawEventRecorderFile::OpenNew(const char_type* aFilename)
{
  MOZ_ASSERT(!mOutputStream.is_open());

  mOutputStream.open(aFilename, ofstream::binary);
  WriteHeader(mOutputStream);
}

void
DrawEventRecorderFile::Close()
{
  MOZ_ASSERT(mOutputStream.is_open());

  mOutputStream.close();
}

DrawEventRecorderMemory::DrawEventRecorderMemory()
{
  WriteHeader(mOutputStream);
}

DrawEventRecorderMemory::DrawEventRecorderMemory(const SerializeResourcesFn &aFn) :
  mSerializeCallback(aFn)
{
  mExternalFonts = true;
  WriteHeader(mOutputStream);
}


void
DrawEventRecorderMemory::Flush()
{
}

void
DrawEventRecorderMemory::FlushItem(IntRect aRect)
{
  MOZ_RELEASE_ASSERT(!aRect.IsEmpty());
  // Detatching our existing resources will add some
  // destruction events to our stream so we need to do that
  // first.
  DetatchResources();

  WriteElement(mIndex, mOutputStream.mLength);

  // write out the fonts into the extra data section
  mSerializeCallback(mOutputStream, mUnscaledFonts);
  WriteElement(mIndex, mOutputStream.mLength);

  WriteElement(mIndex, aRect.x);
  WriteElement(mIndex, aRect.y);
  WriteElement(mIndex, aRect.XMost());
  WriteElement(mIndex, aRect.YMost());
  ClearResources();
  WriteHeader(mOutputStream);
}

bool
DrawEventRecorderMemory::Finish()
{
  // this length might be 0, and things should still work.
  // for example if there are no items in a particular area
  size_t indexOffset = mOutputStream.mLength;
  // write out the index
  mOutputStream.write(mIndex.mData, mIndex.mLength);
  bool hasItems = mIndex.mLength != 0;
  mIndex = MemStream();
  // write out the offset of the Index to the end of the output stream
  WriteElement(mOutputStream, indexOffset);
  ClearResources();
  return hasItems;
}


size_t
DrawEventRecorderMemory::RecordingSize()
{
  return mOutputStream.mLength;
}

void
DrawEventRecorderMemory::WipeRecording()
{
  mOutputStream = MemStream();
  mIndex = MemStream();

  WriteHeader(mOutputStream);
}

} // namespace gfx
} // namespace mozilla
