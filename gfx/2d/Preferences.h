/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_Preferences_h
#define mozilla_gfx_Preferences_h

namespace mozilla {
namespace gfx {

class PreferenceAccess
{
public:
  virtual ~PreferenceAccess() {};

  // This will be called with the derived class, so we will can register the
  // callbacks with it.
  static void SetAccess(PreferenceAccess* aAccess);

  static int32_t RegisterLivePref(const char* aName, int32_t* aVar,
                                  int32_t aDefault);
protected:
  // This should connect the variable aVar to be updated whenever a preference
  // aName is modified.  aDefault would be used if the preference is undefined,
  // so that we always get the valid value for aVar.
  virtual void LivePref(const char* aName, int32_t* aVar, int32_t aDefault) = 0;
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_gfx_Preferences_h
