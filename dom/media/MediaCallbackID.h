/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaCallbackID_h_
#define MediaCallbackID_h_

namespace mozilla {

struct CallbackID
{
  static char const* INVALID_TAG;
  static int32_t const INVALID_ID;

  CallbackID();

  explicit CallbackID(char const* aTag, int32_t aID = 0);

  CallbackID& operator++();   // prefix++

  CallbackID operator++(int); // postfix++

  bool operator==(const CallbackID& rhs) const;

  bool operator!=(const CallbackID& rhs) const;

  operator int() const;

private:
  char const* mTag;
  int32_t mID;
};

} // namespace mozilla

#endif // MediaCallbackID_h_
