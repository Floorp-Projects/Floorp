// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/important_file_writer.h"

#include <stdio.h>

#include <ostream>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/time.h"

using base::TimeDelta;

namespace {

const int kDefaultCommitIntervalMs = 10000;

class WriteToDiskTask : public Task {
 public:
  WriteToDiskTask(const FilePath& path, const std::string& data)
      : path_(path),
        data_(data) {
  }

  virtual void Run() {
    // Write the data to a temp file then rename to avoid data loss if we crash
    // while writing the file. Ensure that the temp file is on the same volume
    // as target file, so it can be moved in one step, and that the temp file
    // is securely created.
    FilePath tmp_file_path;
    FILE* tmp_file = file_util::CreateAndOpenTemporaryFileInDir(
        path_.DirName(), &tmp_file_path);
    if (!tmp_file) {
      LogFailure("could not create temporary file");
      return;
    }

    size_t bytes_written = fwrite(data_.data(), 1, data_.length(), tmp_file);
    if (!file_util::CloseFile(tmp_file)) {
      file_util::Delete(tmp_file_path, false);
      LogFailure("failed to close temporary file");
      return;
    }
    if (bytes_written < data_.length()) {
      file_util::Delete(tmp_file_path, false);
      LogFailure("error writing, bytes_written=" + UintToString(bytes_written));
      return;
    }

    if (file_util::Move(tmp_file_path, path_)) {
      LogSuccess();
      return;
    }

    file_util::Delete(tmp_file_path, false);
    LogFailure("could not rename temporary file");
  }

 private:
  void LogSuccess() {
    LOG(INFO) << "successfully saved " << path_.value();
  }

  void LogFailure(const std::string& message) {
    LOG(WARNING) << "failed to write " << path_.value()
                 << ": " << message;
  }

  const FilePath path_;
  const std::string data_;

  DISALLOW_COPY_AND_ASSIGN(WriteToDiskTask);
};

}  // namespace

ImportantFileWriter::ImportantFileWriter(const FilePath& path,
                                         const base::Thread* backend_thread)
    : path_(path),
      backend_thread_(backend_thread),
      commit_interval_(TimeDelta::FromMilliseconds(kDefaultCommitIntervalMs)) {
  DCHECK(CalledOnValidThread());
}

ImportantFileWriter::~ImportantFileWriter() {
  DCHECK(CalledOnValidThread());
  if (timer_.IsRunning()) {
    timer_.Stop();
    CommitPendingData();
  }
}

void ImportantFileWriter::WriteNow(const std::string& data) {
  DCHECK(CalledOnValidThread());

  if (timer_.IsRunning())
    timer_.Stop();

  Task* task = new WriteToDiskTask(path_, data);
  if (backend_thread_) {
    backend_thread_->message_loop()->PostTask(FROM_HERE, task);
  } else {
    task->Run();
    delete task;
  }
}

void ImportantFileWriter::ScheduleWrite(const std::string& data) {
  DCHECK(CalledOnValidThread());

  data_ = data;
  if (!timer_.IsRunning()) {
    timer_.Start(commit_interval_, this,
                 &ImportantFileWriter::CommitPendingData);
  }
}

void ImportantFileWriter::CommitPendingData() {
  WriteNow(data_);
}
