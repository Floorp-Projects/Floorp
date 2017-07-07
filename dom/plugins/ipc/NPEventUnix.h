/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=4 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_plugins_NPEventUnix_h
#define mozilla_dom_plugins_NPEventUnix_h 1

#include "npapi.h"

#ifdef MOZ_X11
#include "mozilla/X11Util.h"
#endif

namespace mozilla {

namespace plugins {

struct NPRemoteEvent {
    NPEvent event;
};

}

}


//
// XEvent is defined as a union of all more specific X*Events.
// Luckily, as of xorg 1.6.0 / X protocol 11 rev 0, the only pointer
// field contained in any of these specific X*Event structs is a
// |Display*|.  So to simplify serializing these XEvents, we make the
//
// ********** XXX ASSUMPTION XXX **********
//
// that the process to which the event is forwarded shares the same
// display as the process on which the event originated.
//
// With this simplification, serialization becomes a simple memcpy to
// the output stream.  Deserialization starts as just a memcpy from
// the input stream, BUT we then have to write the correct |Display*|
// into the right field of each X*Event that contains one.
//

namespace IPC {

template <>
struct ParamTraits<mozilla::plugins::NPRemoteEvent>     // synonym for XEvent
{
    typedef mozilla::plugins::NPRemoteEvent paramType;

    static void Write(Message* aMsg, const paramType& aParam)
    {
        aMsg->WriteBytes(&aParam, sizeof(paramType));
    }

    static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
    {
        if (!aMsg->ReadBytesInto(aIter, aResult, sizeof(paramType))) {
            return false;
        }

#ifdef MOZ_X11
        SetXDisplay(aResult->event);
#endif
        return true;
    }

    static void Log(const paramType& aParam, std::wstring* aLog)
    {
        // TODO
        aLog->append(L"(XEvent)");
    }

#ifdef MOZ_X11
private:
    static void SetXDisplay(XEvent& ev)
    {
        Display* display = mozilla::DefaultXDisplay();
        if (ev.type >= KeyPress) {
            ev.xany.display = display;
        }
        else {
            // XXX assuming that this is an error event
            // (type == 0? not clear from Xlib.h)
            ev.xerror.display = display;
        }
    }
#endif
};

} // namespace IPC


#endif // ifndef mozilla_dom_plugins_NPEventX11_h
