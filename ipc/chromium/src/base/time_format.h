// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic time formatting methods.  These methods use the current locale
// formatting for displaying the time.

#ifndef BASE_TIME_FORMAT_H_
#define BASE_TIME_FORMAT_H_

#include <string>

namespace base {

class Time;

// Returns the time of day, e.g., "3:07 PM".
std::wstring TimeFormatTimeOfDay(const Time& time);

// Returns a shortened date, e.g. "Nov 7, 2007"
std::wstring TimeFormatShortDate(const Time& time);

// Returns a numeric date such as 12/13/52.
std::wstring TimeFormatShortDateNumeric(const Time& time);

// Formats a time in a friendly sentence format, e.g.
// "Monday, March 6, 2008 2:44:30 PM".
std::wstring TimeFormatShortDateAndTime(const Time& time);

// Formats a time in a friendly sentence format, e.g.
// "Monday, March 6, 2008 2:44:30 PM".
std::wstring TimeFormatFriendlyDateAndTime(const Time& time);

// Formats a time in a friendly sentence format, e.g.
// "Monday, March 6, 2008".
std::wstring TimeFormatFriendlyDate(const Time& time);

}  // namespace base

#endif  // BASE_TIME_FORMAT_H_
