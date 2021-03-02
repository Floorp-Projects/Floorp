/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _Accessible_H_
#define _Accessible_H_

#include <stdint.h>
#include "mozilla/a11y/Role.h"
#include "mozilla/a11y/AccTypes.h"

class nsAtom;

struct nsRoleMapEntry;

namespace mozilla {
namespace a11y {

class Accessible {
 protected:
  Accessible();

 public:
  /**
   * Return ARIA role map if any.
   */
  const nsRoleMapEntry* ARIARoleMap() const;

  /**
   * Return true if ARIA role is specified on the element.
   */
  bool HasARIARole() const;
  bool IsARIARole(nsAtom* aARIARole) const;
  bool HasStrongARIARole() const;

  /**
   * Return true if the accessible belongs to the given accessible type.
   */
  bool HasGenericType(AccGenericType aType) const;

 private:
  static const uint8_t kTypeBits = 6;
  static const uint8_t kGenericTypesBits = 18;

  void StaticAsserts() const;

 protected:
  uint32_t mType : kTypeBits;
  uint32_t mGenericTypes : kGenericTypesBits;
  uint8_t mRoleMapEntryIndex;
};

}  // namespace a11y
}  // namespace mozilla

#endif
