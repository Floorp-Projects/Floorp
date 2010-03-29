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

#ifndef mozilla_dom_plugins_NPEventOSX_h
#define mozilla_dom_plugins_NPEventOSX_h 1


#include "npapi.h"

namespace mozilla {

namespace plugins {

struct NPRemoteEvent {
    NPCocoaEvent event;
};

} // namespace plugins

} // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::plugins::NPRemoteEvent>
{
    typedef mozilla::plugins::NPRemoteEvent paramType;

    static void Write(Message* aMsg, const paramType& aParam)
    {
        // Make a non-const copy of aParam so that we can muck with
        // its insides for transport
        paramType paramCopy;

        paramCopy.event = aParam.event;

        switch (paramCopy.event.type) {
            case NPCocoaEventMouseDown:
            case NPCocoaEventMouseUp:
            case NPCocoaEventMouseMoved:
            case NPCocoaEventMouseEntered:
            case NPCocoaEventMouseExited:
            case NPCocoaEventMouseDragged:
            case NPCocoaEventFocusChanged:
            case NPCocoaEventWindowFocusChanged:
            case NPCocoaEventScrollWheel:
                aMsg->WriteBytes(&paramCopy, sizeof(paramType));
                return;
            case NPCocoaEventDrawRect:
                paramCopy.event.data.draw.context = NULL;
                aMsg->WriteBytes(&paramCopy, sizeof(paramType));
                return;
            case NPCocoaEventFlagsChanged:
                paramCopy.event.data.key.characters = NULL;
                paramCopy.event.data.key.charactersIgnoringModifiers = NULL;
                aMsg->WriteBytes(&paramCopy, sizeof(paramType));
                return;
            case NPCocoaEventKeyDown:
            case NPCocoaEventKeyUp:
                paramCopy.event.data.key.characters = NULL;
                paramCopy.event.data.key.charactersIgnoringModifiers = NULL;
                aMsg->WriteBytes(&paramCopy, sizeof(paramType));
                WriteParam(aMsg, aParam.event.data.key.characters);
                WriteParam(aMsg, aParam.event.data.key.charactersIgnoringModifiers);
                return;
            case NPCocoaEventTextInput:
                paramCopy.event.data.text.text = NULL;
                aMsg->WriteBytes(&paramCopy, sizeof(paramType));
                WriteParam(aMsg, aParam.event.data.text.text);
                return;
            default:
                NS_NOTREACHED("Attempted to serialize unknown event type.");
                return;
        }
    }

    static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
    {
        const char* bytes = 0;

        if (!aMsg->ReadBytes(aIter, &bytes, sizeof(paramType))) {
            return false;
        }
        memcpy(aResult, bytes, sizeof(paramType));

        switch (aResult->event.type) {
            case NPCocoaEventMouseDown:
            case NPCocoaEventMouseUp:
            case NPCocoaEventMouseMoved:
            case NPCocoaEventMouseEntered:
            case NPCocoaEventMouseExited:
            case NPCocoaEventMouseDragged:
            case NPCocoaEventFocusChanged:
            case NPCocoaEventWindowFocusChanged:
            case NPCocoaEventScrollWheel:
            case NPCocoaEventDrawRect:
            case NPCocoaEventFlagsChanged:
                break;
            case NPCocoaEventKeyDown:
            case NPCocoaEventKeyUp:
                if (!ReadParam(aMsg, aIter, &aResult->event.data.key.characters) ||
                    !ReadParam(aMsg, aIter, &aResult->event.data.key.charactersIgnoringModifiers)) {
                  return false;
                }
                break;
            case NPCocoaEventTextInput:
                if (!ReadParam(aMsg, aIter, &aResult->event.data.text.text)) {
                    return false;
                }
                break;
            default:
                // ignore any events we don't expect
                return false;
        }

        return true;
    }

    static void Log(const paramType& aParam, std::wstring* aLog)
    {
        aLog->append(L"(NPCocoaEvent)");
    }
};

} // namespace IPC

#endif // ifndef mozilla_dom_plugins_NPEventOSX_h
