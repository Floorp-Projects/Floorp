/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workerinternals_JSSettings_h
#define mozilla_dom_workerinternals_JSSettings_h

#include <stdint.h>

#include "jsapi.h"
#include "js/ContextOptions.h"
#include "js/GCAPI.h"
#include "mozilla/Maybe.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
namespace workerinternals {

// Random unique constant to facilitate JSPrincipal debugging
const uint32_t kJSPrincipalsDebugToken = 0x7e2df9d2;

struct JSSettings {
  struct JSGCSetting {
    JSGCParamKey key;
    // Nothing() represents the default value, the result of calling
    // JS_ResetGCParameter.
    Maybe<uint32_t> value;

    // For the IndexOf call below.
    bool operator==(JSGCParamKey k) const { return key == k; }
  };

  // Settings that change based on chrome/content context.
  struct JSContentChromeSettings {
    JS::RealmOptions realmOptions;
    int32_t maxScriptRuntime = 0;
  };

  JSContentChromeSettings chrome;
  JSContentChromeSettings content;
  CopyableTArray<JSGCSetting> gcSettings;
  JS::ContextOptions contextOptions;

#ifdef JS_GC_ZEAL
  uint8_t gcZeal = 0;
  uint32_t gcZealFrequency = 0;
#endif

  // Returns whether there was a change in the setting.
  bool ApplyGCSetting(JSGCParamKey aKey, Maybe<uint32_t> aValue) {
    size_t index = gcSettings.IndexOf(aKey);
    if (index == gcSettings.NoIndex) {
      gcSettings.AppendElement(JSGCSetting{aKey, aValue});
      return true;
    }
    if (gcSettings[index].value != aValue) {
      gcSettings[index].value = aValue;
      return true;
    }
    return false;
  }
};

}  // namespace workerinternals
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_workerinternals_JSSettings_h
