/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DeclarationBlock.h"

#include "mozilla/css/Rule.h"
#include "mozilla/ServoBindings.h"

#include "nsCSSProps.h"
#include "nsIMemoryReporter.h"

namespace mozilla {

/* static */
already_AddRefed<DeclarationBlock> DeclarationBlock::FromCssText(
    const nsAString& aCssText, URLExtraData* aExtraData, nsCompatibility aMode,
    css::Loader* aLoader) {
  NS_ConvertUTF16toUTF8 value(aCssText);
  RefPtr<RawServoDeclarationBlock> raw =
      Servo_ParseStyleAttribute(&value, aExtraData, aMode, aLoader).Consume();
  RefPtr<DeclarationBlock> decl = new DeclarationBlock(raw.forget());
  return decl.forget();
}

MOZ_DEFINE_MALLOC_SIZE_OF(ServoDeclarationBlockMallocSizeOf)
MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoDeclarationBlockEnclosingSizeOf)

size_t DeclarationBlock::SizeofIncludingThis(MallocSizeOf aMallocSizeOf) {
  size_t n = aMallocSizeOf(this);
  n += Servo_DeclarationBlock_SizeOfIncludingThis(
      ServoDeclarationBlockMallocSizeOf, ServoDeclarationBlockEnclosingSizeOf,
      mRaw.get());
  return n;
}

bool DeclarationBlock::OwnerIsReadOnly() const {
  css::Rule* rule = GetOwningRule();
  return rule && rule->IsReadOnly();
}

}  // namespace mozilla
