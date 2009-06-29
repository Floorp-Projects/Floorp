// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Counters used within the browser.

#ifndef CHROME_COMMON_CHROME_COUNTERS_H_
#define CHROME_COMMON_CHROME_COUNTERS_H_

class StatsCounter;
class StatsCounterTimer;
class StatsRate;

namespace chrome {

class Counters {
 public:
  // The number of messages sent on IPC channels.
  static StatsCounter& ipc_send_counter();

  // The amount of time spent in chrome initialization.
  static StatsCounterTimer& chrome_main();

  // The amount of time spent in renderer initialization.
  static StatsCounterTimer& renderer_main();

  // Time spent in spellchecker initialization.
  static StatsCounterTimer& spellcheck_init();

  // Time/Count of spellcheck lookups.
  static StatsRate& spellcheck_lookup();

  // Time spent loading the Chrome plugins.
  static StatsCounterTimer& plugin_load();

  // Time/Count of plugin network interception.
  static StatsRate& plugin_intercept();
};

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_COUNTERS_H_
