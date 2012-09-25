/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawEventRecorder.h"
#include "PathRecording.h"

namespace mozilla {
namespace gfx {

using namespace std;

const uint32_t kMagicInt = 0xc001feed;

DrawEventRecorderPrivate::DrawEventRecorderPrivate(std::ostream *aStream)
  : mOutputStream(aStream)
{
}

void
DrawEventRecorderPrivate::RecordEvent(const RecordedEvent &aEvent)
{
  WriteElement(*mOutputStream, aEvent.mType);

  aEvent.RecordToStream(*mOutputStream);

  Flush();
}

DrawEventRecorderFile::DrawEventRecorderFile(const char *aFilename)
  : DrawEventRecorderPrivate(NULL) 
  , mOutputFile(aFilename, ofstream::binary)
{
  mOutputStream = &mOutputFile;

  WriteElement(*mOutputStream, kMagicInt);
  WriteElement(*mOutputStream, kMajorRevision);
  WriteElement(*mOutputStream, kMinorRevision);
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

}
}
