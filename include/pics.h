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

#ifndef PICS_H
#define PICS_H

typedef struct {
   char    *service;
   XP_Bool  generic;
   char    *fur;     /* means 'for' */
   XP_List *ratings;
} PICS_RatingsStruct;

typedef struct {
   char 	    *name;
   double 	     value;
} PICS_RatingValue;

typedef enum {
   PICS_RATINGS_PASSED,
   PICS_RATINGS_FAILED,
   PICS_NO_RATINGS
} PICS_PassFailReturnVal;

void PICS_FreeRatingsStruct(PICS_RatingsStruct *rs);

/* return NULL or ratings struct */
PICS_RatingsStruct * PICS_ParsePICSLable(char * label);

/* returns TRUE if page should be censored
 * FALSE if page is allowed to be shown
 */
PICS_PassFailReturnVal PICS_CompareToUserSettings(PICS_RatingsStruct *rs, char *cur_page_url);

XP_Bool PICS_IsPICSEnabledByUser(void);

XP_Bool PICS_AreRatingsRequired(void);

/* returns a URL string from a RatingsStruct
 * that includes the service URL and rating info
 */
char * PICS_RStoURL(PICS_RatingsStruct *rs, char *cur_page_url);

void PICS_Init(MWContext *context);

XP_Bool PICS_CanUserEnableAdditionalJavaCapabilities(void);

XP_Bool PICS_CheckForValidTreeRating(char *url_address);


#endif /* PICS_H */
