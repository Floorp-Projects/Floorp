#ifndef __bcDOMImplementation_h__
#define __bcDOMImplementation_h__

#include "dom.h"
#include "nsIDOMDOMImplementation.h"

#define DOM_DOMIMPLEMENTATION_PTR(_domimplementation_) (!_domimplementation_ ? nsnull : ((bcDOMImplementation*)_domimplementation_)->domPtr)
#define NEW_BCDOMIMPLEMENTATION(_domimplementation_) (!_domimplementation_ ? nsnull : new bcDOMImplementation(_domimplementation_))

class bcDOMImplementation : public DOMImplementation
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_DOMIMPLEMENTATION

  bcDOMImplementation(nsIDOMDOMImplementation* domPtr);
  virtual ~bcDOMImplementation();
private:
  nsIDOMDOMImplementation* domPtr;
  /* additional members */
};

#endif //  __bcDOMImplementation_h__
