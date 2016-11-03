/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoDeclarationBlock_h
#define mozilla_ServoDeclarationBlock_h

#include "mozilla/ServoBindings.h"
#include "mozilla/DeclarationBlock.h"

namespace mozilla {

class ServoDeclarationBlock final : public DeclarationBlock
{
public:
  ServoDeclarationBlock()
    : ServoDeclarationBlock(Servo_DeclarationBlock_CreateEmpty().Consume()) {}

  NS_INLINE_DECL_REFCOUNTING(ServoDeclarationBlock)

  static already_AddRefed<ServoDeclarationBlock>
  FromStyleAttribute(const nsAString& aString);

  RawServoDeclarationBlock* const* RefRaw() const {
    static_assert(sizeof(RefPtr<RawServoDeclarationBlock>) ==
                  sizeof(RawServoDeclarationBlock*),
                  "RefPtr should just be a pointer");
    return reinterpret_cast<RawServoDeclarationBlock* const*>(&mRaw);
  }

protected:
  explicit ServoDeclarationBlock(
    already_AddRefed<RawServoDeclarationBlock> aRaw)
    : DeclarationBlock(StyleBackendType::Servo), mRaw(aRaw) {}

private:
  ~ServoDeclarationBlock() {}

  RefPtr<RawServoDeclarationBlock> mRaw;
};

} // namespace mozilla

#endif // mozilla_ServoDeclarationBlock_h
