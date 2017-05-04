/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InputType_h__
#define InputType_h__

#include <stdint.h>
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace dom {
class HTMLInputElement;
} // namespace dom
} // namespace mozilla

struct DoNotDelete;

/**
 * A common superclass for different types of a HTMLInputElement.
 */
class InputType
{
public:
  static mozilla::UniquePtr<InputType, DoNotDelete>
  Create(mozilla::dom::HTMLInputElement* aInputElement, uint8_t aType,
         void* aMemory);

  virtual ~InputType() {}

  /**
   * Drop the reference to the input element.
   */
  void DropReference();

  virtual bool IsTooLong() const;
  virtual bool IsTooShort() const;

protected:
  explicit InputType(mozilla::dom::HTMLInputElement* aInputElement)
    : mInputElement(aInputElement)
  {}

  mozilla::dom::HTMLInputElement* mInputElement;
};

// Custom deleter for UniquePtr<InputType> to avoid freeing memory pre-allocated
// for InputType, but we still need to call the destructor explictly.
struct DoNotDelete { void operator()(::InputType* p) { p->~InputType(); } };

#endif /* InputType_h__ */
