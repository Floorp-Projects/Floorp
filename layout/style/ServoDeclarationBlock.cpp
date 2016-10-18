/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoDeclarationBlock.h"

#include "mozilla/ServoBindings.h"

namespace mozilla {

/* static */ already_AddRefed<ServoDeclarationBlock>
ServoDeclarationBlock::FromStyleAttribute(const nsAString& aString)
{
  NS_ConvertUTF16toUTF8 value(aString);
  RefPtr<RawServoDeclarationBlock> raw = Servo_ParseStyleAttribute(
      reinterpret_cast<const uint8_t*>(value.get()), value.Length()).Consume();
  RefPtr<ServoDeclarationBlock> decl = new ServoDeclarationBlock(raw.forget());
  return decl.forget();
}

} // namespace mozilla
