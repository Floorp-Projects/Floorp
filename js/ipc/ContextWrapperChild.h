/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_ContextWrapperChild_h__
#define mozilla_jsipc_ContextWrapperChild_h__

#include "mozilla/jsipc/PContextWrapperChild.h"
#include "mozilla/jsipc/ObjectWrapperChild.h"

#include "jsapi.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"

namespace mozilla {
namespace jsipc {

class ContextWrapperChild
    : public PContextWrapperChild
{
public:

    ContextWrapperChild(JSContext* cx)
        : mContext(cx)
    {
        mResidentObjectTable.Init();
    }

    JSContext* GetContext() { return mContext; }

    PObjectWrapperChild* GetOrCreateWrapper(JSObject* obj,
                                            bool makeGlobal = false)
    {
        if (!obj) // Don't wrap nothin'!
            return NULL;
        PObjectWrapperChild* wrapper;
        while (!mResidentObjectTable.Get(obj, &wrapper)) {
            wrapper = SendPObjectWrapperConstructor(AllocPObjectWrapper(obj),
                                                    makeGlobal);
            if (wrapper)
                mResidentObjectTable.Put(obj, wrapper);
            else
                return NULL;
        }
        return wrapper;
    }

protected:

    PObjectWrapperChild* AllocPObjectWrapper(JSObject* obj) {
        return new ObjectWrapperChild(mContext, obj);
    }
    
    PObjectWrapperChild* AllocPObjectWrapper(const bool&) {
        return AllocPObjectWrapper(JS_GetGlobalObject(mContext));
    }

    bool DeallocPObjectWrapper(PObjectWrapperChild* actor) {
        ObjectWrapperChild* owc = static_cast<ObjectWrapperChild*>(actor);
        mResidentObjectTable.Remove(owc->GetJSObject());
        return true;
    }

private:

    JSContext* const mContext;

    nsClassHashtable<nsPtrHashKey<JSObject>,
                     PObjectWrapperChild> mResidentObjectTable;

};

}}

#endif
