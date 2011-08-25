/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=4 ts=8 et tw=80 ft=cpp : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

    static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
    {
        const char* bytes = 0;

        if (!aMsg->ReadBytes(aIter, &bytes, sizeof(paramType))) {
            return false;
        }

        memcpy(aResult, bytes, sizeof(paramType));

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
