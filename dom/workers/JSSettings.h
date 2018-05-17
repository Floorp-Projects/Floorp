/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workerinternals_JSSettings_h
#define mozilla_dom_workerinternals_JSSettings_h

#include "jsapi.h"
#include "mozilla/Maybe.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
namespace workerinternals {

// Random unique constant to facilitate JSPrincipal debugging
const uint32_t kJSPrincipalsDebugToken = 0x7e2df9d2;

struct JSSettings
{
  enum {
    // All the GC parameters that we support.
    JSSettings_JSGC_MAX_BYTES = 0,
    JSSettings_JSGC_MAX_MALLOC_BYTES,
    JSSettings_JSGC_HIGH_FREQUENCY_TIME_LIMIT,
    JSSettings_JSGC_LOW_FREQUENCY_HEAP_GROWTH,
    JSSettings_JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN,
    JSSettings_JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX,
    JSSettings_JSGC_HIGH_FREQUENCY_LOW_LIMIT,
    JSSettings_JSGC_HIGH_FREQUENCY_HIGH_LIMIT,
    JSSettings_JSGC_ALLOCATION_THRESHOLD,
    JSSettings_JSGC_SLICE_TIME_BUDGET,
    JSSettings_JSGC_DYNAMIC_HEAP_GROWTH,
    JSSettings_JSGC_DYNAMIC_MARK_SLICE,
    // JSGC_MODE not supported

    // This must be last so that we get an accurate count.
    kGCSettingsArraySize
  };

  struct JSGCSetting
  {
    mozilla::Maybe<JSGCParamKey> key;
    uint32_t value;

    JSGCSetting()
    : key(), value(0)
    { }
  };

  // There are several settings that we know we need so it makes sense to
  // preallocate here.
  typedef JSGCSetting JSGCSettingsArray[kGCSettingsArraySize];

  // Settings that change based on chrome/content context.
  struct JSContentChromeSettings
  {
    JS::RealmOptions realmOptions;
    int32_t maxScriptRuntime;

    JSContentChromeSettings()
    : realmOptions(), maxScriptRuntime(0)
    { }
  };

  JSContentChromeSettings chrome;
  JSContentChromeSettings content;
  JSGCSettingsArray gcSettings;
  JS::ContextOptions contextOptions;

#ifdef JS_GC_ZEAL
  uint8_t gcZeal;
  uint32_t gcZealFrequency;
#endif

  JSSettings()
#ifdef JS_GC_ZEAL
  : gcZeal(0), gcZealFrequency(0)
#endif
  {
    for (uint32_t index = 0; index < ArrayLength(gcSettings); index++) {
      new (gcSettings + index) JSGCSetting();
    }
  }

  bool
  ApplyGCSetting(JSGCParamKey aKey, uint32_t aValue)
  {
    JSSettings::JSGCSetting* firstEmptySetting = nullptr;
    JSSettings::JSGCSetting* foundSetting = nullptr;

    for (uint32_t index = 0; index < ArrayLength(gcSettings); index++) {
      JSSettings::JSGCSetting& setting = gcSettings[index];
      if (setting.key.isSome() && *setting.key == aKey) {
        foundSetting = &setting;
        break;
      }
      if (!firstEmptySetting && setting.key.isNothing()) {
        firstEmptySetting = &setting;
      }
    }

    if (aValue) {
      if (!foundSetting) {
        foundSetting = firstEmptySetting;
        if (!foundSetting) {
          NS_ERROR("Not enough space for this value!");
          return false;
        }
      }
      foundSetting->key = mozilla::Some(aKey);
      foundSetting->value = aValue;
      return true;
    }

    if (foundSetting) {
      foundSetting->key.reset();
      return true;
    }

    return false;
  }
};

} // workerinternals namespace
} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_workerinternals_JSSettings_h
