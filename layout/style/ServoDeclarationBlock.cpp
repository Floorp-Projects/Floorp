/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoDeclarationBlock.h"

#include "mozilla/ServoBindings.h"

namespace mozilla {

/* static */ already_AddRefed<ServoDeclarationBlock>
ServoDeclarationBlock::FromCssText(const nsAString& aCssText)
{
  NS_ConvertUTF16toUTF8 value(aCssText);
  RefPtr<RawServoDeclarationBlock>
    raw = Servo_ParseStyleAttribute(&value).Consume();
  RefPtr<ServoDeclarationBlock> decl = new ServoDeclarationBlock(raw.forget());
  return decl.forget();
}

} // namespace mozilla
