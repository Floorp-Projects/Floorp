//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// signal_utils:
//   Implements the Observer pattern for sending state change notifications
//   from Subject objects to dependent Observer objects.
//
//   See design document:
//   https://docs.google.com/document/d/15Edfotqg6_l1skTEL8ADQudF_oIdNa7i8Po43k6jMd4/

#include "libANGLE/signal_utils.h"

#include <algorithm>

#include "common/debug.h"

namespace angle
{
// Observer implementation.
ObserverInterface::~ObserverInterface() = default;

// Subject implementation.
Subject::Subject()
{
}

Subject::~Subject()
{
    resetObservers();
}

bool Subject::hasObservers() const
{
    return !mObservers.empty();
}

void Subject::addObserver(ObserverBinding *observer)
{
    ASSERT(std::find(mObservers.begin(), mObservers.end(), observer) == mObservers.end());
    mObservers.push_back(observer);
}

void Subject::removeObserver(ObserverBinding *observer)
{
    auto iter = std::find(mObservers.begin(), mObservers.end(), observer);
    ASSERT(iter != mObservers.end());
    mObservers.erase(iter);
}

void Subject::onStateChange(const gl::Context *context, SubjectMessage message) const
{
    if (mObservers.empty())
        return;

    for (const angle::ObserverBinding *receiver : mObservers)
    {
        receiver->onStateChange(context, message);
    }
}

void Subject::resetObservers()
{
    for (angle::ObserverBinding *observer : mObservers)
    {
        observer->onSubjectReset();
    }
    mObservers.clear();
}

// ObserverBinding implementation.
ObserverBinding::ObserverBinding(ObserverInterface *observer, SubjectIndex index)
    : mSubject(nullptr), mObserver(observer), mIndex(index)
{
    ASSERT(observer);
}

ObserverBinding::~ObserverBinding()
{
    reset();
}

ObserverBinding::ObserverBinding(const ObserverBinding &other) = default;

ObserverBinding &ObserverBinding::operator=(const ObserverBinding &other) = default;

void ObserverBinding::bind(Subject *subject)
{
    ASSERT(mObserver);
    if (mSubject)
    {
        mSubject->removeObserver(this);
    }

    mSubject = subject;

    if (mSubject)
    {
        mSubject->addObserver(this);
    }
}

void ObserverBinding::reset()
{
    bind(nullptr);
}

void ObserverBinding::onStateChange(const gl::Context *context, SubjectMessage message) const
{
    mObserver->onSubjectStateChange(context, mIndex, message);
}

void ObserverBinding::onSubjectReset()
{
    mSubject = nullptr;
}
}  // namespace angle
