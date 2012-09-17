// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_test_sink.h"

namespace IPC {

TestSink::TestSink() {
}

TestSink::~TestSink() {
}

void TestSink::OnMessageReceived(const Message& msg) {
  messages_.push_back(Message(msg));
}

void TestSink::ClearMessages() {
  messages_.clear();
}

const Message* TestSink::GetMessageAt(size_t index) const {
  if (index >= messages_.size())
    return NULL;
  return &messages_[index];
}

const Message* TestSink::GetFirstMessageMatching(uint16_t id) const {
  for (size_t i = 0; i < messages_.size(); i++) {
    if (messages_[i].type() == id)
      return &messages_[i];
  }
  return NULL;
}

const Message* TestSink::GetUniqueMessageMatching(uint16_t id) const {
  size_t found_index = 0;
  size_t found_count = 0;
  for (size_t i = 0; i < messages_.size(); i++) {
    if (messages_[i].type() == id) {
      found_count++;
      found_index = i;
    }
  }
  if (found_count != 1)
    return NULL;  // Didn't find a unique one.
  return &messages_[found_index];
}

}  // namespace IPC
