#ifndef _nsIStatefulFrame_h
#define _nsIStatefulFrame_h

#include "nsISupports.h"

class nsPresContext;
class nsPresState;

#define NS_ISTATEFULFRAME_IID \
{0x26254ab7, 0xdea3, 0x4375, \
{0xb0, 0x1d, 0xbd, 0x11, 0xa1, 0x4b, 0x54, 0xbc}}

class nsIStatefulFrame : public nsISupports {
 public: 
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISTATEFULFRAME_IID)

  // If you create a special type stateful frame (e.g. scroll) that needs
  // to be captured outside of the standard pass through the frames, you'll need
  // a special ID by which to refer to that type.
  //
  // There is space reserved between standard ID's and special ID's by the
  // offset NS_CONTENT_ID_COUNTER_BASE
  enum SpecialStateID {eNoID=0, eDocumentScrollState};

  NS_IMETHOD SaveState(nsPresContext* aPresContext, nsPresState** aState) = 0;
  NS_IMETHOD RestoreState(nsPresContext* aPresContext, nsPresState* aState) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIStatefulFrame, NS_ISTATEFULFRAME_IID)

#endif /* _nsIStatefulFrame_h */
