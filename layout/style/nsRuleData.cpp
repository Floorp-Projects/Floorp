/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsRuleData.h"
#include "nsCSSProps.h"
#include "nsPresArena.h"

#include "mozilla/StandardInteger.h"

inline size_t
nsRuleData::GetPoisonOffset()
{
  // Fill in mValueOffsets such that mValueStorage + mValueOffsets[i]
  // will yield the frame poison value for all uninitialized value
  // offsets.
  MOZ_STATIC_ASSERT(sizeof(uintptr_t) == sizeof(size_t),
                    "expect uintptr_t and size_t to be the same size");
  MOZ_STATIC_ASSERT(uintptr_t(-1) > uintptr_t(0),
                    "expect uintptr_t to be unsigned");
  MOZ_STATIC_ASSERT(size_t(-1) > size_t(0),
                    "expect size_t to be unsigned");
  uintptr_t framePoisonValue = nsPresArena::GetPoisonValue();
  return size_t(framePoisonValue - uintptr_t(mValueStorage)) /
         sizeof(nsCSSValue);
}

nsRuleData::nsRuleData(PRUint32 aSIDs, nsCSSValue* aValueStorage,
                       nsPresContext* aContext, nsStyleContext* aStyleContext)
  : mSIDs(aSIDs),
    mCanStoreInRuleTree(true),
    mPresContext(aContext),
    mStyleContext(aStyleContext),
    mPostResolveCallback(nsnull),
    mValueStorage(aValueStorage)
{
#ifndef MOZ_VALGRIND
  size_t framePoisonOffset = GetPoisonOffset();
  for (size_t i = 0; i < nsStyleStructID_Length; ++i) {
    mValueOffsets[i] = framePoisonOffset;
  }
#endif
}

#ifdef DEBUG
nsRuleData::~nsRuleData()
{
#ifndef MOZ_VALGRIND
  // assert nothing in mSIDs has poison value
  size_t framePoisonOffset = GetPoisonOffset();
  for (size_t i = 0; i < nsStyleStructID_Length; ++i) {
    NS_ABORT_IF_FALSE(!(mSIDs & (1 << i)) ||
                      mValueOffsets[i] != framePoisonOffset,
                      "value in SIDs was left with poison offset");
  }
#endif
}
#endif
