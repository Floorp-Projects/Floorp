
#ifndef nscssdecintholder___h___
#define nscssdecintholder___h___

#include "nsICSSDecIntHolder.h"

////////////////////////////////////////////////////
// Quick and dirty declaration of holder object

class nsCSSDecIntHolder : public nsICSSDecIntHolder {

public:
  nsCSSDecIntHolder();
  nsCSSDecIntHolder(PRUint32 aInt);
  virtual ~nsCSSDecIntHolder();
  
  PRUint32 mInt;
  
  // nsISupports
	NS_DECL_ISUPPORTS

  // nsICSSDecIntHolder
	NS_DECL_NSICSSDECINTHOLDER
};

#endif
