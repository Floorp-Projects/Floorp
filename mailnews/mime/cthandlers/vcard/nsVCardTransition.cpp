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

#include "prtypes.h"
#include "prmem.h"
#include "plstr.h"
#include "nsCRT.h"
#include "nsVCardTransition.h"

extern "C" int MK_OUT_OF_MEMORY = 100;
extern "C" int MK_ADDR_HOSTNAMEIP = 102;
extern "C" int MK_ADDR_SPECIFIC_DLS = 103;
extern "C" int MK_ADDR_DEFAULT_DLS = 104;
extern "C" int MK_ADDR_CONFINFO = 105;
extern "C" int MK_LDAP_USEHTML = 106;
extern "C" int MK_MSG_ADD_TO_ADDR_BOOK = 107;
extern "C" int MK_ADDR_VIEW_CONDENSED_VCARD = 108;
extern "C" int MK_ADDR_VIEW_COMPLETE_VCARD = 109;
extern "C" int MK_LDAP_PHONE_NUMBER = 110;
extern "C" int MK_LDAP_BBS_TYPE = 111;
extern "C" int MK_LDAP_PAGER_TYPE = 112;
extern "C" int MK_LDAP_CELL_TYPE = 113;
extern "C" int MK_LDAP_MSG_TYPE = 114;
extern "C" int MK_LDAP_FAX_TYPE = 115;
extern "C" int MK_LDAP_VOICE_TYPE = 116;
extern "C" int MK_LDAP_WORK_TYPE = 117;
extern "C" int MK_LDAP_HOME_TYPE = 118;
extern "C" int MK_LDAP_PREF_TYPE = 119;
extern "C" int MK_LDAP_VERSION = 120;
extern "C" int MK_LDAP_SOUND = 121;
extern "C" int MK_LDAP_REVISION = 122;
extern "C" int MK_LDAP_SECRETARY = 123;
extern "C" int MK_LDAP_LOGO = 124;
extern "C" int MK_LDAP_ROLE = 125;
extern "C" int MK_LDAP_GEO = 126;
extern "C" int MK_LDAP_TZ = 127;
extern "C" int MK_LDAP_MAILER = 128;
extern "C" int MK_LDAP_MIDDLE_NAME = 129;
extern "C" int MK_LDAP_NAME_SUFFIX = 130;
extern "C" int MK_LDAP_NAME_PREFIX = 131;
extern "C" int MK_LDAP_GIVEN_NAME = 132;
extern "C" int MK_LDAP_SURNAME = 133;
extern "C" int MK_LDAP_EMAIL_ADDRESS = 134;
extern "C" int MK_LDAP_LABEL = 135;
extern "C" int MK_LDAP_BIRTHDAY = 136;
extern "C" int MK_LDAP_PHOTOGRAPH = 137;
extern "C" int MK_LDAP_PARCEL_TYPE = 138;
extern "C" int MK_LDAP_POSTAL_TYPE = 139;
extern "C" int MK_LDAP_INTL_TYPE = 140;
extern "C" int MK_LDAP_DOM_TYPE = 141;
extern "C" int MK_LDAP_X400 = 142;
extern "C" int MK_LDAP_TLX_TYPE = 143;
extern "C" int MK_LDAP_PRODIGY_TYPE = 144;
extern "C" int MK_LDAP_POWERSHARE_TYPE = 145;
extern "C" int MK_LDAP_MCIMAIL_TYPE = 146;
extern "C" int MK_LDAP_IBMMAIL_TYPE = 147;
extern "C" int MK_LDAP_INTERNET_TYPE = 148;
extern "C" int MK_LDAP_EWORLD_TYPE = 149;
extern "C" int MK_LDAP_CSI_TYPE = 150;
extern "C" int MK_LDAP_ATTMAIL_TYPE = 151;
extern "C" int MK_LDAP_APPLELINK_TYPE = 152;
extern "C" int MK_LDAP_AOL_TYPE = 153;
extern "C" int MK_LDAP_REGION = 154;
extern "C" int MK_LDAP_MODEM_TYPE = 155;
extern "C" int MK_LDAP_CAR_TYPE = 156;
extern "C" int MK_LDAP_ISDN_TYPE = 157;
extern "C" int MK_LDAP_VIDEO_TYPE = 158;
extern "C" int MK_LDAP_KEY = 159;
extern "C" int MK_LDAP_ADDRESS = 160;
extern "C" int MK_LDAP_UPDATEURL = 161;
extern "C" int MK_LDAP_COOLTALKADDRESS = 162;
extern "C" int MK_ADDR_ADDINFO = 163;

/* 
 * This is a transitional file that will help with the various message
 * ID definitions and the functions to retrieve the error string
 */
extern "C" char 
*XP_GetString (int i)
{
  if (i == MK_LDAP_REGION) return PL_strdup("State");
  if (i == MK_LDAP_DOM_TYPE) return PL_strdup("Domestic");
  if (i == MK_LDAP_INTL_TYPE) return PL_strdup("International");
  if (i == MK_LDAP_POSTAL_TYPE) return PL_strdup("Postal");
  if (i == MK_LDAP_PARCEL_TYPE) return PL_strdup("Parcel");
  if (i == MK_LDAP_WORK_TYPE) return PL_strdup("Work");
  if (i == MK_LDAP_HOME_TYPE) return PL_strdup("Home");
  if (i == MK_LDAP_PREF_TYPE) return PL_strdup("Preferred");
  if (i == MK_LDAP_VOICE_TYPE) return PL_strdup("Voice");
  if (i == MK_LDAP_FAX_TYPE) return PL_strdup("Fax");
  if (i == MK_LDAP_MSG_TYPE) return PL_strdup("Message");
  if (i == MK_LDAP_CELL_TYPE) return PL_strdup("Cellular");
  if (i == MK_LDAP_PAGER_TYPE) return PL_strdup("Pager");
  if (i == MK_LDAP_BBS_TYPE) return PL_strdup("BBS");
  if (i == MK_LDAP_MODEM_TYPE) return PL_strdup("Modem");
  if (i == MK_LDAP_CAR_TYPE) return PL_strdup("Car");
  if (i == MK_LDAP_ISDN_TYPE) return PL_strdup("ISDN");
  if (i == MK_LDAP_VIDEO_TYPE) return PL_strdup("Video");
  if (i == MK_LDAP_AOL_TYPE) return PL_strdup("AOL");
  if (i == MK_LDAP_APPLELINK_TYPE) return PL_strdup("Applelink");
  if (i == MK_LDAP_ATTMAIL_TYPE) return PL_strdup("AT&T Mail");
  if (i == MK_LDAP_CSI_TYPE) return PL_strdup("Compuserve");
  if (i == MK_LDAP_EWORLD_TYPE) return PL_strdup("eWorld");
  if (i == MK_LDAP_INTERNET_TYPE) return PL_strdup("Internet");
  if (i == MK_LDAP_IBMMAIL_TYPE) return PL_strdup("IBM Mail");
  if (i == MK_LDAP_MCIMAIL_TYPE) return PL_strdup("MCI Mail");
  if (i == MK_LDAP_POWERSHARE_TYPE) return PL_strdup("Powershare");
  if (i == MK_LDAP_PRODIGY_TYPE) return PL_strdup("Prodigy");
  if (i == MK_LDAP_TLX_TYPE) return PL_strdup("Telex");
  if (i == MK_LDAP_MIDDLE_NAME) return PL_strdup("Additional Name");
  if (i == MK_LDAP_NAME_PREFIX) return PL_strdup("Prefix");
  if (i == MK_LDAP_NAME_SUFFIX) return PL_strdup("Suffix");
  if (i == MK_LDAP_TZ) return PL_strdup("Time Zone");
  if (i == MK_LDAP_GEO) return PL_strdup("Geographic Position");
  if (i == MK_LDAP_SOUND) return PL_strdup("Sound");
  if (i == MK_LDAP_REVISION) return PL_strdup("Revision");
  if (i == MK_LDAP_VERSION) return PL_strdup("Version");
  if (i == MK_LDAP_KEY) return PL_strdup("Public Key");
  if (i == MK_LDAP_LOGO) return PL_strdup("Logo");
  if (i == MK_LDAP_BIRTHDAY) return PL_strdup("Birthday");
  if (i == MK_LDAP_X400) return PL_strdup("X400");
  if (i == MK_LDAP_ADDRESS) return PL_strdup("Address");
  if (i == MK_LDAP_LABEL) return PL_strdup("Label");
  if (i == MK_LDAP_MAILER) return PL_strdup("Mailer");
  if (i == MK_LDAP_ROLE) return PL_strdup("Role");
  if (i == MK_LDAP_UPDATEURL) return PL_strdup("Update From");
  if (i == MK_LDAP_COOLTALKADDRESS) return PL_strdup("Conference Address");
  if (i == MK_LDAP_USEHTML) return PL_strdup("HTML Mail");
  if ( i == MK_MSG_ADD_TO_ADDR_BOOK) return PL_strdup("Add to Personal Address Book");
  if (i == MK_ADDR_ADDINFO) return PL_strdup("Additional Information:");
  if (i == MK_ADDR_VIEW_COMPLETE_VCARD) return PL_strdup("View Complete Card");
  if (i == MK_ADDR_VIEW_CONDENSED_VCARD) return PL_strdup("View Condensed Card");
  return PL_strdup("Unknown");
}

/*	Very similar to strdup except it free's too
 */
extern "C" char * 
vCard_SACopy (char **destination, const char *source)
{
  if(*destination)
  {
    PR_Free(*destination);
    *destination = 0;
  }
  if (! source)
  {
    *destination = NULL;
  }
  else 
  {
    *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
    if (*destination == NULL) 
      return(NULL);
    
    PL_strcpy (*destination, source);
  }
  return *destination;
}

/*  Again like strdup but it concatinates and free's and uses Realloc
*/
extern "C"  char *
vCard_SACat (char **destination, const char *source)
{
  if (source && *source)
  {
    if (*destination)
    {
      int length = PL_strlen (*destination);
      *destination = (char *) PR_Realloc (*destination, length + PL_strlen(source) + 1);
      if (*destination == NULL)
        return(NULL);
      
      PL_strcpy (*destination + length, source);
    }
    else
    {
      *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
      if (*destination == NULL)
        return(NULL);
      
      PL_strcpy (*destination, source);
    }
  }
  return *destination;
}
