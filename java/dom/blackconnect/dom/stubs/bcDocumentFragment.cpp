#include "bcDocumentFragment.h"

NS_IMPL_ISUPPORTS1(bcDocumentFragment, DocumentFragment)

bcDocumentFragment::bcDocumentFragment(nsIDOMDocumentFragment* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
  nodePtr = new bcNode(ptr);
}

bcDocumentFragment::~bcDocumentFragment()
{
  /* destructor code */
}

