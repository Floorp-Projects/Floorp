#include "bcCDATASection.h"

NS_IMPL_ISUPPORTS1(bcCDATASection, CDATASection)

bcCDATASection::bcCDATASection(nsIDOMCDATASection* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
  textPtr = new bcText(ptr);
}

bcCDATASection::~bcCDATASection()
{
  /* destructor code */
}

