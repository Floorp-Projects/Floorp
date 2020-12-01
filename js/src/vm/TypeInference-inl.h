/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Inline members for javascript type inference. */

#ifndef vm_TypeInference_inl_h
#define vm_TypeInference_inl_h

#include "vm/TypeInference.h"

#include "mozilla/BinarySearch.h"
#include "mozilla/Casting.h"
#include "mozilla/PodOperations.h"

#include <utility>  // for ::std::swap

#include "builtin/Symbol.h"
#include "gc/GC.h"
#include "jit/BaselineJIT.h"
#include "jit/IonScript.h"
#include "jit/JitScript.h"
#include "jit/JitZone.h"
#include "js/HeapAPI.h"
#include "util/DiagnosticAssertions.h"
#include "vm/ArrayObject.h"
#include "vm/BooleanObject.h"
#include "vm/JSFunction.h"
#include "vm/NativeObject.h"
#include "vm/NumberObject.h"
#include "vm/ObjectGroup.h"
#include "vm/PlainObject.h"  // js::PlainObject
#include "vm/Shape.h"
#include "vm/SharedArrayObject.h"
#include "vm/StringObject.h"
#include "vm/TypedArrayObject.h"

#include "jit/JitScript-inl.h"
#include "vm/JSContext-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/ObjectGroup-inl.h"

#endif /* vm_TypeInference_inl_h */
