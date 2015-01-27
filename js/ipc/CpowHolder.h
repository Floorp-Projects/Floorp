/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_CpowHolder_h__
#define mozilla_jsipc_CpowHolder_h__

#include "js/TypeDecls.h"

namespace mozilla {
namespace jsipc {

class CpowHolder
{
  public:
    virtual bool ToObject(JSContext* cx, JS::MutableHandle<JSObject*> objp) = 0;
};

} // namespace jsipc
} // namespace mozilla

#endif // mozilla_jsipc_CpowHolder_h__
