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

#ifndef _NS_PRIVILEGE_H_
#define _NS_PRIVILEGE_H_

#include "prtypes.h"
//#include "nsCaps.h"
#include "nsCapsEnums.h"

PRBool nsPrivilegeInitialize(void);

struct nsPrivilege {

public:
	/* Public Field Accessors */
	nsPermissionState itsPerm;

	nsDurationState itsDuration;

	/* Public Methods */
	nsPrivilege(nsPermissionState perm, nsDurationState duration);

	virtual ~nsPrivilege(void);

	static nsPrivilege * findPrivilege(nsPermissionState permission, nsDurationState duration);

	static nsPermissionState add(nsPermissionState perm1, nsPermissionState perm2);

	static nsPrivilege * add(nsPrivilege *privilege1, nsPrivilege *privilege2);

	PRBool samePermission(nsPrivilege *privilege);

	PRBool samePermission(nsPermissionState perm);

	PRBool sameDuration(nsPrivilege *privilege);

	PRBool sameDuration(nsDurationState duration);

	PRBool isAllowed(void);

	PRBool isAllowedForever(void);

	PRBool isForbidden(void);

	PRBool isForbiddenForever(void);

	PRBool isBlank(void);

	nsPermissionState getPermission(void);

	nsDurationState getDuration(void);

	static nsPrivilege * findPrivilege(char *privStr);

	char * toString(void);

private:

	/* Private Field Accessors */
	char *itsString;

	static PRBool theInited;

	/* Private Methods */
};

/**
 * add() method takes two permissions and returns a new permission.
 * Permission addition follows these rules:
 * <pre>
 *   ALLOWED   + ALLOWED   = ALLOWED        1 + 1 = 1
 *   ALLOWED   + BLANK     = ALLOWED        1 + 2 = 1
 *   BLANK     + BLANK     = BLANK          2 + 2 = 2
 *   ALLOWED   + FORBIDDEN = FORBIDDEN      1 + 0 = 0
 *   BLANK     + FORBIDDEN = FORBIDDEN      2 + 0 = 0
 *   FORBIDDEN + FORBIDDEN = FORBIDDEN      0 + 0 = 0
 * </pre>
 *
 * @return the permission that results from the above rules
 * <p>
 * Note: there are two versions of add().  One adds two Privilege
 * objects.  The other just adds permissions (as returned by
 * <a href="#getPermission">getPermission()</a>).
 */
/* XXX: Whenever you change the value of FORBIDDEN, ALLOWED and BLANK
 * make sure they obey the laws mentioned in add() method.
 */
inline nsPermissionState nsPrivilege::add(nsPermissionState p1, nsPermissionState p2)
{
  if (p1 < p2)
    return p1;
  else 
    return p2;
}

inline nsPrivilege * nsPrivilege::add(nsPrivilege *p1, nsPrivilege *p2)
{
  if (p1->itsPerm < p2->itsPerm)
    return p1;
  else
    return p2;
}


#endif /* _NS_PRIVILEGE_H_ */
