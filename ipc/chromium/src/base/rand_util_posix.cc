// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/logging.h"

namespace base {

uint64_t RandUint64() {
  uint64_t number;

  int urandom_fd = open("/dev/urandom", O_RDONLY);
  CHECK(urandom_fd >= 0);
  bool success = file_util::ReadFromFD(urandom_fd,
                                       reinterpret_cast<char*>(&number),
                                       sizeof(number));
  CHECK(success);
  close(urandom_fd);

  return number;
}

}  // namespace base
