#include "bcEntityReference.h"

NS_IMPL_ISUPPORTS1(bcEntityReference, EntityReference)

bcEntityReference::bcEntityReference(nsIDOMEntityReference* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
  nodePtr = new bcNode(ptr);
}

bcEntityReference::~bcEntityReference()
{
  /* destructor code */
}

