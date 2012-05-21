/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_ContextWrapperTypes_h__
#define mozilla_jsipc_ContextWrapperTypes_h__

#include "jsapi.h"
#include "jspubtd.h"

using mozilla::void_t;

namespace mozilla {
namespace jsipc {

using namespace IPC;

template <typename P>
struct CPOWSingleton
{
    static void Write(Message*, const P&) {}
    static bool Read(const Message*, void**, P*) { return true; }
};

template <typename Type, typename As>
struct CPOWConvertible
{
    static void Write(Message* m, const Type& t) {
        WriteParam(m, As(t));
    }
    static bool Read(const Message* m, void** iter, Type* tp) {
        As a;
        return (ReadParam(m, iter, &a) &&
                (*tp = Type(a), true));
    }
}; 

} // namespace jsipc
} // namespace mozilla

namespace IPC {

using namespace mozilla::jsipc;

template <> struct ParamTraits<JSType> : public CPOWConvertible<JSType, int> {};

}

// TODO Use a more standard logging mechanism.
#ifdef LOGGING
#define CPOW_LOG(PRINTF_ARGS) \
    JS_BEGIN_MACRO            \
    printf("CPOW | ");        \
    printf PRINTF_ARGS ;      \
    printf("\n");             \
    JS_END_MACRO
#define JSVAL_TO_CSTR(CX, V) \
    NS_ConvertUTF16toUTF8(nsString(JS_GetStringChars(JS_ValueToString(CX, V)))).get()
#else
#define CPOW_LOG(_) JS_BEGIN_MACRO JS_END_MACRO
#define JSVAL_TO_CSTR(CX, V) ((char*)0)
#endif

#endif
