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

#include <string.h>
#include "prtypes.h"
#include "plhash.h"
#include "nsHashtable.h"
#include "nsVector.h"
//#include "nsCaps.h"
#include "nsIPrincipal.h"
#include "nsIPrivilege.h"
#include "nsUserDialogHelper.h"

typedef nsVector nsTargetArray;

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


extern PRBool CreateSystemTargets(nsIPrincipal *sysPrin);

class nsTarget {

public:

	/* Public Methods */

	virtual ~nsTarget(void);

	nsTarget(char *name, nsIPrincipal * prin, 
		PRInt32 risk = JavaSecUI_targetRiskHigh(), 
		/* XXX: char *riskColor = JavaSecUI_getString(CAPS_TARGET_RISK_COLOR_HIGH), */
		char * riskColor = "High",
		char * description = NULL, 
		char * detailDescription = NULL,
		char * url = NULL, 
		nsTargetArray* targetArray = NULL)
	{
		this->Init(name, prin, targetArray, risk, riskColor, description, detailDescription, url);
    }

	nsTarget(char *name, nsIPrincipal * prin, 
		PRInt32 risk = JavaSecUI_targetRiskHigh(), 
		/* XXX: char *riskColor = JavaSecUI_getString(CAPS_TARGET_RISK_COLOR_HIGH), */
		char * riskColor = "High",
		int desc_id = 0, 
		int detail_desc_id = 0,
		int help_url_id = 0, 
		nsTargetArray* targetArray = NULL);

	nsTarget * RegisterTarget(void);

	nsTarget * RegisterTarget(void* context);

	static nsTarget * FindTarget(nsTarget *target);

	static nsTarget * FindTarget(char * name);

	static nsTarget * FindTarget(char * name, nsIPrincipal *prin);

	nsIPrivilege * CheckPrivilegeEnabled(nsTargetArray* prinArray, void *data);

	nsIPrivilege * CheckPrivilegeEnabled(nsTargetArray* prinArray);

	nsIPrivilege * CheckPrivilegeEnabled(nsIPrincipal *p, void *data);

	virtual nsIPrivilege * EnablePrivilege(nsIPrincipal *prin, void *data);

	nsIPrivilege * GetPrincipalPrivilege(nsIPrincipal *prin, void *data);

	nsTargetArray* GetFlattenedTargetArray(void);

	static nsTargetArray* GetAllRegisteredTargets(void);

	char * GetRisk(void);

	char * GetRiskColor(void);

	char * GetDescription(void);

	char * GetDetailDescription(void);

	static nsTarget * GetTargetFromDescription(char *a);

	char * GetHelpURL(void);

	char * GetDetailedInfo(void *a);

	nsIPrincipal * GetPrincipal(void);

	char * GetName(void);

	PRBool Equals(nsTarget *a);

	PRInt32 HashCode(void);

	char * ToString(void);

	PRBool IsRegistered(void);

private:

	/* Private Field Accessors */
	char * itsName;

	nsIPrincipal * itsPrincipal;

	PRInt32 itsRisk;

	char * itsRiskColorStr;

	char * itsDescriptionStr;

	char * itsDetailDescriptionStr;

	char * itsURLStr;

	PRBool itsRegistered;

	nsTargetArray* itsTargetArray;

	nsTargetArray* itsExpandedTargetArray;
	
	char *itsString;

	PRUint32 itsDescriptionHash;

	static PRBool theInited;

	/* Private Methods */
	void Init(char *name, nsIPrincipal *prin, nsTargetArray* targetArray, PRInt32 risk,
				char *riskColor, char *description, char *detailDescription, char *url);

	void GetFlattenedTargets(nsHashtable *targHash, nsTargetArray* expandedTargetArray);

};


class TargetKey: public nsHashKey {
public:
  nsTarget *itsTarget;
  TargetKey(nsTarget *targ) {
    itsTarget = targ;
  }
  
  PRUint32 HashValue(void) const {
    return itsTarget->HashCode();
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return (itsTarget->Equals(((const TargetKey *) aKey)->itsTarget));
  }

  nsHashKey *Clone(void) const {
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
