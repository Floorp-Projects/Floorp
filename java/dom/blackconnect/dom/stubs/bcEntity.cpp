#include "bcEntity.h"

NS_IMPL_ISUPPORTS1(bcEntity, Entity)

bcEntity::bcEntity(nsIDOMEntity* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
  nodePtr = new bcNode(ptr);
}

bcEntity::~bcEntity()
{
  /* destructor code */
}

/* readonly attribute DOMString publicId; */
NS_IMETHODIMP bcEntity::GetPublicId(DOMString *aPublicId)
{
  nsString ret;
  nsresult rv = domPtr->GetPublicId(ret);
  *aPublicId = ret.ToNewUnicode();
  return rv;
}


/* readonly attribute DOMString systemId; */
NS_IMETHODIMP bcEntity::GetSystemId(DOMString *aSystemId)
{
  nsString ret;
  nsresult rv = domPtr->GetSystemId(ret);
  *aSystemId = ret.ToNewUnicode();
  return rv;
}


/* readonly attribute DOMString notationName; */
NS_IMETHODIMP bcEntity::GetNotationName(DOMString *aNotationName)
{
  nsString ret;
  nsresult rv = domPtr->GetNotationName(ret);
  *aNotationName = ret.ToNewUnicode();
  return rv;
}


