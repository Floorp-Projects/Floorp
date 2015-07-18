/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TimelineConsumers_h_
#define mozilla_TimelineConsumers_h_

class nsDocShell;

namespace mozilla {

// # TimelineConsumers
//
// A class to trace how many frontends are interested in markers. Whenever
// interest is expressed in markers, these fields will keep track of that.
class TimelineConsumers
{
private:
  // Counter for how many timelines are currently interested in markers.
  static unsigned long sActiveConsumers;

public:
  static void AddConsumer();
  static void RemoveConsumer();
  static bool IsEmpty();
};

} // namespace mozilla

#endif /* mozilla_TimelineConsumers_h_ */
