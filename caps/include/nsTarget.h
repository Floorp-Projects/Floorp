/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _NS_TARGET_H_
#define _NS_TARGET_H_

#include "nsHashtable.h"
#include "nsVector.h"
#include "nsIPrincipal.h"
#include "nsIPrincipalArray.h"
#include "nsIPrivilege.h"
#include "nsITarget.h"
#include "nsUserDialogHelper.h"

PR_BEGIN_EXTERN_C
#include "jpermission.h"
PR_PUBLIC_API(void)
java_netscape_security_getTargetDetails(const char *charSetName, char* targetName, 
                                        char** details, char **risk);
/* XXXXXXXX Begin oF HACK */

#ifdef XXX
extern int CAPS_TARGET_RISK_COLOR_HIGH;
#endif

extern char* capsGetString(int id);
/* XXXXXXXX end oF HACK */

PR_END_EXTERN_C

extern PRBool CreateSystemTargets(nsIPrincipal * sysPrin);

class nsTarget : public nsITarget {

public:

	NS_DECL_ISUPPORTS

	nsTarget(char *name, nsIPrincipal * prin, 
		PRInt32 risk = JavaSecUI_targetRiskHigh(), 
		/* XXX: char *riskColor = JavaSecUI_getString(CAPS_TARGET_RISK_COLOR_HIGH), */
		char * riskColor = "High", char * description = NULL, char * detailDescription = NULL,
		char * url = NULL, nsTargetArray* targetArray = NULL)
	{
		this->Init(name, prin, targetArray, risk, riskColor, description, detailDescription, url);
	}

	nsTarget(char *name, nsIPrincipal * prin, 
		PRInt32 risk = JavaSecUI_targetRiskHigh(), 
		/* XXX: char *riskColor = JavaSecUI_getString(CAPS_TARGET_RISK_COLOR_HIGH), */
		char * riskColor = "High", int desc_id = 0, int detail_desc_id = 0,
		int help_url_id = 0, nsTargetArray* targetArray = NULL);

	virtual ~nsTarget(void);

	static nsITarget * FindTarget(nsITarget * target);

	static nsITarget * FindTarget(char * name);

	static nsITarget * FindTarget(char * name, nsIPrincipal *prin);

	static nsITarget * GetTargetFromDescription(char *a);

	static nsTargetArray * GetAllRegisteredTargets(void);

	nsIPrivilege * CheckPrivilegeEnabled(nsTargetArray * targetArray, void *data);

	nsIPrivilege * CheckPrivilegeEnabled(nsTargetArray * targetArray);

	nsIPrivilege * CheckPrivilegeEnabled(nsIPrincipal * p, void * data);

	NS_IMETHOD RegisterTarget(void * context, nsITarget * * target);

	NS_IMETHOD EnablePrivilege(nsIPrincipal * prin, void * data, nsIPrivilege * * priv);

//	virtual nsIPrivilege * EnablePrivilege(nsIPrincipal *prin, void *data);
//seems more likely to bleong to nsIPrivilege
	nsIPrivilege * GetPrincipalPrivilege(nsIPrincipal *prin, void *data);

	NS_IMETHOD GetFlattenedTargetArray(nsTargetArray * * targs);

	NS_IMETHOD GetRisk(char * * risk);

	NS_IMETHOD GetRiskColor(char * * riskColor);

	NS_IMETHOD GetDescription(char * * description);

	NS_IMETHOD GetDetailDescription(char * * detailedDescription);

	NS_IMETHOD GetHelpURL(char * * helpUrl);

	NS_IMETHOD GetDetailedInfo(void *a, char * * dinfo);

	NS_IMETHOD GetPrincipal (nsIPrincipal * * prin);

	NS_IMETHOD GetName(char * * name);

	NS_IMETHOD IsRegistered(PRBool * result);

	NS_IMETHOD Equals(nsITarget * other, PRBool * eq);

	NS_IMETHOD HashCode(PRUint32 * code);

	NS_IMETHOD ToString(char * * result);

private:

	/* Private Field Accessors */
	char * itsName;
	nsIPrincipal * itsPrincipal;
	PRBool itsRegistered;
	PRInt32 itsRisk;
	char * itsRiskColorStr;
	char * itsDescriptionStr;
	char * itsDetailDescriptionStr;
	char * itsURLStr;
	nsTargetArray * itsTargetArray;
	nsTargetArray * itsExpandedTargetArray;
	char * itsString;
	PRUint32 itsDescriptionHash;
	static PRBool theInited;

	/* Private Methods */
	void Init(char *name, nsIPrincipal *prin, nsTargetArray* targetArray, PRInt32 risk,
				char *riskColor, char *description, char *detailDescription, char *url);

	void GetFlattenedTargets(nsHashtable *targHash, nsTargetArray* expandedTargetArray);

};


class TargetKey: public nsHashKey {
public:
	nsITarget * itsTarget;
	TargetKey(nsITarget * targ) {
		itsTarget = targ;
	}

	PRUint32 HashValue(void) const {
		PRUint32 code=0;
		itsTarget->HashCode(& code);
		return NS_OK;
	}

	PRBool Equals(const nsHashKey * aKey) const {
		PRBool eq;
		itsTarget->Equals(((const TargetKey *) aKey)->itsTarget, & eq);
		return eq;
	}

	nsHashKey * Clone(void) const {
		return new TargetKey(itsTarget);
	}
};

/* XXX: IMO, IntegerKey and StringKey should be part of xpcom */
class IntegerKey: public nsHashKey {
private:
  PRUint32 itsHash;
  
public:
  IntegerKey(PRUint32 hash) {
    itsHash = hash;
  }
  
  PRUint32 HashValue(void) const {
    return itsHash;
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return (itsHash == (((const IntegerKey *) aKey)->itsHash)) ? PR_TRUE : PR_FALSE;
  }

  nsHashKey *Clone(void) const {
    return new IntegerKey(itsHash);
  }
};

class StringKey: public nsHashKey {
public:
  const char *itsString;

  StringKey(const char *string) {
    itsString = string;
  }
  
  PRUint32 HashValue(void) const {
    return PL_HashString(itsString);
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return (strcmp(itsString, (((const StringKey *) aKey)->itsString)) == 0);
  }

  nsHashKey *Clone(void) const {
    return new StringKey(itsString);
  }
};

#endif /* _NS_TARGET_H_ */
