#include "bcText.h"

NS_IMPL_ISUPPORTS1(bcText, Text)

bcText::bcText(nsIDOMText* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
  dataPtr = new bcCharacterData(ptr);
}

bcText::~bcText()
{
  /* destructor code */
}

/* Text splitText (in unsigned long offset)  raises (DOMException); */
NS_IMETHODIMP bcText::SplitText(PRUint32 offset, Text **_retval)
{
  nsIDOMText* ret = nsnull;
  nsresult rv = domPtr->SplitText(offset, &ret);
  *_retval = NEW_BCTEXT(ret);
  return rv;
}

