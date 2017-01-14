/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoPropPrefList_h
#define mozilla_ServoPropPrefList_h

namespace mozilla {

#define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_, kwtable_, \
                 stylestruct_, stylestructoffset_, animtype_)                 \
    const bool SERVO_PREF_ENABLED_##id_ = !(sizeof(pref_) == 1);
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_, pref_)  \
    const bool SERVO_PREF_ENABLED_##id_ = !(sizeof(pref_) == 1);
#define CSS_PROP_LIST_INCLUDE_LOGICAL
#include "nsCSSPropList.h"
#undef CSS_PROP_LIST_INCLUDE_LOGICAL
#undef CSS_PROP
#undef CSS_PROP_SHORTHAND

}

#endif // mozilla_ServoPropPrefList_h
