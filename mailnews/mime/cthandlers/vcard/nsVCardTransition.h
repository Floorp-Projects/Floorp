/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#ifndef nsMimeTransition_h_
#define nsMimeTransition_h

#include "prtypes.h"
#include "plstr.h"
#include "net.h"

extern "C" int MK_OUT_OF_MEMORY;
extern "C" int MK_ADDR_HOSTNAMEIP;
extern "C" int MK_ADDR_SPECIFIC_DLS;
extern "C" int MK_ADDR_DEFAULT_DLS;
extern "C" int MK_ADDR_CONFINFO;
extern "C" int MK_LDAP_USEHTML;
extern "C" int MK_MSG_ADD_TO_ADDR_BOOK;
extern "C" int MK_ADDR_VIEW_CONDENSED_VCARD;
extern "C" int MK_ADDR_VIEW_COMPLETE_VCARD;
extern "C" int MK_LDAP_PHONE_NUMBER;
extern "C" int MK_LDAP_BBS_TYPE;
extern "C" int MK_LDAP_PAGER_TYPE;
extern "C" int MK_LDAP_CELL_TYPE;
extern "C" int MK_LDAP_MSG_TYPE;
extern "C" int MK_LDAP_FAX_TYPE;
extern "C" int MK_LDAP_VOICE_TYPE;
extern "C" int MK_LDAP_WORK_TYPE;
extern "C" int MK_LDAP_HOME_TYPE;
extern "C" int MK_LDAP_PREF_TYPE;
extern "C" int MK_LDAP_VERSION;
extern "C" int MK_LDAP_SOUND;
extern "C" int MK_LDAP_REVISION;
extern "C" int MK_LDAP_SECRETARY;
extern "C" int MK_LDAP_LOGO;
extern "C" int MK_LDAP_ROLE;
extern "C" int MK_LDAP_GEO;
extern "C" int MK_LDAP_TZ;
extern "C" int MK_LDAP_MAILER;
extern "C" int MK_LDAP_MIDDLE_NAME;
extern "C" int MK_LDAP_NAME_SUFFIX;
extern "C" int MK_LDAP_NAME_PREFIX;
extern "C" int MK_LDAP_GIVEN_NAME;
extern "C" int MK_LDAP_SURNAME;
extern "C" int MK_LDAP_EMAIL_ADDRESS;
extern "C" int MK_LDAP_LABEL;
extern "C" int MK_LDAP_BIRTHDAY;
extern "C" int MK_LDAP_PHOTOGRAPH;
extern "C" int MK_LDAP_PARCEL_TYPE;
extern "C" int MK_LDAP_POSTAL_TYPE;
extern "C" int MK_LDAP_INTL_TYPE;
extern "C" int MK_LDAP_DOM_TYPE;
extern "C" int MK_LDAP_X400;
extern "C" int MK_LDAP_TLX_TYPE;
extern "C" int MK_LDAP_PRODIGY_TYPE;
extern "C" int MK_LDAP_POWERSHARE_TYPE;
extern "C" int MK_LDAP_MCIMAIL_TYPE;
extern "C" int MK_LDAP_IBMMAIL_TYPE;
extern "C" int MK_LDAP_INTERNET_TYPE;
extern "C" int MK_LDAP_EWORLD_TYPE;
extern "C" int MK_LDAP_CSI_TYPE;
extern "C" int MK_LDAP_ATTMAIL_TYPE;
extern "C" int MK_LDAP_APPLELINK_TYPE;
extern "C" int MK_LDAP_AOL_TYPE;
extern "C" int MK_LDAP_REGION;
extern "C" int MK_LDAP_MODEM_TYPE;
extern "C" int MK_LDAP_CAR_TYPE;
extern "C" int MK_LDAP_ISDN_TYPE;
extern "C" int MK_LDAP_VIDEO_TYPE;
extern "C" int MK_LDAP_KEY;
extern "C" int MK_LDAP_ADDRESS;
extern "C" int MK_LDAP_UPDATEURL;
extern "C" int MK_LDAP_COOLTALKADDRESS;
extern "C" int MK_ADDR_ADDINFO;

/* 
 * This is a transitional file that will help with the various message
 * ID definitions and the functions to retrieve the error string
 */

extern "C" char 
*XP_GetString (int i);

/*	Very similar to strdup except it free's too
 */
extern "C" char * 
vCard_SACopy (char **destination, const char *source);

extern "C"  char *
vCard_SACat (char **destination, const char *source);

#endif /* nsMimeTransition_h_ */
