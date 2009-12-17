// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTANT_FILE_WRITER_H_
#define CHROME_COMMON_IMPORTANT_FILE_WRITER_H_

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/non_thread_safe.h"
#include "base/time.h"
#include "base/timer.h"

namespace base {
class Thread;
}

// Helper to ensure that a file won't be corrupted by the write (for example on
// application crash). Consider a naive way to save an important file F:
//
// 1. Open F for writing, truncating it.
// 2. Write new data to F.
//
// It's good when it works, but it gets very bad if step 2. doesn't complete.
// It can be caused by a crash, a computer hang, or a weird I/O error. And you
// end up with a broken file.
//
// To be safe, we don't start with writing directly to F. Instead, we write to
// to a temporary file. Only after that write is successful, we rename the
// temporary file to target filename.
//
// If you want to know more about this approach and ext3/ext4 fsync issues, see
// http://valhenson.livejournal.com/37921.html
class ImportantFileWriter : public NonThreadSafe {
 public:
  // Initialize the writer.
  // |path| is the name of file to write. Disk operations will be executed on
  // |backend_thread|, or current thread if |backend_thread| is NULL.
  //
  // All non-const methods, ctor and dtor must be called on the same thread.
  ImportantFileWriter(const FilePath& path, const base::Thread* backend_thread);

  // On destruction all pending writes are executed on |backend_thread|.
  ~ImportantFileWriter();

  FilePath path() const { return path_; }

  // Save |data| to target filename. Does not block. If there is a pending write
  // scheduled by ScheduleWrite, it is cancelled.
  void WriteNow(const std::string& data);

  // Schedule saving |data| to target filename. Data will be saved to disk after
  // the commit interval. If an other ScheduleWrite is issued before that, only
  // one write to disk will happen - with the more recent data. This operation
  // does not block.
  void ScheduleWrite(const std::string& data);

  base::TimeDelta commit_interval() const {
    return commit_interval_;
  }

  void set_commit_interval(const base::TimeDelta& interval) {
    commit_interval_ = interval;
  }

 private:
  // If there is a data scheduled to write, issue a disk operation.
  void CommitPendingData();

  // Path being written to.
  const FilePath path_;

  // Thread on which disk operation run. NULL means no separate thread is used.
  const base::Thread* backend_thread_;

  // Timer used to schedule commit after ScheduleWrite.
  base::OneShotTimer<ImportantFileWriter> timer_;

  // Data scheduled to save.
  std::string data_;

  // Time delta after which scheduled data will be written to disk.
  base::TimeDelta commit_interval_;

  DISALLOW_COPY_AND_ASSIGN(ImportantFileWriter);
};

#endif  // CHROME_COMMON_IMPORTANT_FILE_WRITER_H_
