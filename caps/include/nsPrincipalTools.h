#ifndef _NS_PRINCIPAL_TOOLS_H_
#define _NS_PRINCIPAL_TOOLS_H_

#include "nsIPrincipal.h"
#include "nsHashtable.h"

typedef  nsVector nsPrincipalArray;

class PrincipalKey: public nsHashKey {
public:
	nsIPrincipal * itsPrincipal;

	PrincipalKey(nsIPrincipal * prin) {
		itsPrincipal = prin;
	}

	PRUint32 HashValue(void) const {
		PRUint32 * code;
		itsPrincipal->HashCode(code);
		return *code;
	}

	PRBool Equals(const nsHashKey * aKey) const {
		PRBool * result;
		itsPrincipal->Equals(((const PrincipalKey *) aKey)->itsPrincipal, result);
		return *result;
	}

	nsHashKey * Clone(void) const {
	  return new PrincipalKey(itsPrincipal);
	}
};

#endif /* _NS_PRINCIPAL_TOOLS_H_ */
