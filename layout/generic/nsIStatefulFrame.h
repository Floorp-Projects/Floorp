/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * interface for rendering objects whose state is saved in
 * session-history (back-forward navigation)
 */

#ifndef _nsIStatefulFrame_h
#define _nsIStatefulFrame_h

#include "nsQueryFrame.h"

class nsPresState;

class nsIStatefulFrame
{
 public: 
  NS_DECL_QUERYFRAME_TARGET(nsIStatefulFrame)

  // If you create a special type stateful frame (e.g. scroll) that needs
  // to be captured outside of the standard pass through the frames, you'll need
  // a special ID by which to refer to that type.
  enum SpecialStateID {eNoID=0, eDocumentScrollState};

  // Save the state for this frame.  Some implementations may choose to return
  // different states depending on the value of aStateID.  If this method
  // succeeds, the caller is responsible for deleting the resulting state when
  // done with it.
  NS_IMETHOD SaveState(SpecialStateID aStateID, nsPresState** aState) = 0;

  // Restore the state for this frame from aState
  NS_IMETHOD RestoreState(nsPresState* aState) = 0;
};

#endif /* _nsIStatefulFrame_h */
