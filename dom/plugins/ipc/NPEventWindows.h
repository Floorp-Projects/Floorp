/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=4 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_plugins_NPEventWindows_h
#define mozilla_dom_plugins_NPEventWindows_h 1


#include "npapi.h"
namespace mozilla {

namespace plugins {

// We use an NPRemoteEvent struct so that we can store the extra data on
// the stack so that we don't need to worry about managing the memory.
struct NPRemoteEvent
{
    NPEvent event;
    union {
        RECT rect;
        WINDOWPOS windowpos;
    } lParamData;
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
        // Make a non-const copy of aParam so that we can muck with
        // its insides for tranport
        paramType paramCopy;

        paramCopy.event = aParam.event;

        // We can't blindly ipc events because they may sometimes contain
        // pointers to memory in the sending process. For example, the
        // WM_IME_CONTROL with the IMC_GETCOMPOSITIONFONT message has lParam
        // set to a pointer to a LOGFONT structure.
        switch (paramCopy.event.event) {
            case WM_WINDOWPOSCHANGED:
                // The lParam parameter of WM_WINDOWPOSCHANGED holds a pointer to
                // a WINDOWPOS structure that contains information about the
                // window's new size and position
                paramCopy.lParamData.windowpos = *(reinterpret_cast<WINDOWPOS*>(paramCopy.event.lParam));
                break;
            case WM_PAINT:
                // The lParam parameter of WM_PAINT holds a pointer to an RECT
                // structure specifying the bounding box of the update area.
                paramCopy.lParamData.rect = *(reinterpret_cast<RECT*>(paramCopy.event.lParam));
                break;

            // the white list of events that we will ipc to the client
            case WM_CHAR:
            case WM_SYSCHAR:

            case WM_KEYUP:
            case WM_SYSKEYUP:

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:

            case WM_DEADCHAR:
            case WM_SYSDEADCHAR:
            case WM_CONTEXTMENU:

            case WM_CUT:
            case WM_COPY:
            case WM_PASTE:
            case WM_CLEAR:
            case WM_UNDO:

            case WM_MOUSELEAVE:
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_MBUTTONDBLCLK:
            case WM_RBUTTONDBLCLK:

            case WM_SETFOCUS:
            case WM_KILLFOCUS:
                break;

            default:
                // RegisterWindowMessage events should be passed.
                if (paramCopy.event.event >= 0xC000 && paramCopy.event.event <= 0xFFFF)
                    break;

                // FIXME/bug 567465: temporarily work around unhandled
                // events by forwarding a "dummy event".  The eventual
                // fix will be to stop trying to send these events
                // entirely.
                paramCopy.event.event = WM_NULL;
                break;
        }

        aMsg->WriteBytes(&paramCopy, sizeof(paramType));
    }

    static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
    {
        const char* bytes = 0;

        if (!aMsg->ReadBytes(aIter, &bytes, sizeof(paramType))) {
            return false;
        }
        memcpy(aResult, bytes, sizeof(paramType));

        if (aResult->event.event == WM_PAINT) {
            // restore the lParam to point at the RECT
            aResult->event.lParam = reinterpret_cast<LPARAM>(&aResult->lParamData.rect);
        } else if (aResult->event.event == WM_WINDOWPOSCHANGED) {
            // restore the lParam to point at the WINDOWPOS
            aResult->event.lParam = reinterpret_cast<LPARAM>(&aResult->lParamData.windowpos);
        }

        return true;
    }

    static void Log(const paramType& aParam, std::wstring* aLog)
    {
        aLog->append(L"(WINEvent)");
    }

};

} // namespace IPC

#endif // ifndef mozilla_dom_plugins_NPEventWindows_h
