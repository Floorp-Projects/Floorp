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

  NS_IMETHOD GetStatePropertyAsSupports(const nsAString& aName,
					nsISupports** aResult) = 0;
  NS_IMETHOD SetStatePropertyAsSupports(const nsAString& aName,
					nsISupports* aValue) = 0;

  /**
   * Get the current value of a property.
   *
   * @param aName the name of the property.
   * @param aResult the value (may legitimately be the empty string) [OUT]
   * @throws NS_STATE_PROPERTY_NOT_THERE if the property is not set 
   * @throws NS_STATE_PROPERTY_EXISTS if the attribute exists (==NS_OK)
   */
  NS_IMETHOD GetStateProperty(const nsAString& aName,
			      nsAString& aResult) = 0;
  NS_IMETHOD SetStateProperty(const nsAString& aProperty,
			      const nsAString& aValue) = 0;

  NS_IMETHOD RemoveStateProperty(const nsAString& aName) = 0;
};

nsresult
NS_NewPresState(nsIPresState** aResult);

#endif /* _nsIPresState_h */
