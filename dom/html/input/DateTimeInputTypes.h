/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DateTimeInputTypes_h__
#define DateTimeInputTypes_h__

#include "InputType.h"

class DateTimeInputTypeBase : public ::InputType
{
public:
  ~DateTimeInputTypeBase() override {}

protected:
  explicit DateTimeInputTypeBase(mozilla::dom::HTMLInputElement* aInputElement)
    : InputType(aInputElement)
  {}
};

// input type=date
class DateInputType : public DateTimeInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) DateInputType(aInputElement);
  }

private:
  explicit DateInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : DateTimeInputTypeBase(aInputElement)
  {}
};

// input type=time
class TimeInputType : public DateTimeInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) TimeInputType(aInputElement);
  }

private:
  explicit TimeInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : DateTimeInputTypeBase(aInputElement)
  {}
};

// input type=week
class WeekInputType : public DateTimeInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) WeekInputType(aInputElement);
  }

private:
  explicit WeekInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : DateTimeInputTypeBase(aInputElement)
  {}
};

// input type=month
class MonthInputType : public DateTimeInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) MonthInputType(aInputElement);
  }

private:
  explicit MonthInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : DateTimeInputTypeBase(aInputElement)
  {}
};

// input type=datetime-local
class DateTimeLocalInputType : public DateTimeInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) DateTimeLocalInputType(aInputElement);
  }

private:
  explicit DateTimeLocalInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : DateTimeInputTypeBase(aInputElement)
  {}
};


#endif /* DateTimeInputTypes_h__ */
