/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=4 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a NPEventX11.h derived stub for Android
// Plugins aren't actually supported yet

#ifndef mozilla_dom_plugins_NPEventAndroid_h
#define mozilla_dom_plugins_NPEventAndroid_h

#include "npapi.h"

namespace mozilla {

namespace plugins {

struct NPRemoteEvent {
    NPEvent event;
};

}

}


namespace IPC {

template <>
struct ParamTraits<mozilla::plugins::NPRemoteEvent>
{
    typedef mozilla::plugins::NPRemoteEvent paramType;

    static void Write(Message* aMsg, const paramType& aParam)
    {
        aMsg->WriteBytes(&aParam, sizeof(paramType));
    }

    static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
    {
        const char* bytes = 0;

        if (!aMsg->ReadBytes(aIter, &bytes, sizeof(paramType))) {
            return false;
        }

        memcpy(aResult, bytes, sizeof(paramType));
        return true;
    }

    static void Log(const paramType& aParam, std::wstring* aLog)
    {
        // TODO
        aLog->append(L"(AndroidEvent)");
    }
};

} // namespace IPC


#endif // mozilla_dom_plugins_NPEventAndroid_h
