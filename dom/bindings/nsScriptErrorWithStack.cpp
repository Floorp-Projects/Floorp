/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsScriptErrorWithStack implementation.
 * a main-thread-only, cycle-collected subclass of nsScriptErrorBase
 * that can store a SavedFrame stack trace object.
 */

#include "nsScriptError.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsGlobalWindow.h"
#include "nsCycleCollectionParticipant.h"

using namespace mozilla::dom;

namespace {

static nsCString
FormatStackString(JSContext* cx, JS::HandleObject aStack) {
    JS::RootedString formattedStack(cx);

    if (!JS::BuildStackString(cx, aStack, &formattedStack)) {
        return nsCString();
    }

    nsAutoJSString stackJSString;
    if (!stackJSString.init(cx, formattedStack)) {
        return nsCString();
    }

    return NS_ConvertUTF16toUTF8(stackJSString.get());
}

}


NS_IMPL_CYCLE_COLLECTION_CLASS(nsScriptErrorWithStack)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsScriptErrorWithStack)
  tmp->mStack = nullptr;
  tmp->mStackGlobal = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsScriptErrorWithStack)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsScriptErrorWithStack)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mStack)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mStackGlobal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsScriptErrorWithStack)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsScriptErrorWithStack)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsScriptErrorWithStack)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIConsoleMessage)
  NS_INTERFACE_MAP_ENTRY(nsIScriptError)
NS_INTERFACE_MAP_END

nsScriptErrorWithStack::nsScriptErrorWithStack(JS::HandleObject aStack,
                                               JS::HandleObject aStackGlobal)
  : mStack(aStack)
  , mStackGlobal(aStackGlobal)
{
    MOZ_ASSERT(NS_IsMainThread(), "You can't use this class on workers.");

    MOZ_ASSERT(JS_IsGlobalObject(mStackGlobal));
    js::AssertSameCompartment(mStack, mStackGlobal);

    mozilla::HoldJSObjects(this);
}

nsScriptErrorWithStack::~nsScriptErrorWithStack() {
    mozilla::DropJSObjects(this);
}

NS_IMETHODIMP
nsScriptErrorWithStack::GetStack(JS::MutableHandleValue aStack) {
    aStack.setObjectOrNull(mStack);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorWithStack::ToString(nsACString& /*UTF8*/ aResult)
{
    MOZ_ASSERT(NS_IsMainThread());

    nsCString message;
    nsresult rv = nsScriptErrorBase::ToString(message);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mStack) {
        aResult.Assign(message);
        return NS_OK;
    }

    AutoJSAPI jsapi;
    if (!jsapi.Init(mStackGlobal)) {
        return NS_ERROR_FAILURE;
    }

    JSContext* cx = jsapi.cx();
    JS::RootedObject stack(cx, mStack);
    nsCString stackString = FormatStackString(cx, stack);
    nsCString combined = message + NS_LITERAL_CSTRING("\n") + stackString;
    aResult.Assign(combined);

    return NS_OK;
}
