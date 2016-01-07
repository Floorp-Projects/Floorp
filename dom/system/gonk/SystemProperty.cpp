/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SystemProperty.h"
#include <dlfcn.h>

#include "prinit.h"

namespace mozilla {
namespace system {

namespace {

typedef int (*PropertyGet)(const char*, char*, const char*);
typedef int (*PropertySet)(const char*, const char*);

static void *sLibcUtils;
static PRCallOnceType sInitLibcUtils;

static int
FakePropertyGet(const char* key, char* value, const char* default_value)
{
  if(!default_value) {
    value[0] = '\0';
    return 0;
  }

  int len = strlen(default_value);
  if (len >= Property::VALUE_MAX_LENGTH) {
    len = Property::VALUE_MAX_LENGTH - 1;
  }
  memcpy(value, default_value, len);
  value[len] = '\0';

  return len;
}

static int
FakePropertySet(const char* key, const char* value)
{
  return 0;
}

static PRStatus
InitLibcUtils()
{
  sLibcUtils = dlopen("/system/lib/libcutils.so", RTLD_LAZY);
  // We will fallback to the fake getter/setter when sLibcUtils is not valid.
  return PR_SUCCESS;
}

static void*
GetLibcUtils()
{
  PR_CallOnce(&sInitLibcUtils, InitLibcUtils);
  return sLibcUtils;
}

} // anonymous namespace

/*static*/ int
Property::Get(const char* key, char* value, const char* default_value)
{
  void *libcutils = GetLibcUtils();
  if (libcutils) {
    PropertyGet getter = (PropertyGet) dlsym(libcutils, "property_get");
    if (getter) {
      return getter(key, value, default_value);
    }
    NS_WARNING("Failed to get property_get() from libcutils!");
  }
  NS_WARNING("Fallback to the FakePropertyGet()");
  return FakePropertyGet(key, value, default_value);
}

/*static*/ int
Property::Set(const char* key, const char* value)
{
  void *libcutils = GetLibcUtils();
  if (libcutils) {
    PropertySet setter = (PropertySet) dlsym(libcutils, "property_set");
    if (setter) {
      return setter(key, value);
    }
    NS_WARNING("Failed to get property_set() from libcutils!");
  }
  NS_WARNING("Fallback to the FakePropertySet()");
  return FakePropertySet(key, value);
}

} // namespace system
} // namespace mozilla
