/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ErrorObject.h"

#include "vm/GlobalObject.h"

#include "jsobjinlines.h"

#include "vm/Shape-inl.h"

using namespace js;

/* static */ Shape *
js::ErrorObject::assignInitialShape(ExclusiveContext *cx, Handle<ErrorObject*> obj)
{
    MOZ_ASSERT(obj->nativeEmpty());

    if (!obj->addDataProperty(cx, cx->names().fileName, FILENAME_SLOT, 0))
        return nullptr;
    if (!obj->addDataProperty(cx, cx->names().lineNumber, LINENUMBER_SLOT, 0))
        return nullptr;
    if (!obj->addDataProperty(cx, cx->names().columnNumber, COLUMNNUMBER_SLOT, 0))
        return nullptr;
    return obj->addDataProperty(cx, cx->names().stack, STACK_SLOT, 0);
}

/* static */ bool
js::ErrorObject::init(JSContext *cx, Handle<ErrorObject*> obj, JSExnType type,
                      ScopedJSFreePtr<JSErrorReport> *errorReport, HandleString fileName,
                      HandleString stack, uint32_t lineNumber, uint32_t columnNumber,
                      HandleString message)
{
    // Null out early in case of error, for exn_finalize's sake.
    obj->initReservedSlot(ERROR_REPORT_SLOT, PrivateValue(nullptr));

    if (!EmptyShape::ensureInitialCustomShape<ErrorObject>(cx, obj))
        return false;

    // The .message property isn't part of the initial shape because it's
    // present in some error objects -- |Error.prototype|, |new Error("f")|,
    // |new Error("")| -- but not in others -- |new Error(undefined)|,
    // |new Error()|.
    RootedShape messageShape(cx);
    if (message) {
        messageShape = obj->addDataProperty(cx, cx->names().message, MESSAGE_SLOT, 0);
        if (!messageShape)
            return false;
        MOZ_ASSERT(messageShape->slot() == MESSAGE_SLOT);
    }

    MOZ_ASSERT(obj->nativeLookupPure(NameToId(cx->names().fileName))->slot() == FILENAME_SLOT);
    MOZ_ASSERT(obj->nativeLookupPure(NameToId(cx->names().lineNumber))->slot() == LINENUMBER_SLOT);
    MOZ_ASSERT(obj->nativeLookupPure(NameToId(cx->names().columnNumber))->slot() ==
               COLUMNNUMBER_SLOT);
    MOZ_ASSERT(obj->nativeLookupPure(NameToId(cx->names().stack))->slot() == STACK_SLOT);
    MOZ_ASSERT_IF(message,
                  obj->nativeLookupPure(NameToId(cx->names().message))->slot() == MESSAGE_SLOT);

    MOZ_ASSERT(JSEXN_ERR <= type && type < JSEXN_LIMIT);

    JSErrorReport *report = errorReport ? errorReport->forget() : nullptr;
    obj->initReservedSlot(EXNTYPE_SLOT, Int32Value(type));
    obj->setReservedSlot(ERROR_REPORT_SLOT, PrivateValue(report));
    obj->initReservedSlot(FILENAME_SLOT, StringValue(fileName));
    obj->initReservedSlot(LINENUMBER_SLOT, Int32Value(lineNumber));
    obj->initReservedSlot(COLUMNNUMBER_SLOT, Int32Value(columnNumber));
    obj->initReservedSlot(STACK_SLOT, StringValue(stack));
    if (message)
        obj->nativeSetSlotWithType(cx, messageShape, StringValue(message));

    if (report && report->originPrincipals)
        JS_HoldPrincipals(report->originPrincipals);

    return true;
}

/* static */ ErrorObject *
js::ErrorObject::create(JSContext *cx, JSExnType errorType, HandleString stack,
                        HandleString fileName, uint32_t lineNumber, uint32_t columnNumber,
                        ScopedJSFreePtr<JSErrorReport> *report, HandleString message)
{
    Rooted<JSObject*> proto(cx, cx->global()->getOrCreateCustomErrorPrototype(cx, errorType));
    if (!proto)
        return nullptr;

    Rooted<ErrorObject*> errObject(cx);
    {
        JSObject* obj = NewObjectWithGivenProto(cx, &ErrorObject::class_, proto, nullptr);
        if (!obj)
            return nullptr;
        errObject = &obj->as<ErrorObject>();
    }

    if (!ErrorObject::init(cx, errObject, errorType, report, fileName, stack,
                           lineNumber, columnNumber, message))
    {
        return nullptr;
    }

    return errObject;
}
