#ifndef _NS_PRINCIPAL_ARRAY_H_
#define _NS_PRINCIPAL_ARRAY_H_

#include "nsIPrincipalArray.h"
#include "nsIPrincipal.h"
#include "nsVector.h"
#include "nsHashtable.h"


class nsPrincipalArray : public nsIPrincipalArray {
public:

NS_DECL_ISUPPORTS

nsPrincipalArray(void);
nsPrincipalArray(PRUint32 count);
~nsPrincipalArray();

NS_IMETHOD
ComparePrincipalArray(nsIPrincipalArray * other, PRInt16 * comparisonType);

NS_IMETHOD
IntersectPrincipalArray(nsIPrincipalArray * other , nsIPrincipalArray * * result);

NS_IMETHOD
FreePrincipalArray();

NS_IMETHOD
AddPrincipalArrayElement(nsIPrincipal * principal);

NS_IMETHOD
GetPrincipalArrayElement(PRUint32 index, nsIPrincipal * * result);

NS_IMETHOD
SetPrincipalArrayElement(PRUint32 index, nsIPrincipal * principal);

NS_IMETHOD
GetPrincipalArraySize(PRUint32 * result);

private:
nsVector * itsArray;
};

class PrincipalKey: public nsHashKey {
public:
	nsIPrincipal * itsPrincipal;

	PrincipalKey(nsIPrincipal * prin) {
		itsPrincipal = prin;
	}

	PRUint32 HashValue(void) const {
		PRUint32 * code = 0;
		itsPrincipal->HashCode(code);
		return *code;
	}

	PRBool Equals(const nsHashKey * aKey) const {
		PRBool result = PR_FALSE;
		itsPrincipal->Equals(((const PrincipalKey *) aKey)->itsPrincipal,& result);
		return result;
	}

	nsHashKey * Clone(void) const {
		return new PrincipalKey(itsPrincipal);
	}
};

#endif /* _NS_PRINCIPAL_TOOLS_H_ */
