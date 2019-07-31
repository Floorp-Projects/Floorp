/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/TIOracle.h"

#include "jit/IonBuilder.h"
#include "vm/TypedArrayObject.h"

#include "vm/JSScript-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::jit;

using ObjectKey = js::TypeSet::ObjectKey;

TIOracle::TIOracle(IonBuilder* builder, CompilerConstraintList* constraints)
    : builder_(builder), constraints_(constraints) {}

TIOracle::Group::Group(JSObject* raw) : raw_(ObjectKey::get(raw)) {}
TIOracle::Group::Group(ObjectGroup* raw) : raw_(ObjectKey::get(raw)) {}

AbortReasonOr<Ok> TIOracle::ensureBallast() {
  if (!builder_->alloc().ensureBallast()) {
    return builder_->abort(AbortReason::Alloc);
  }
  return Ok();
}

TIOracle::TypeSet TIOracle::resultTypeSet(MDefinition* def) {
  return TIOracle::TypeSet(def->resultTypeSet());
}

TIOracle::Object TIOracle::wrapObject(JSObject* object) {
  return TIOracle::Object(builder_->checkNurseryObject(object));
}

bool TIOracle::hasStableClass(TIOracle::Object obj) {
  ObjectKey* key = ObjectKey::get(obj.toRaw());
  return key->hasStableClassAndProto(constraints_);
}

bool TIOracle::hasStableProto(TIOracle::Object obj) {
  ObjectKey* key = ObjectKey::get(obj.toRaw());
  return key->hasStableClassAndProto(constraints_);
}
