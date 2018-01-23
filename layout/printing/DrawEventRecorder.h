/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_printing_DrawEventRecorder_h
#define mozilla_layout_printing_DrawEventRecorder_h

#include <memory>

#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/gfx/RecordingTypes.h"
#include "prio.h"

namespace mozilla {
namespace layout {

class PRFileDescStream : public mozilla::gfx::EventStream {
  // Most writes, as seen in the print IPC use case, are very small (<32 bytes),
  // with a small number of very large (>40KB) writes. Writes larger than this
  // value are not buffered.
  static const size_t kBufferSize = 1024;
public:
  PRFileDescStream() : mFd(nullptr), mBuffer(nullptr), mBufferPos(0),
                       mGood(true) {}
  PRFileDescStream(const PRFileDescStream& other) = delete;
  ~PRFileDescStream() { Close(); }

  void OpenFD(PRFileDesc* aFd)
  {
    MOZ_DIAGNOSTIC_ASSERT(!IsOpen());
    mFd = aFd;
    mGood = !!mFd;
    mBuffer.reset(new uint8_t[kBufferSize]);
    mBufferPos = 0;
  }

  void Close() {
    // We need to be API compatible with std::ostream, and so we silently handle
    // closes on a closed FD.
    if (IsOpen()) {
      Flush();
      PR_Close(mFd);
      mFd = nullptr;
      mBuffer.reset();
      mBufferPos = 0;
    }
  }

  bool IsOpen() {
    return mFd != nullptr;
  }

  void Flush() {
    // See comment in Close().
    if (IsOpen() && mBufferPos > 0) {
      PRInt32 length =
        PR_Write(mFd, static_cast<const void*>(mBuffer.get()), mBufferPos);
      mGood = length >= 0 && static_cast<size_t>(length) == mBufferPos;
      mBufferPos = 0;
    }
  }

  void Seek(PRInt64 aOffset, PRSeekWhence aWhence)
  {
    Flush();
    PRInt64 pos = PR_Seek64(mFd, aOffset, aWhence);
    mGood = pos != -1;
  }

  void write(const char* aData, size_t aSize) override {
    if (!good()) {
      return;
    }

    // See comment in Close().
    if (IsOpen()) {
      // If we're writing more data than could ever fit in our buffer, flush the
      // buffer and write directly.
      if (aSize > kBufferSize) {
        Flush();
        PRInt32 length = PR_Write(mFd, static_cast<const void*>(aData), aSize);
        mGood = length >= 0 && static_cast<size_t>(length) == aSize;
      // If our write could fit in our buffer, but doesn't because the buffer
      // is partially full, write to the buffer, flush the buffer, and then
      // write the rest of the data to the buffer.
      } else if (aSize > AvailableBufferSpace()) {
        size_t length = AvailableBufferSpace();
        WriteToBuffer(aData, length);
        Flush();

        WriteToBuffer(aData + length, aSize - length);
      // Write fits in the buffer.
      } else {
        WriteToBuffer(aData, aSize);
      }
    }
  }

  void read(char* aOut, size_t aSize) override {
    if (!good()) {
      return;
    }

    Flush();
    PRInt32 res = PR_Read(mFd, static_cast<void*>(aOut), aSize);
    mGood = res >= 0 && (static_cast<size_t>(res) == aSize);
  }

  bool good() {
    return mGood;
  }

private:
  size_t AvailableBufferSpace() {
    return kBufferSize - mBufferPos;
  }

  void WriteToBuffer(const char* aData, size_t aSize) {
    MOZ_ASSERT(aSize <= AvailableBufferSpace());
    memcpy(mBuffer.get() + mBufferPos, aData, aSize);
    mBufferPos += aSize;
  }

  PRFileDesc* mFd;
  std::unique_ptr<uint8_t[]> mBuffer;
  size_t mBufferPos;
  bool mGood;
};

class DrawEventRecorderPRFileDesc : public gfx::DrawEventRecorderPrivate
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawEventRecorderPRFileDesc, override)
  explicit DrawEventRecorderPRFileDesc(){};
  ~DrawEventRecorderPRFileDesc();

  void RecordEvent(const gfx::RecordedEvent& aEvent) override;

  /**
   * Returns whether a recording file is currently open.
   */
  bool IsOpen();

  /**
   * Opens the recorder with the provided PRFileDesc *.
   */
  void OpenFD(PRFileDesc* aFd);

  /**
   * Closes the file so that it can be processed. The recorder does NOT forget
   * which objects it has recorded. This can be used with OpenNew, so that a
   * recording can be processed in chunks. The file must be open.
   */
  void Close();

private:
  void Flush() override;

  PRFileDescStream mOutputStream;
};

}

}

#endif /* mozilla_layout_printing_DrawEventRecorder_h */
