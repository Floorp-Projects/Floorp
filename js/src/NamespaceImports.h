/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file imports some common JS:: names into the js namespace so we can
// make unqualified references to them.

#ifndef NamespaceImports_h
#define NamespaceImports_h

// These includes are needed these for some typedefs (e.g. HandleValue) and
// functions (e.g. NullValue())...
#include "js/CallNonGenericMethod.h"
#include "js/TypeDecls.h"
#include "js/Value.h"

// ... but we do forward declarations of the structs and classes not pulled in
// by the headers included above.
namespace JS {

class Latin1CharsZ;
class StableCharPtr;
class TwoByteChars;

class AutoFunctionVector;
class AutoIdVector;
class AutoObjectVector;
class AutoScriptVector;
class AutoValueVector;

class AutoIdArray;

class AutoGCRooter;
class AutoArrayRooter;
template <typename T> class AutoVectorRooter;
template<typename K, typename V> class AutoHashMapRooter;
template<typename T> class AutoHashSetRooter;

}

// Do the importing.
namespace js {

using JS::Value;
using JS::BooleanValue;
using JS::DoubleValue;
using JS::Float32Value;
using JS::Int32Value;
using JS::IsPoisonedValue;
using JS::MagicValue;
using JS::NullValue;
using JS::NumberValue;
using JS::ObjectOrNullValue;
using JS::ObjectValue;
using JS::PrivateUint32Value;
using JS::PrivateValue;
using JS::StringValue;
using JS::UndefinedValue;

using JS::IsPoisonedPtr;

using JS::Latin1CharsZ;
using JS::StableCharPtr;
using JS::TwoByteChars;

using JS::AutoFunctionVector;
using JS::AutoIdVector;
using JS::AutoObjectVector;
using JS::AutoScriptVector;
using JS::AutoValueVector;

using JS::AutoIdArray;

using JS::AutoGCRooter;
using JS::AutoArrayRooter;
using JS::AutoHashMapRooter;
using JS::AutoHashSetRooter;
using JS::AutoVectorRooter;

using JS::CallArgs;
using JS::CallNonGenericMethod;
using JS::CallReceiver;
using JS::CompileOptions;
using JS::IsAcceptableThis;
using JS::NativeImpl;

using JS::Rooted;
using JS::RootedFunction;
using JS::RootedId;
using JS::RootedObject;
using JS::RootedScript;
using JS::RootedString;
using JS::RootedValue;

using JS::Handle;
using JS::HandleFunction;
using JS::HandleId;
using JS::HandleObject;
using JS::HandleScript;
using JS::HandleString;
using JS::HandleValue;

using JS::MutableHandle;
using JS::MutableHandleFunction;
using JS::MutableHandleId;
using JS::MutableHandleObject;
using JS::MutableHandleScript;
using JS::MutableHandleString;
using JS::MutableHandleValue;

using JS::Zone;

} /* namespace js */

#endif /* NamespaceImports_h */
