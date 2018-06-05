//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Observer:
//   Implements the Observer pattern for sending state change notifications
//   from Subject objects to dependent Observer objects.
//
//   See design document:
//   https://docs.google.com/document/d/15Edfotqg6_l1skTEL8ADQudF_oIdNa7i8Po43k6jMd4/

#include "libANGLE/Observer.h"

#include <algorithm>

#include "common/debug.h"

namespace angle
{
namespace
{
template <typename HaystackT, typename NeedleT>
bool IsInContainer(const HaystackT &haystack, const NeedleT &needle)
{
    return std::find(haystack.begin(), haystack.end(), needle) != haystack.end();
}
}  // anonymous namespace

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
    return !mFastObservers.empty();
}

void Subject::addObserver(ObserverBinding *observer)
{
    ASSERT(!IsInContainer(mFastObservers, observer) && !IsInContainer(mSlowObservers, observer));

    if (!mFastObservers.full())
    {
        mFastObservers.push_back(observer);
    }
    else
    {
        mSlowObservers.push_back(observer);
    }
}

void Subject::removeObserver(ObserverBinding *observer)
{
    auto iter = std::find(mFastObservers.begin(), mFastObservers.end(), observer);
    if (iter != mFastObservers.end())
    {
        size_t index = iter - mFastObservers.begin();
        std::swap(mFastObservers[index], mFastObservers[mFastObservers.size() - 1]);
        mFastObservers.resize(mFastObservers.size() - 1);
        if (!mSlowObservers.empty())
        {
            mFastObservers.push_back(mSlowObservers.back());
            mSlowObservers.pop_back();
            ASSERT(mFastObservers.full());
        }
    }
    else
    {
        auto slowIter = std::find(mSlowObservers.begin(), mSlowObservers.end(), observer);
        ASSERT(slowIter != mSlowObservers.end());
        mSlowObservers.erase(slowIter);
    }
}

void Subject::onStateChange(const gl::Context *context, SubjectMessage message) const
{
    if (mFastObservers.empty())
        return;

    for (const angle::ObserverBinding *receiver : mFastObservers)
    {
        receiver->onStateChange(context, message);
    }

    for (const angle::ObserverBinding *receiver : mSlowObservers)
    {
        receiver->onStateChange(context, message);
    }
}

void Subject::resetObservers()
{
    for (angle::ObserverBinding *observer : mFastObservers)
    {
        observer->onSubjectReset();
    }
    mFastObservers.clear();

    for (angle::ObserverBinding *observer : mSlowObservers)
    {
        observer->onSubjectReset();
    }
    mSlowObservers.clear();
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

const Subject *ObserverBinding::getSubject() const
{
    return mSubject;
}
}  // namespace angle
