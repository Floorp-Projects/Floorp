#ifndef _nsIStatefulFrame_h
#define _nsIStatefulFrame_h

#include "nsISupports.h"

class nsPresContext;
class nsIPresState;

#define NS_ISTATEFULFRAME_IID_STR "306c8ca0-5f0c-11d3-a9fb-000064657374"

#define NS_ISTATEFULFRAME_IID \
{0x306c8ca0, 0x5f0c, 0x11d3, \
{0xa9, 0xfb, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74}}

class nsIStatefulFrame : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISTATEFULFRAME_IID)

  // If you create a special type stateful frame (e.g. scroll) that needs
  // to be captured outside of the standard pass through the frames, you'll need
  // a special ID by which to refer to that type.
  //
  // There is space reserved between standard ID's and special ID's by the
  // offset NS_CONTENT_ID_COUNTER_BASE
  enum SpecialStateID {eNoID=0, eDocumentScrollState};

  NS_IMETHOD SaveState(nsPresContext* aPresContext, nsIPresState** aState) = 0;
  NS_IMETHOD RestoreState(nsPresContext* aPresContext, nsIPresState* aState) = 0;
};

#endif /* _nsIStatefulFrame_h */
