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
                // The lParam paramter of WM_WINDOWPOSCHANGED holds a pointer to
                // a WINDOWPOS structure that contains information about the
                // window's new size and position
                paramCopy.lParamData.windowpos = *(reinterpret_cast<WINDOWPOS*>(paramCopy.event.lParam));
                break;
            case WM_PAINT:
                // The lParam paramter of WM_PAINT holds a pointer to an RECT
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

                // ignore any events we don't expect
                return;
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
