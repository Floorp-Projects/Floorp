#include "nsCSSDecIntHolder.h"

////////////////////////////////////////////////////
// Quick and dirty definition of holder object

nsCSSDecIntHolder::nsCSSDecIntHolder() { }

nsCSSDecIntHolder::nsCSSDecIntHolder(PRUint32 aInt)
{
  mInt = aInt;
  NS_INIT_REFCNT();

}

nsCSSDecIntHolder::~nsCSSDecIntHolder() 
{ 
}

NS_IMPL_ISUPPORTS1(nsCSSDecIntHolder, nsICSSDecIntHolder);
