#include "bcNodeList.h"
#include "bcNode.h"

NS_IMPL_ISUPPORTS1(bcNodeList, NodeList)

bcNodeList::bcNodeList(nsIDOMNodeList* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
}

bcNodeList::~bcNodeList()
{
  /* destructor code */
}

/* Node item (in unsigned long index); */
NS_IMETHODIMP bcNodeList::Item(PRUint32 index, Node **_retval)
{
  nsIDOMNode* ret = NULL;
  nsresult rv = domPtr->Item(index, &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

/* readonly attribute unsigned long length; */
NS_IMETHODIMP bcNodeList::GetLength(PRUint32 *aLength)
{
    return domPtr->GetLength(aLength);
}

