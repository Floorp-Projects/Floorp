#include "bcComment.h"

NS_IMPL_ISUPPORTS1(bcComment, Comment)

bcComment::bcComment(nsIDOMComment* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
  dataPtr = new bcCharacterData(ptr);
}

bcComment::~bcComment()
{
  /* destructor code */
}

