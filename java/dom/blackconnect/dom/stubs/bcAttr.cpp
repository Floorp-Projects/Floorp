#include "bcAttr.h"
#include "bcElement.h"

NS_IMPL_ISUPPORTS1(bcAttr, Attr)

bcAttr::bcAttr(nsIDOMAttr* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
  nodePtr = new bcNode(ptr);
}

bcAttr::~bcAttr()
{
  /* destructor code */
}

/* readonly attribute DOMString name; */
NS_IMETHODIMP bcAttr::GetName(DOMString *aName)
{
  nsString ret;
  nsresult rv = domPtr->GetName(ret);
  *aName = ret.ToNewUnicode();
  return rv;
}

/* readonly attribute boolean specified; */
NS_IMETHODIMP bcAttr::GetSpecified(PRBool *aSpecified)
{
  return domPtr->GetSpecified(aSpecified);
}

/* attribute DOMString value; */
NS_IMETHODIMP bcAttr::GetValue(DOMString *aValue)
{
  nsString ret;
  nsresult rv = domPtr->GetValue(ret);
  *aValue = ret.ToNewUnicode();
  return rv;
}
NS_IMETHODIMP bcAttr::SetValue(DOMString aValue)
{
  nsString val(aValue);
  nsresult rv = domPtr->SetValue(val);
  return rv;
}

/* readonly attribute Element ownerElement; */
NS_IMETHODIMP bcAttr::GetOwnerElement(Element * *aOwnerElement)
{
  nsIDOMElement* ret = nsnull;
  nsresult rv = domPtr->GetOwnerElement(&ret);
  *aOwnerElement = new bcElement(ret);
  return rv;
}
