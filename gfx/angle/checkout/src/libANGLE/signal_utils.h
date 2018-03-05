//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// signal_utils:
//   Implements the Observer pattern for sending state change notifications
//   from Subject objects to dependent Observer objects.
//
//   See design document:
//   https://docs.google.com/document/d/15Edfotqg6_l1skTEL8ADQudF_oIdNa7i8Po43k6jMd4/

#ifndef LIBANGLE_SIGNAL_UTILS_H_
#define LIBANGLE_SIGNAL_UTILS_H_

#include "common/angleutils.h"

namespace gl
{
class Context;
}  // namespace gl

namespace angle
{

using SubjectIndex = size_t;

enum class SubjectMessage
{
    STATE_CHANGE,
};

// The observing class inherits from this interface class.
class ObserverInterface
{
  public:
    virtual ~ObserverInterface();
    virtual void onSubjectStateChange(const gl::Context *context,
                                      SubjectIndex index,
                                      SubjectMessage message) = 0;
};

class ObserverBinding;

// Maintains a list of observer bindings. Sends update messages to the observer.
class Subject : NonCopyable
{
  public:
    Subject();
    virtual ~Subject();

    void onStateChange(const gl::Context *context, SubjectMessage message) const;
    bool hasObservers() const;
    void resetObservers();

  private:
    // Only the ObserverBinding class should add or remove observers.
    friend class ObserverBinding;
    void addObserver(ObserverBinding *observer);
    void removeObserver(ObserverBinding *observer);

    std::vector<ObserverBinding *> mObservers;
};

// Keeps a binding between a Subject and Observer, with a specific subject index.
class ObserverBinding final
{
  public:
    ObserverBinding(ObserverInterface *observer, SubjectIndex index);
    ~ObserverBinding();
    ObserverBinding(const ObserverBinding &other);
    ObserverBinding &operator=(const ObserverBinding &other);

    void bind(Subject *subject);
    void reset();
    void onStateChange(const gl::Context *context, SubjectMessage message) const;
    void onSubjectReset();

  private:
    Subject *mSubject;
    ObserverInterface *mObserver;
    SubjectIndex mIndex;
};

}  // namespace angle

#endif  // LIBANGLE_SIGNAL_UTILS_H_
