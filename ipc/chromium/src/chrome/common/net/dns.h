// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has shared types used across IPC between render_dns_master.cc
// and dns_master.cc


#ifndef CHROME_COMMON_DNS_H_
#define CHROME_COMMON_DNS_H_

#include <string>
#include <vector>

namespace chrome_common_net {

// IPC messages are passed from the renderer to the browser in the form of
// Namelist instances.
// Each element of this vector is a hostname that needs to be looked up.
// The hostnames should never be empty strings.
typedef std::vector<std::string> NameList;
}

#endif  // CHROME_COMMON_DNS_H_
