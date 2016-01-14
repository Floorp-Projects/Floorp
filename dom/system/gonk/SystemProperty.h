/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_Property_h
#define mozilla_system_Property_h

namespace mozilla {
namespace system {

/**
* Abstraction of property_get/property_get in libcutils from AOSP.
*/
class Property
{
public:
  // Constants defined in system_properties.h from AOSP.
  enum {
    KEY_MAX_LENGTH = 32,
    VALUE_MAX_LENGTH = 92
  };

  static int
  Get(const char* key, char* value, const char* default_value);

  static int
  Set(const char* key, const char* value);

private:
  Property() {}
  virtual ~Property() {}
};

} // namespace system
} // namespace mozilla

 #endif // mozilla_system_Property_h
