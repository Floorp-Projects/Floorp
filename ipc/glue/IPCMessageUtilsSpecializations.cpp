/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCMessageUtilsSpecializations.h"
#include "nsGkAtoms.h"

namespace IPC {

static const uint16_t kDynamicAtomToken = 0xffff;
static const uint16_t kAtomsCount =
    static_cast<uint16_t>(mozilla::detail::GkAtoms::Atoms::AtomsCount);

static_assert(static_cast<size_t>(
                  mozilla::detail::GkAtoms::Atoms::AtomsCount) == kAtomsCount,
              "Number of static atoms must fit in a uint16_t");

static_assert(kDynamicAtomToken >= kAtomsCount,
              "Exceeded supported number of static atoms");

/* static */
void ParamTraits<nsAtom*>::Write(Message* aMsg, const nsAtom* aParam) {
  MOZ_ASSERT(aParam);

  if (aParam->IsStatic()) {
    const nsStaticAtom* atom = aParam->AsStatic();
    uint16_t index = static_cast<uint16_t>(nsGkAtoms::IndexOf(atom));
    MOZ_ASSERT(index < kAtomsCount);
    WriteParam(aMsg, index);
    return;
  }
  WriteParam(aMsg, kDynamicAtomToken);
  nsDependentAtomString atomStr(aParam);
  // nsDependentAtomString is serialized as its base, nsString, but we
  // can be explicit about it.
  nsString& str = atomStr;
  WriteParam(aMsg, str);
}

/* static */
bool ParamTraits<nsAtom*>::Read(const Message* aMsg, PickleIterator* aIter,
                                RefPtr<nsAtom>* aResult) {
  uint16_t token;
  if (!ReadParam(aMsg, aIter, &token)) {
    return false;
  }
  if (token != kDynamicAtomToken) {
    if (token >= kAtomsCount) {
      return false;
    }
    *aResult = nsGkAtoms::GetAtomByIndex(token);
    return true;
  }

  nsAutoString str;
  if (!ReadParam(aMsg, aIter, static_cast<nsString*>(&str))) {
    return false;
  }

  *aResult = NS_Atomize(str);
  MOZ_ASSERT(*aResult);
  return true;
}

}  // namespace IPC
