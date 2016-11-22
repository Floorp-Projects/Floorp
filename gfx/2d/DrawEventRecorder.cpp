/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawEventRecorder.h"
#include "PathRecording.h"
#include "RecordingTypes.h"

namespace mozilla {
namespace gfx {

using namespace std;

DrawEventRecorderPrivate::DrawEventRecorderPrivate(std::ostream *aStream)
  : mOutputStream(aStream)
{
}

void
DrawEventRecorderPrivate::WriteHeader()
{
  WriteElement(*mOutputStream, kMagicInt);
  WriteElement(*mOutputStream, kMajorRevision);
  WriteElement(*mOutputStream, kMinorRevision);
}

void
DrawEventRecorderPrivate::RecordEvent(const RecordedEvent &aEvent)
{
  WriteElement(*mOutputStream, aEvent.mType);

  aEvent.RecordToStream(*mOutputStream);

  Flush();
}

DrawEventRecorderFile::DrawEventRecorderFile(const char *aFilename)
  : DrawEventRecorderPrivate(nullptr)
  , mOutputFilename(aFilename)
  , mOutputFile(aFilename, ofstream::binary)
{
  mOutputStream = &mOutputFile;

  WriteHeader();
}

DrawEventRecorderFile::~DrawEventRecorderFile()
{
  mOutputFile.close();
}

void
DrawEventRecorderFile::Flush()
{
  mOutputFile.flush();
}

void
DrawEventRecorderFile::OpenAndTruncate()
{
  if (mOutputFile.is_open()) {
    return;
  }

  mOutputFile.open(mOutputFilename.c_str(), ofstream::binary | ofstream::trunc);
  WriteHeader();
}

void
DrawEventRecorderFile::Close()
{
  MOZ_ASSERT(mOutputFile.is_open());

  mOutputFile.close();
}

DrawEventRecorderMemory::DrawEventRecorderMemory()
  : DrawEventRecorderPrivate(nullptr)
{
  mOutputStream = &mMemoryStream;

  WriteHeader();
}

void
DrawEventRecorderMemory::Flush()
{
   mOutputStream->flush();
}

size_t
DrawEventRecorderMemory::RecordingSize()
{
  return mMemoryStream.tellp();
}

bool
DrawEventRecorderMemory::CopyRecording(char* aBuffer, size_t aBufferLen)
{
  return !!mMemoryStream.read(aBuffer, aBufferLen);
}

void
DrawEventRecorderMemory::WipeRecording()
{
  mMemoryStream.str(std::string());
  mMemoryStream.clear();

  WriteHeader();
}

} // namespace gfx
} // namespace mozilla
