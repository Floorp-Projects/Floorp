// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_FILE_DESCRIPTOR_SET_POSIX_H_
#define CHROME_COMMON_FILE_DESCRIPTOR_SET_POSIX_H_

#include <vector>

#include "base/basictypes.h"
#include "base/file_descriptor_posix.h"
#include "base/ref_counted.h"

// -----------------------------------------------------------------------------
// A FileDescriptorSet is an ordered set of POSIX file descriptors. These are
// associated with IPC messages so that descriptors can be transmitted over a
// UNIX domain socket.
// -----------------------------------------------------------------------------
class FileDescriptorSet : public base::RefCountedThreadSafe<FileDescriptorSet> {
 public:
  FileDescriptorSet();
  ~FileDescriptorSet();

  // This is the maximum number of descriptors per message. We need to know this
  // because the control message kernel interface has to be given a buffer which
  // is large enough to store all the descriptor numbers. Otherwise the kernel
  // tells us that it truncated the control data and the extra descriptors are
  // lost.
  //
  // In debugging mode, it's a fatal error to try and add more than this number
  // of descriptors to a FileDescriptorSet.
  enum {
    MAX_DESCRIPTORS_PER_MESSAGE = 7
  };

  // ---------------------------------------------------------------------------
  // Interfaces for building during message serialisation...

  // Add a descriptor to the end of the set. Returns false iff the set is full.
  bool Add(int fd);
  // Add a descriptor to the end of the set and automatically close it after
  // transmission. Returns false iff the set is full.
  bool AddAndAutoClose(int fd);

  // ---------------------------------------------------------------------------


  // ---------------------------------------------------------------------------
  // Interfaces for accessing during message deserialisation...

  // Return the number of descriptors
  unsigned size() const { return descriptors_.size(); }
  // Return true if no unconsumed descriptors remain
  bool empty() const { return descriptors_.empty(); }
  // Fetch the nth descriptor from the beginning of the set. Code using this
  // /must/ access the descriptors in order, except that it may wrap from the
  // end to index 0 again.
  //
  // This interface is designed for the deserialising code as it doesn't
  // support close flags.
  //   returns: file descriptor, or -1 on error
  int GetDescriptorAt(unsigned n) const;

  // ---------------------------------------------------------------------------


  // ---------------------------------------------------------------------------
  // Interfaces for transmission...

  // Fill an array with file descriptors without 'consuming' them. CommitAll
  // must be called after these descriptors have been transmitted.
  //   buffer: (output) a buffer of, at least, size() integers.
  void GetDescriptors(int* buffer) const;
  // This must be called after transmitting the descriptors returned by
  // GetDescriptors. It marks all the descriptors as consumed and closes those
  // which are auto-close.
  void CommitAll();

  // ---------------------------------------------------------------------------


  // ---------------------------------------------------------------------------
  // Interfaces for receiving...

  // Set the contents of the set from the given buffer. This set must be empty
  // before calling. The auto-close flag is set on all the descriptors so that
  // unconsumed descriptors are closed on destruction.
  void SetDescriptors(const int* buffer, unsigned count);

  // ---------------------------------------------------------------------------

 private:
  // A vector of descriptors and close flags. If this message is sent, then
  // these descriptors are sent as control data. After sending, any descriptors
  // with a true flag are closed. If this message has been received, then these
  // are the descriptors which were received and all close flags are true.
  std::vector<base::FileDescriptor> descriptors_;

  // This contains the index of the next descriptor which should be consumed.
  // It's used in a couple of ways. Firstly, at destruction we can check that
  // all the descriptors have been read (with GetNthDescriptor). Secondly, we
  // can check that they are read in order.
  mutable unsigned consumed_descriptor_highwater_;

  DISALLOW_COPY_AND_ASSIGN(FileDescriptorSet);
};

#endif  // CHROME_COMMON_FILE_DESCRIPTOR_SET_POSIX_H_
