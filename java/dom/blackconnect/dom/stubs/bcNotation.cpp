#include "bcNotation.h"

NS_IMPL_ISUPPORTS1(bcNotation, Notation)

bcNotation::bcNotation(nsIDOMNotation* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
  nodePtr = new bcNode(ptr);
}

bcNotation::~bcNotation()
{
  /* destructor code */
}

/* readonly attribute DOMString publicId; */
NS_IMETHODIMP bcNotation::GetPublicId(DOMString *aPublicId)
{
  nsString ret;
  nsresult rv = domPtr->GetPublicId(ret);
  *aPublicId = ret.ToNewUnicode();
  return rv;
}


/* readonly attribute DOMString systemId; */
NS_IMETHODIMP bcNotation::GetSystemId(DOMString *aSystemId)
{
  nsString ret;
  nsresult rv = domPtr->GetSystemId(ret);
  *aSystemId = ret.ToNewUnicode();
  return rv;
}


