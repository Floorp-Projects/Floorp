#ifndef __bcCDATASection_h__
#define __bcCDATASection_h__

#include "dom.h"
#include "nsIDOMCDATASection.h"
#include "bcText.h"

#define DOM_CDATASECTION_PTR(_cdatasection_) (!_cdatasection_ ? nsnull : ((bcCDATASection*)_cdatasection_)->domPtr)
#define NEW_BCCDATASECTION(_cdatasection_) (!_cdatasection_ ? nsnull : new bcCDATASection(_cdatasection_))

class bcCDATASection : public CDATASection
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_CDATASECTION
  NS_FORWARD_NODE(textPtr->)
  NS_FORWARD_CHARACTERDATA(textPtr->)
  NS_FORWARD_TEXT(textPtr->)

  bcCDATASection(nsIDOMCDATASection* domPtr);
  virtual ~bcCDATASection();
private:
  nsIDOMCDATASection* domPtr;
  bcText* textPtr;
  /* additional members */
};

#endif //  __bcCDATASection_h__
