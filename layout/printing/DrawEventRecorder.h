/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_printing_DrawEventRecorder_h
#define mozilla_layout_printing_DrawEventRecorder_h

#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/gfx/RecordingTypes.h"
#include "prio.h"

namespace mozilla {
namespace layout {

class PRFileDescStream : public mozilla::gfx::EventStream {
public:
  PRFileDescStream() : mFd(nullptr), mGood(true) {}

  void OpenFD(PRFileDesc* aFd) {
    MOZ_ASSERT(!IsOpen());
    mFd = aFd;
    mGood = true;
  }

  void Close() {
    PR_Close(mFd);
    mFd = nullptr;
  }

  bool IsOpen() {
    return mFd != nullptr;
  }

  void Flush() {
    // For std::ostream this flushes any internal buffers. PRFileDesc's IO isn't
    // buffered, so nothing to do here.
  }

  void Seek(PRInt32 aOffset, PRSeekWhence aWhence) {
    PR_Seek(mFd, aOffset, aWhence);
  }

  void write(const char* aData, size_t aSize) {
    // We need to be API compatible with std::ostream, and so we silently handle
    // writes on a closed FD.
    if (IsOpen()) {
      PR_Write(mFd, static_cast<const void*>(aData), aSize);
    }
  }

  void read(char* aOut, size_t aSize) {
    PRInt32 res = PR_Read(mFd, static_cast<void*>(aOut), aSize);
    mGood = res >= 0 && ((size_t)res == aSize);
  }

  bool good() {
    return mGood;
  }

private:
  PRFileDesc* mFd;
  bool mGood;
};

class DrawEventRecorderPRFileDesc : public gfx::DrawEventRecorderPrivate
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawEventRecorderPRFileDesc, override)
  explicit DrawEventRecorderPRFileDesc() { };
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
