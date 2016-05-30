/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=4 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_plugins_NPEventOSX_h
#define mozilla_dom_plugins_NPEventOSX_h 1


#include "npapi.h"

namespace mozilla {

namespace plugins {

struct NPRemoteEvent {
    NPCocoaEvent event;
    double contentsScaleFactor;
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
        aMsg->WriteInt(aParam.event.type);
        aMsg->WriteUInt32(aParam.event.version);
        switch (aParam.event.type) {
            case NPCocoaEventMouseDown:
            case NPCocoaEventMouseUp:
            case NPCocoaEventMouseMoved:
            case NPCocoaEventMouseEntered:
            case NPCocoaEventMouseExited:
            case NPCocoaEventMouseDragged:
            case NPCocoaEventScrollWheel:
                aMsg->WriteUInt32(aParam.event.data.mouse.modifierFlags);
                aMsg->WriteDouble(aParam.event.data.mouse.pluginX);
                aMsg->WriteDouble(aParam.event.data.mouse.pluginY);
                aMsg->WriteInt32(aParam.event.data.mouse.buttonNumber);
                aMsg->WriteInt32(aParam.event.data.mouse.clickCount);
                aMsg->WriteDouble(aParam.event.data.mouse.deltaX);
                aMsg->WriteDouble(aParam.event.data.mouse.deltaY);
                aMsg->WriteDouble(aParam.event.data.mouse.deltaZ);
                break;
            case NPCocoaEventKeyDown:
            case NPCocoaEventKeyUp:
            case NPCocoaEventFlagsChanged:
                aMsg->WriteUInt32(aParam.event.data.key.modifierFlags);
                WriteParam(aMsg, aParam.event.data.key.characters);
                WriteParam(aMsg, aParam.event.data.key.charactersIgnoringModifiers);
                aMsg->WriteUnsignedChar(aParam.event.data.key.isARepeat);
                aMsg->WriteUInt16(aParam.event.data.key.keyCode);
                break;
            case NPCocoaEventFocusChanged:
            case NPCocoaEventWindowFocusChanged:
                aMsg->WriteUnsignedChar(aParam.event.data.focus.hasFocus);
                break;
            case NPCocoaEventDrawRect:
                // We don't write out the context pointer, it would always be
                // nullptr and is just filled in as such on the read.
                aMsg->WriteDouble(aParam.event.data.draw.x);
                aMsg->WriteDouble(aParam.event.data.draw.y);
                aMsg->WriteDouble(aParam.event.data.draw.width);
                aMsg->WriteDouble(aParam.event.data.draw.height);
                break;
            case NPCocoaEventTextInput:
                WriteParam(aMsg, aParam.event.data.text.text);
                break;
            default:
                NS_NOTREACHED("Attempted to serialize unknown event type.");
                return;
        }
        aMsg->WriteDouble(aParam.contentsScaleFactor);
    }

    static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
    {
        int type = 0;
        if (!aMsg->ReadInt(aIter, &type)) {
            return false;
        }
        aResult->event.type = static_cast<NPCocoaEventType>(type);

        if (!aMsg->ReadUInt32(aIter, &aResult->event.version)) {
            return false;
        }

        switch (aResult->event.type) {
            case NPCocoaEventMouseDown:
            case NPCocoaEventMouseUp:
            case NPCocoaEventMouseMoved:
            case NPCocoaEventMouseEntered:
            case NPCocoaEventMouseExited:
            case NPCocoaEventMouseDragged:
            case NPCocoaEventScrollWheel:
                if (!aMsg->ReadUInt32(aIter, &aResult->event.data.mouse.modifierFlags)) {
                    return false;
                }
                if (!aMsg->ReadDouble(aIter, &aResult->event.data.mouse.pluginX)) {
                    return false;
                }
                if (!aMsg->ReadDouble(aIter, &aResult->event.data.mouse.pluginY)) {
                    return false;
                }
                if (!aMsg->ReadInt32(aIter, &aResult->event.data.mouse.buttonNumber)) {
                    return false;
                }
                if (!aMsg->ReadInt32(aIter, &aResult->event.data.mouse.clickCount)) {
                    return false;
                }
                if (!aMsg->ReadDouble(aIter, &aResult->event.data.mouse.deltaX)) {
                    return false;
                }
                if (!aMsg->ReadDouble(aIter, &aResult->event.data.mouse.deltaY)) {
                    return false;
                }
                if (!aMsg->ReadDouble(aIter, &aResult->event.data.mouse.deltaZ)) {
                    return false;
                }
                break;
            case NPCocoaEventKeyDown:
            case NPCocoaEventKeyUp:
            case NPCocoaEventFlagsChanged:
                if (!aMsg->ReadUInt32(aIter, &aResult->event.data.key.modifierFlags)) {
                    return false;
                }
                if (!ReadParam(aMsg, aIter, &aResult->event.data.key.characters)) {
                    return false;
                }
                if (!ReadParam(aMsg, aIter, &aResult->event.data.key.charactersIgnoringModifiers)) {
                    return false;
                }
                if (!aMsg->ReadUnsignedChar(aIter, &aResult->event.data.key.isARepeat)) {
                    return false;
                }
                if (!aMsg->ReadUInt16(aIter, &aResult->event.data.key.keyCode)) {
                    return false;
                }
                break;
            case NPCocoaEventFocusChanged:
            case NPCocoaEventWindowFocusChanged:
                if (!aMsg->ReadUnsignedChar(aIter, &aResult->event.data.focus.hasFocus)) {
                    return false;
                }
                break;
            case NPCocoaEventDrawRect:
                aResult->event.data.draw.context = nullptr;
                if (!aMsg->ReadDouble(aIter, &aResult->event.data.draw.x)) {
                    return false;
                }
                if (!aMsg->ReadDouble(aIter, &aResult->event.data.draw.y)) {
                    return false;
                }
                if (!aMsg->ReadDouble(aIter, &aResult->event.data.draw.width)) {
                    return false;
                }
                if (!aMsg->ReadDouble(aIter, &aResult->event.data.draw.height)) {
                    return false;
                }
                break;
            case NPCocoaEventTextInput:
                if (!ReadParam(aMsg, aIter, &aResult->event.data.text.text)) {
                    return false;
                }
                break;
            default:
                NS_NOTREACHED("Attempted to de-serialize unknown event type.");
                return false;
        }
        if (!aMsg->ReadDouble(aIter, &aResult->contentsScaleFactor)) {
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
