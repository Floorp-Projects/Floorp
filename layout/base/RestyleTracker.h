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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     Boris Zbarsky <bzbarsky@mit.edu> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#ifndef mozilla_css_RestyleTracker_h
#define mozilla_css_RestyleTracker_h

/**
 * A class which manages pending restyles.  This handles keeping track
 * of what nodes restyles need to happen on and so forth.
 */

#include "mozilla/dom/Element.h"
#include "nsDataHashtable.h"

class nsCSSFrameConstructor;

namespace mozilla {
namespace css {

class RestyleTracker {
public:
  typedef mozilla::dom::Element Element;

  RestyleTracker(PRUint32 aRestyleBits,
                 nsCSSFrameConstructor* aFrameConstructor) :
    mRestyleBits(aRestyleBits), mFrameConstructor(aFrameConstructor)
  {
    NS_PRECONDITION((mRestyleBits & ~ELEMENT_ALL_RESTYLE_FLAGS) == 0,
                    "Why do we have these bits set?");
    NS_PRECONDITION((mRestyleBits & ELEMENT_PENDING_RESTYLE_FLAGS) != 0,
                    "Must have a restyle flag");
    NS_PRECONDITION((mRestyleBits & ELEMENT_PENDING_RESTYLE_FLAGS) !=
                      ELEMENT_PENDING_RESTYLE_FLAGS,
                    "Shouldn't have both restyle flags set");
    NS_PRECONDITION((mRestyleBits & ~ELEMENT_PENDING_RESTYLE_FLAGS) != 0,
                    "Must have root flag");
    NS_PRECONDITION((mRestyleBits & ~ELEMENT_PENDING_RESTYLE_FLAGS) !=
                    (ELEMENT_ALL_RESTYLE_FLAGS & ~ELEMENT_PENDING_RESTYLE_FLAGS),
                    "Shouldn't have both root flags");
  }

  PRBool Init() {
    return mPendingRestyles.Init();
  }

  PRUint32 Count() const {
    return mPendingRestyles.Count();
  }

  /**
   * Add a restyle for the given element to the tracker.
   */
  void AddPendingRestyle(Element* aElement, nsRestyleHint aRestyleHint,
                         nsChangeHint aMinChangeHint) {
    AddPendingRestyle(aElement, aRestyleHint, nsRestyleHint(0), aMinChangeHint);
  }

  /**
   * Process the restyles we've been tracking.
   */
  void ProcessRestyles();

  struct RestyleData {
    nsRestyleHint mRestyleHint;  // What we want to restyle
    nsChangeHint  mChangeHint;   // The minimal change hint for "self"
  };

  struct RestyleEnumerateData : public RestyleData {
    nsCOMPtr<Element> mElement;
  };

private:
  /**
   * Add a restyle for the given element to the tracker and remove the
   * given bits from the existing restyle hint for the element, if
   * there is one.
   */
  inline void AddPendingRestyle(Element* aElement, nsRestyleHint aRestyleHint,
                                nsRestyleHint aRestyleHintToRemove,
                                nsChangeHint aMinChangeHint);

  // Handle a single mPendingRestyles entry.
  inline void ProcessOneRestyle(Element* aElement,
                                nsRestyleHint aRestyleHint,
                                nsChangeHint aChangeHint);
  
  typedef nsDataHashtable<nsISupportsHashKey, RestyleData> PendingRestyleTable;
  // Our restyle bits.  These will be a subset of ELEMENT_ALL_RESTYLE_FLAGS, and
  // will include one flag from ELEMENT_PENDING_RESTYLE_FLAGS and one flag
  // that's not in ELEMENT_PENDING_RESTYLE_FLAGS.
  PRUint32 mRestyleBits;
  nsCSSFrameConstructor* mFrameConstructor; // Owns us
  PendingRestyleTable mPendingRestyles;
};

inline void RestyleTracker::AddPendingRestyle(Element* aElement,
                                              nsRestyleHint aRestyleHint,
                                              nsRestyleHint aRestyleHintToRemove,
                                              nsChangeHint aMinChangeHint)
{
  RestyleData existingData;
  existingData.mRestyleHint = nsRestyleHint(0);
  existingData.mChangeHint = NS_STYLE_HINT_NONE;

  mPendingRestyles.Get(aElement, &existingData);

  existingData.mRestyleHint =
    nsRestyleHint((existingData.mRestyleHint | aRestyleHint) &
                  ~aRestyleHintToRemove);
  NS_UpdateHint(existingData.mChangeHint, aMinChangeHint);

  mPendingRestyles.Put(aElement, existingData);
}

} // namespace css
} // namespacs mozilla

#endif /* mozilla_css_RestyleTracker_h */
