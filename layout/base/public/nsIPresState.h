#ifndef _nsIPresState_h
#define _nsIPresState_h

#include "nsISupports.h"
#include "nsString.h"

// {98DABCE1-C9D7-11d3-BF87-00105A1B0627}
#define NS_IPRESSTATE_IID \
{ 0x98dabce1, 0xc9d7, 0x11d3, { 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }

class nsIPresState : public nsISupports {
public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPRESSTATE_IID)

  NS_IMETHOD GetStateProperty(nsString& aProperty) = 0;
  NS_IMETHOD SetStateProperty(const nsString& aProperty) = 0;
};

#endif /* _nsIPresState_h */
