#ifndef _NSKEYGENHANDLER_H_
#define _NSKEYGENHANDLER_H_
// Form Processor 
#include "nsIFormProcessor.h" 
#include "ssmdefs.h"
#include "cmtcmn.h"

class nsIPSMComponent;

class nsKeygenFormProcessor : public nsIFormProcessor { 
public: 
  nsKeygenFormProcessor(); 
  NS_IMETHOD ProcessValue(nsIDOMHTMLElement *aElement, 
                          const nsString& aName, 
                          nsString& aValue); 

  NS_IMETHOD ProvideContent(const nsString& aFormType, 
                            nsVoidArray& aContent, 
                            nsString& aAttribute); 
  NS_DECL_ISUPPORTS 
protected:
  nsresult GetPublicKey(nsString& value, nsString& challenge, 
			nsString& keyType, nsString& outPublicKey,
			nsString& pqg);
  char * ChooseToken(PCMT_CONTROL control, CMKeyGenTagArg *psmarg,  
		     CMKeyGenTagReq *reason);
  char * SetUserPassword(PCMT_CONTROL control, CMKeyGenTagArg *psmarg,  
		     CMKeyGenTagReq *reason);
  nsIPSMComponent *mPSM;
}; 

#endif //_NSKEYGENHANDLER_H_
