/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIAbsorbingTransaction_h__
#define nsIAbsorbingTransaction_h__

#include "nsISupports.h"

/*
Transaction interface to outside world
*/

#define NS_IABSORBINGTRANSACTION_IID \
{ /* a6cf9116-15b3-11d2-932e-00805f8add32 */ \
    0xa6cf9116, \
    0x15b3, \
    0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

class nsIAtom;

namespace mozilla {
class EditorBase;
class PlaceholderTransaction;
class SelectionState;
} // namespace mozilla

/**
 * A transaction interface mixin - for transactions that can support.
 * the placeholder absorbtion idiom.
 */
class nsIAbsorbingTransaction  : public nsISupports{
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IABSORBINGTRANSACTION_IID)

  NS_IMETHOD EndPlaceHolderBatch()=0;

  NS_IMETHOD GetTxnName(nsIAtom **aName)=0;

  NS_IMETHOD StartSelectionEquals(mozilla::SelectionState* aSelState,
                                  bool* aResult) = 0;

  NS_IMETHOD ForwardEndBatchTo(nsIAbsorbingTransaction *aForwardingAddress)=0;

  NS_IMETHOD Commit()=0;

  NS_IMETHOD_(mozilla::PlaceholderTransaction*)
    AsPlaceholderTransaction() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIAbsorbingTransaction,
                              NS_IABSORBINGTRANSACTION_IID)

#endif // nsIAbsorbingTransaction_h__

