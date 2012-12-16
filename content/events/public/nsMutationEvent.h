/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMutationEvent_h__
#define nsMutationEvent_h__

#include "nsGUIEvent.h"
#include "nsIDOMNode.h"
#include "nsIAtom.h"

class nsMutationEvent : public nsEvent
{
public:
  nsMutationEvent(bool isTrusted, uint32_t msg)
    : nsEvent(isTrusted, msg, NS_MUTATION_EVENT),
      mAttrChange(0)
  {
    mFlags.mCancelable = false;
  }

  nsCOMPtr<nsIDOMNode> mRelatedNode;
  nsCOMPtr<nsIAtom>    mAttrName;
  nsCOMPtr<nsIAtom>    mPrevAttrValue;
  nsCOMPtr<nsIAtom>    mNewAttrValue;
  unsigned short       mAttrChange;
};

#define NS_MUTATION_START           1800
#define NS_MUTATION_SUBTREEMODIFIED                   (NS_MUTATION_START)
#define NS_MUTATION_NODEINSERTED                      (NS_MUTATION_START+1)
#define NS_MUTATION_NODEREMOVED                       (NS_MUTATION_START+2)
#define NS_MUTATION_NODEREMOVEDFROMDOCUMENT           (NS_MUTATION_START+3)
#define NS_MUTATION_NODEINSERTEDINTODOCUMENT          (NS_MUTATION_START+4)
#define NS_MUTATION_ATTRMODIFIED                      (NS_MUTATION_START+5)
#define NS_MUTATION_CHARACTERDATAMODIFIED             (NS_MUTATION_START+6)
#define NS_MUTATION_END                               (NS_MUTATION_START+6)

// Bits are actually checked to optimize mutation event firing.
// That's why I don't number from 0x00.  The first event should
// always be 0x01.
#define NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED                0x01
#define NS_EVENT_BITS_MUTATION_NODEINSERTED                   0x02
#define NS_EVENT_BITS_MUTATION_NODEREMOVED                    0x04
#define NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT        0x08
#define NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT       0x10
#define NS_EVENT_BITS_MUTATION_ATTRMODIFIED                   0x20
#define NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED          0x40

#endif // nsMutationEvent_h__
