/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Rooting_h
#define gc_Rooting_h

#include "gc/Allocator.h"
#include "gc/Policy.h"
#include "js/GCVector.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

class JSLinearString;

namespace js {

class PropertyName;
class NativeObject;
class ArrayObject;
class GlobalObject;
class PlainObject;
class ScriptSourceObject;
class Shape;
class Scope;

// These are internal counterparts to the public types such as HandleObject.

using HandleNativeObject = JS::Handle<NativeObject*>;
using HandleShape = JS::Handle<Shape*>;
using HandleAtom = JS::Handle<JSAtom*>;
using HandleLinearString = JS::Handle<JSLinearString*>;
using HandlePropertyName = JS::Handle<PropertyName*>;
using HandleArrayObject = JS::Handle<ArrayObject*>;
using HandlePlainObject = JS::Handle<PlainObject*>;
using HandleScriptSourceObject = JS::Handle<ScriptSourceObject*>;
using HandleScope = JS::Handle<Scope*>;

using MutableHandleShape = JS::MutableHandle<Shape*>;
using MutableHandleAtom = JS::MutableHandle<JSAtom*>;
using MutableHandleNativeObject = JS::MutableHandle<NativeObject*>;
using MutableHandlePlainObject = JS::MutableHandle<PlainObject*>;
using MutableHandleScope = JS::MutableHandle<Scope*>;
using MutableHandleArrayObject = JS::MutableHandle<ArrayObject*>;

using RootedNativeObject = JS::Rooted<NativeObject*>;
using RootedShape = JS::Rooted<Shape*>;
using RootedAtom = JS::Rooted<JSAtom*>;
using RootedLinearString = JS::Rooted<JSLinearString*>;
using RootedPropertyName = JS::Rooted<PropertyName*>;
using RootedArrayObject = JS::Rooted<ArrayObject*>;
using RootedGlobalObject = JS::Rooted<GlobalObject*>;
using RootedPlainObject = JS::Rooted<PlainObject*>;
using RootedScriptSourceObject = JS::Rooted<ScriptSourceObject*>;
using RootedScope = JS::Rooted<Scope*>;

using FunctionVector = JS::GCVector<JSFunction*>;
using PropertyNameVector = JS::GCVector<PropertyName*>;
using ShapeVector = JS::GCVector<Shape*>;
using StringVector = JS::GCVector<JSString*>;

} /* namespace js */

#endif /* gc_Rooting_h */
