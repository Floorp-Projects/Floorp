// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//------------------------------------------------------------------------------
// Tracked is the base class for all tracked objects.  During construction, it
// registers the fact that an instance was created, and at destruction time, it
// records that event.  The instance may be tagged with a name, which is refered
// to as its Location.  The Location is a file and line number, most
// typically indicated where the object was constructed.  In some cases, as the
// object's significance is refined (for example, a Task object is augmented to
// do additonal things), its Location may be redefined to that later location.

// Tracking includes (for each instance) recording the birth thread, death
// thread, and duration of life (from construction to destruction).  All this
// data is accumulated and filtered for review at about:objects.

#ifndef BASE_TRACKED_H_
#define BASE_TRACKED_H_

#include <string>

#include "base/time.h"

#ifndef NDEBUG
#ifndef TRACK_ALL_TASK_OBJECTS
#define TRACK_ALL_TASK_OBJECTS
#endif   // TRACK_ALL_TASK_OBJECTS
#endif  // NDEBUG

namespace tracked_objects {

//------------------------------------------------------------------------------
// Location provides basic info where of an object was constructed, or was
// significantly brought to life.

class Location {
 public:
  // Constructor should be called with a long-lived char*, such as __FILE__.
  // It assumes the provided value will persist as a global constant, and it
  // will not make a copy of it.
  Location(const char* aFunctionName, const char* aFilename, int aLineNumber)
      : function_name_(aFunctionName),
        file_name_(aFilename),
        line_number_(aLineNumber) { }

  // Provide a default constructor for easy of debugging.
  Location()
      : function_name_("Unknown"),
        file_name_("Unknown"),
        line_number_(-1) { }

  // Comparison operator for insertion into a std::map<> hash tables.
  // All we need is *some* (any) hashing distinction.  Strings should already
  // be unique, so we don't bother with strcmp or such.
  // Use line number as the primary key (because it is fast, and usually gets us
  // a difference), and then pointers as secondary keys (just to get some
  // distinctions).
  bool operator < (const Location& other) const {
    if (line_number_ != other.line_number_)
      return line_number_ < other.line_number_;
    if (file_name_ != other.file_name_)
      return file_name_ < other.file_name_;
    return function_name_ < other.function_name_;
  }

  const char* function_name() const { return function_name_; }
  const char* file_name()     const { return file_name_; }
  int line_number()           const { return line_number_; }

  void Write(bool display_filename, bool display_function_name,
             std::string* output) const;

  // Write function_name_ in HTML with '<' and '>' properly encoded.
  void WriteFunctionName(std::string* output) const;

 private:
  const char* const function_name_;
  const char* const file_name_;
  const int line_number_;
};


//------------------------------------------------------------------------------
// Define a macro to record the current source location.

#define FROM_HERE tracked_objects::Location(__FUNCTION__, __FILE__, __LINE__)


//------------------------------------------------------------------------------


class Births;

class Tracked {
 public:
  B2G_ACL_EXPORT Tracked();
  B2G_ACL_EXPORT virtual ~Tracked();

  // Used to record the FROM_HERE location of a caller.
  void SetBirthPlace(const Location& from_here);
  Location GetBirthPlace() const;

  // When a task sits around a long time, such as in a timer, or object watcher,
  // this method should be called when the task becomes active, and its
  // significant lifetime begins (and its waiting to be woken up has passed).
  void ResetBirthTime();

  bool MissingBirthplace() const;

 private:
#ifdef TRACK_ALL_TASK_OBJECTS

  // Pointer to instance were counts of objects with the same birth location
  // (on the same thread) are stored.
  Births* tracked_births_;
  // The time this object was constructed.  If its life consisted of a long
  // waiting period, and then it became active, then this value is generally
  // reset before the object begins it active life.
  base::Time tracked_birth_time_;

#endif  // TRACK_ALL_TASK_OBJECTS

  DISALLOW_COPY_AND_ASSIGN(Tracked);
};

}  // namespace tracked_objects

#endif  // BASE_TRACKED_H_
