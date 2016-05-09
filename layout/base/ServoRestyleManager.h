/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoRestyleManager_h
#define mozilla_ServoRestyleManager_h

#include "mozilla/EventStates.h"
#include "nsChangeHint.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla
class nsAttrValue;
class nsIAtom;
class nsIContent;
class nsIFrame;

namespace mozilla {

/**
 * Restyle manager for a Servo-backed style system.
 */
class ServoRestyleManager
{
public:
  NS_INLINE_DECL_REFCOUNTING(ServoRestyleManager)

  ServoRestyleManager();

  void Disconnect();
  void PostRestyleEvent(dom::Element* aElement,
                        nsRestyleHint aRestyleHint,
                        nsChangeHint aMinChangeHint);
  void PostRestyleEventForLazyConstruction();
  void RebuildAllStyleData(nsChangeHint aExtraHint,
                           nsRestyleHint aRestyleHint);
  void PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                    nsRestyleHint aRestyleHint);
  void ProcessPendingRestyles();
  void RestyleForInsertOrChange(dom::Element* aContainer,
                                nsIContent* aChild);
  void RestyleForAppend(dom::Element* aContainer,
                        nsIContent* aFirstNewContent);
  void RestyleForRemove(dom::Element* aContainer,
                        nsIContent* aOldChild,
                        nsIContent* aFollowingSibling);
  nsresult ContentStateChanged(nsIContent* aContent,
                               EventStates aStateMask);
  void AttributeWillChange(dom::Element* aElement,
                           int32_t aNameSpaceID,
                           nsIAtom* aAttribute,
                           int32_t aModType,
                           const nsAttrValue* aNewValue);
  void AttributeChanged(dom::Element* aElement,
                        int32_t aNameSpaceID,
                        nsIAtom* aAttribute,
                        int32_t aModType,
                        const nsAttrValue* aOldValue);
  nsresult ReparentStyleContext(nsIFrame* aFrame);
  bool HasPendingRestyles();
  uint64_t GetRestyleGeneration() const;

protected:
  ~ServoRestyleManager() {}

  uint64_t mRestyleGeneration;
};

} // namespace mozilla

#endif // mozilla_ServoRestyleManager_h
