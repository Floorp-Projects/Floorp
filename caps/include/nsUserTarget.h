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

#ifndef _NS_USER_TARGET_H_
#define _NS_USER_TARGET_H_


#include "prtypes.h"
#include "nsTarget.h"
#include "nsPrivilege.h"
#include "nsPrivilegeManager.h"
#include "nsCom.h"

/* Any methods that is not defined here is in Java */
class nsUserTarget : public nsTarget {

public:

	/* Public Methods */

	nsUserTarget(char *name);
	virtual ~nsUserTarget(void);

	nsUserTarget(char *name, nsIPrincipal * prin, PRInt32 risk, 
					char * riskColor, char * description, 
					char * detailDescription, char * url)
	: nsTarget(name, prin, risk, riskColor, description, detailDescription, url, NULL)
	{
	}

	nsUserTarget(char *name, nsIPrincipal * prin, PRInt32 risk, 
					char *riskColor, char *description, 
					char *detailDescription, char *url, 
					nsTargetArray * targetArray) 
	: nsTarget(name, prin, risk, riskColor, description, detailDescription, url, targetArray)
	{
	}

	nsUserTarget(char *name, nsIPrincipal * prin, PRInt32 risk, 
					char *riskColor, 
					int desc_id, 
					int detail_desc_id,
					int help_url_id)
	: nsTarget(name, prin, risk, riskColor, desc_id, detail_desc_id, 
					help_url_id, NULL)
	{
	}

	nsUserTarget(char *name, nsIPrincipal * prin, PRInt32 risk, 
					char *riskColor, 
					int desc_id, 
					int detail_desc_id,
					int help_url_id,
					nsTargetArray * targetArray) 
	: nsTarget(name, prin, risk, riskColor, desc_id, detail_desc_id, 
					help_url_id, targetArray)
	{
	}

	nsIPrivilege * EnablePrivilege(nsIPrincipal * prin, void *data);

private:

};

#endif /* _NS_USER_TARGET_H_ */
