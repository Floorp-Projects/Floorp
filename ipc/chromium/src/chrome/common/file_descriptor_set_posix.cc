// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/file_descriptor_set_posix.h"

#include "base/eintr_wrapper.h"
#include "base/logging.h"

#include <unistd.h>

FileDescriptorSet::FileDescriptorSet()
    : consumed_descriptor_highwater_(0) {
}

FileDescriptorSet::~FileDescriptorSet() {
  if (consumed_descriptor_highwater_ == descriptors_.size())
    return;

  CHROMIUM_LOG(WARNING) << "FileDescriptorSet destroyed with unconsumed descriptors";
  // We close all the descriptors where the close flag is set. If this
  // message should have been transmitted, then closing those with close
  // flags set mirrors the expected behaviour.
  //
  // If this message was received with more descriptors than expected
  // (which could a DOS against the browser by a rogue renderer) then all
  // the descriptors have their close flag set and we free all the extra
  // kernel resources.
  for (unsigned i = consumed_descriptor_highwater_;
       i < descriptors_.size(); ++i) {
    if (descriptors_[i].auto_close)
      HANDLE_EINTR(close(descriptors_[i].fd));
  }
}

bool FileDescriptorSet::Add(int fd) {
  if (descriptors_.size() == MAX_DESCRIPTORS_PER_MESSAGE)
    return false;

  struct base::FileDescriptor sd;
  sd.fd = fd;
  sd.auto_close = false;
  descriptors_.push_back(sd);
  return true;
}

bool FileDescriptorSet::AddAndAutoClose(int fd) {
  if (descriptors_.size() == MAX_DESCRIPTORS_PER_MESSAGE)
    return false;

  struct base::FileDescriptor sd;
  sd.fd = fd;
  sd.auto_close = true;
  descriptors_.push_back(sd);
  DCHECK(descriptors_.size() <= MAX_DESCRIPTORS_PER_MESSAGE);
  return true;
}

int FileDescriptorSet::GetDescriptorAt(unsigned index) const {
  if (index >= descriptors_.size())
    return -1;

  // We should always walk the descriptors in order, so it's reasonable to
  // enforce this. Consider the case where a compromised renderer sends us
  // the following message:
  //
  //   ExampleMsg:
  //     num_fds:2 msg:FD(index = 1) control:SCM_RIGHTS {n, m}
  //
  // Here the renderer sent us a message which should have a descriptor, but
  // actually sent two in an attempt to fill our fd table and kill us. By
  // setting the index of the descriptor in the message to 1 (it should be
  // 0), we would record a highwater of 1 and then consider all the
  // descriptors to have been used.
  //
  // So we can either track of the use of each descriptor in a bitset, or we
  // can enforce that we walk the indexes strictly in order.
  //
  // There's one more wrinkle: When logging messages, we may reparse them. So
  // we have an exception: When the consumed_descriptor_highwater_ is at the
  // end of the array and index 0 is requested, we reset the highwater value.
  if (index == 0 && consumed_descriptor_highwater_ == descriptors_.size())
    consumed_descriptor_highwater_ = 0;

  if (index != consumed_descriptor_highwater_)
    return -1;

  consumed_descriptor_highwater_ = index + 1;
  return descriptors_[index].fd;
}

void FileDescriptorSet::GetDescriptors(int* buffer) const {
  for (std::vector<base::FileDescriptor>::const_iterator
       i = descriptors_.begin(); i != descriptors_.end(); ++i) {
    *(buffer++) = i->fd;
  }
}

void FileDescriptorSet::CommitAll() {
  for (std::vector<base::FileDescriptor>::iterator
       i = descriptors_.begin(); i != descriptors_.end(); ++i) {
    if (i->auto_close)
      HANDLE_EINTR(close(i->fd));
  }
  descriptors_.clear();
  consumed_descriptor_highwater_ = 0;
}

void FileDescriptorSet::SetDescriptors(const int* buffer, unsigned count) {
  DCHECK_LE(count, MAX_DESCRIPTORS_PER_MESSAGE);
  DCHECK_EQ(descriptors_.size(), 0u);
  DCHECK_EQ(consumed_descriptor_highwater_, 0u);

  descriptors_.reserve(count);
  for (unsigned i = 0; i < count; ++i) {
    struct base::FileDescriptor sd;
    sd.fd = buffer[i];
    sd.auto_close = true;
    descriptors_.push_back(sd);
  }
}
