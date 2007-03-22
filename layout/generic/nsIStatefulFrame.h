/*
 * interface for rendering objects whose state is saved in
 * session-history (back-forward navigation)
 */

#ifndef _nsIStatefulFrame_h
#define _nsIStatefulFrame_h

#include "nsISupports.h"

class nsPresContext;
class nsPresState;

#define NS_ISTATEFULFRAME_IID \
{ 0x25c232cf, 0x40ba, 0x4394, \
 { 0x84, 0xe4, 0x73, 0xa2, 0xf7, 0x4d, 0x8b, 0x64 } }

class nsIStatefulFrame : public nsISupports {
 public: 
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISTATEFULFRAME_IID)

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

NS_DEFINE_STATIC_IID_ACCESSOR(nsIStatefulFrame, NS_ISTATEFULFRAME_IID)

#endif /* _nsIStatefulFrame_h */
