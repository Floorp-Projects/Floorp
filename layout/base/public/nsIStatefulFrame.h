#ifndef _nsIStatefulFrame_h
#define _nsIStatefulFrame_h

#include "nsISupports.h"

#define NS_ISTATEFULFRAME_IID_STR "306c8ca0-5f0c-11d3-a9fb-000064657374"

#define NS_ISTATEFULFRAME_IID \
{0x306c8ca0, 0x5f0c, 0x11d3, \
{0xa9, 0xfb, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74}}

class nsIStatefulFrame : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISTATEFULFRAME_IID)

  NS_IMETHOD SaveState(nsILayoutHistoryState* aState) = 0;
  NS_IMETHOD RestoreState(nsILayoutHistoryState* aState) = 0;
};

#endif /* _nsIStatefulFrame_h */
