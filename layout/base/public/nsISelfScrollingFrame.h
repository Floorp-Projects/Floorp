#ifndef _nsISelfScrollingFrame_h
#define _nsISelfScrollingFrame_h

#include "nsISupports.h"

#define NS_ISELFSCROLLINGFRAME_IID_STR "ffc3c23a-1dd1-11b2-9ffa-bfef1a297bb7"

#define NS_ISELFSCROLLINGFRAME_IID \
{0xffc3c23a, 0x1dd1, 0x11b2, \
{0x9f, 0xfa, 0xbf, 0xef, 0x1a, 0x29, 0x7b, 0xb7}}

class nsIPresContext;

class nsISelfScrollingFrame : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISELFSCROLLINGFRAME_IID)

  NS_IMETHOD ScrollByLines(nsIPresContext* aPresContext, PRInt32 lines)=0;

};

#endif /* _nsISelfScrollingFrame_h */
