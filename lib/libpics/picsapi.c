/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*
 * Glue code to make PICS work in the client.  This is just a thin
 * layer between the W3C code and our layout engine to digest PICS labels.
 * Coded by Lou Montulli
 */

#include "xp.h"
#include "cslutils.h"
#include "csll.h"
#include "csllst.h"
#include "pics.h"
#include "prefapi.h"
#include "xpgetstr.h"
#include "sechash.h"
#include "base64.h"

extern int XP_ALERT_PROMPT_JAVA_CAPIBILITIES_PASSWORD;
extern int XP_ALERT_PROMPT_JAVA_CAPIBILITIES_PASSWORD_FAILED_ONCE;

typedef struct {
	PICS_RatingsStruct * rs;
	XP_Bool rs_invalid;
} ClosureData;

PRIVATE StateRet_t 
target_callback(CSLabel_t *pCSLabel,
		CSParse_t * pCSParse,
 		CSLLTC_t target, XP_Bool closed,
 		void * pClosure)
{
	char * ratingname;
	char * ratingstr;
	ClosureData *cd = (ClosureData *)pClosure;
	
	/* closed signifies that the parsing is done for that label */
    if(!cd || !closed)
    	return StateRet_OK;

    if(target == CSLLTC_SINGLE)
    {
        LabelOptions_t * lo = CSLabel_getLabelOptions(pCSLabel);

		if(lo)
		{
			if(lo->generic.state)
			{
				cd->rs->generic = TRUE;
			}

			if(lo->fur.value && !cd->rs->fur)
			{
				StrAllocCopy(cd->rs->fur, lo->fur.value);
			}
		}
    }
	else if(target ==  CSLLTC_RATING)
	{
    	PICS_RatingsStruct *rating_struct = cd->rs;
        LabelOptions_t * lo = CSLabel_getLabelOptions(pCSLabel);

        ratingstr = CSLabel_getRatingStr(pCSLabel);
		ratingname =  CSLabel_getRatingName(pCSLabel);

		if(ratingname)
		{
			LabelRating_t * label_rating;
			ServiceInfo_t * service_info;

			service_info = CSLabel_getServiceInfo(pCSLabel);

			if(service_info && !rating_struct->service)
			{
			   rating_struct->service = XP_STRDUP(service_info->rating_service.value); 
			}

			label_rating = CSLabel_getLabelRating(pCSLabel);

			if(label_rating)
			{
                double value;
				PICS_RatingValue *rating_value = XP_NEW_ZAP(PICS_RatingValue);

                value = label_rating->value.value;
					
				if(rating_value)
				{
					rating_value->value = label_rating->value.value;
					rating_value->name = XP_STRDUP(label_rating->identifier.value);

					if(rating_value->name)
					{
						/* insert it into the list */
						XP_ListAddObject(rating_struct->ratings, rating_value);
					}
					else
					{
						/* error, cleanup */
						XP_FREE(rating_value);
					}
				}
            }
		}

	}
	return StateRet_OK;
}

PRIVATE StateRet_t 
parse_error_handler(CSLabel_t * pCSLabel, CSParse_t * pCSParse,
                             const char * token, char demark,
                             StateRet_t errorCode)
{
	return errorCode;
}

/* return NULL or ratings struct */
PUBLIC PICS_RatingsStruct * 
PICS_ParsePICSLable(char * label)
{
	CSParse_t *CSParse_handle;
    ClosureData *cd;
	PICS_RatingsStruct *rs;
    CSDoMore_t status;

	if(!label)
		return NULL;

	cd = XP_NEW_ZAP(ClosureData);

	if(!cd)
		return NULL;

	rs = XP_NEW_ZAP(PICS_RatingsStruct);

	if(!rs)
	{
		XP_FREE(cd);
		return NULL;
	}

    rs->ratings = XP_ListNew();

	cd->rs = rs;

	/* parse pics label using w3c api */

	CSParse_handle = CSParse_newLabel(&target_callback, &parse_error_handler);

	if(!CSParse_handle)
		return NULL;

	do {

		status = CSParse_parseChunk(CSParse_handle, label, XP_STRLEN(label), cd);

	} while(status == CSDoMore_more);

	if(cd->rs_invalid)
	{
		PICS_FreeRatingsStruct(rs);
		rs = NULL;
	}

	XP_FREE(cd);

    CSParse_deleteLabel(CSParse_handle);

	return(rs);
}

PUBLIC void
PICS_FreeRatingsStruct(PICS_RatingsStruct *rs)
{
	if(rs)
	{
        PICS_RatingValue *rv;
        
        while((rv = XP_ListRemoveTopObject(rs->ratings)) != NULL)
        {
            XP_FREE(rv->name);
            XP_FREE(rv);
        }

		XP_FREE(rs);
	}
}

#define PICS_DOMAIN  			"browser.PICS."
#define PICS_ENABLED_PREF 		PICS_DOMAIN"ratings_enabled"
#define PICS_MUST_BE_RATED_PREF PICS_DOMAIN"pages_must_be_rated"
#define PICS_DISABLED_FOR_SESSION PICS_DOMAIN"disable_for_this_session"
#define PICS_REENABLE_FOR_SESSION PICS_DOMAIN"reenable_for_this_session"

#define JAVA_SECURITY_PASSWORD "signed.applets.capabilitiesDB.password"

Bool pics_ratings_enabled = FALSE;
Bool pics_pages_must_be_rated_pref = FALSE;
Bool pics_disabled_for_this_session = FALSE;
int pics_violence_pref = 0;
int pics_sexual_pref = 0;
int pics_language_pref = 0;
int pics_nudity_pref = 0;

/* if TRUE the user can allow additional java and JS
 * capibilities.  UniversalFileWrite, etc.
 */
Bool pics_java_capabilities_enabled = TRUE;

int PR_CALLBACK
pics_pref_change(const char *pref_name, void *closure)
{
    XP_Bool bool_rv;

    if(!PREF_GetBoolPref(PICS_ENABLED_PREF, &bool_rv))
        pics_ratings_enabled = bool_rv;
    if(!PREF_GetBoolPref(PICS_MUST_BE_RATED_PREF, &bool_rv))
        pics_pages_must_be_rated_pref = bool_rv;
    if(!PREF_GetBoolPref(PICS_DISABLED_FOR_SESSION, &bool_rv))
	{
		if(bool_rv)
		{
        	pics_disabled_for_this_session = TRUE;
    		PREF_SetBoolPref(PICS_DISABLED_FOR_SESSION, FALSE);
		}
	}
    if(!PREF_GetBoolPref(PICS_REENABLE_FOR_SESSION, &bool_rv))
	{
		if(bool_rv)
		{
        	pics_disabled_for_this_session = FALSE;
    		PREF_SetBoolPref(PICS_REENABLE_FOR_SESSION, FALSE);
		}
	}

    return 0;
}

PRIVATE char *
pics_hash_password(char *pw)
{
        SECStatus status;
        unsigned char result[SHA1_LENGTH];

        status = SHA1_HashBuf(result, (unsigned char *)pw, XP_STRLEN(pw));

	if (status != SECSuccess)
		return NULL;

	return(BTOA_DataToAscii(result, SHA1_LENGTH));
}

PUBLIC void
PICS_Init(MWContext *context)
{
    static XP_Bool first_time=TRUE;

    if(!first_time)
    {
        return;
    }
    else
    {
	    char *password=NULL;

        first_time = FALSE;

	    /* get the prefs */
	    pics_pref_change(PICS_DOMAIN, NULL);

        PREF_RegisterCallback(PICS_DOMAIN, pics_pref_change, NULL);

	    /* check for security pref that password disables the enableing of
	     * java permissions
	     */
        if(PREF_CopyCharPref(JAVA_SECURITY_PASSWORD, &password))
		    password = NULL;

	    if(password && *password)
	    {
		    /* get prompt string from registry
		     */
		    char *prompt_string = XP_GetString(XP_ALERT_PROMPT_JAVA_CAPIBILITIES_PASSWORD);
		    char *user_password;
		    char *hashed_password;

prompt_again:

		    /* prompt the user for the password
		     */
		    user_password = FE_PromptPassword(context, prompt_string);

		    /* ### one-way hash password */
		    if(user_password)
		    {
				hashed_password = pics_hash_password(user_password);
		    }
            else
            {
                hashed_password = NULL;
            }

		    if(!hashed_password)
            {
                pics_java_capabilities_enabled = FALSE;
            }
            else if(!XP_STRCMP(hashed_password, password))
		    {
			    pics_java_capabilities_enabled = TRUE;
		    }
		    else
		    {
                XP_FREE(user_password);
                XP_FREE(hashed_password);
       		    prompt_string = XP_GetString(XP_ALERT_PROMPT_JAVA_CAPIBILITIES_PASSWORD_FAILED_ONCE);
                goto prompt_again;
		    }

		    XP_FREEIF(user_password);
		    XP_FREEIF(hashed_password);
		    
	    }
	    
	    XP_FREEIF(password);
    }
}

PUBLIC XP_Bool
PICS_CanUserEnableAdditionalJavaCapabilities(void)
{
	return(pics_java_capabilities_enabled);
}

PUBLIC XP_Bool
PICS_IsPICSEnabledByUser(void)
{
	/* short circuit */
	if(pics_disabled_for_this_session)
		return FALSE;

    return(pics_ratings_enabled);
}

PUBLIC XP_Bool
PICS_AreRatingsRequired(void)
{
	return pics_pages_must_be_rated_pref;
}

PRIVATE char *
illegal_to_underscore(char *string)
{
    char* ptr = string;

    if(!string)
       return NULL;

    if(!XP_IS_ALPHA(*ptr))
           *ptr = '_';

    for(ptr++; *ptr; ptr++)
        if(!XP_IS_ALPHA(*ptr) && !XP_IS_DIGIT(*ptr))
           *ptr = '_';

    return string;
}

PRIVATE char *
lowercase_string(char *string)
{
    char *ptr = string;

    if(!string)
        return NULL;

    for(; *ptr; ptr++)
        *ptr = XP_TO_LOWER(*ptr);

    return string;
}

#define PICS_URL_PREFIX "about:pics"

/* returns a URL string from a RatingsStruct
 * that includes the service URL and rating info
 */
PUBLIC char *
PICS_RStoURL(PICS_RatingsStruct *rs, char *cur_page_url)
{
    char *rv; 
    char *escaped_cur_page=NULL;

    if(cur_page_url)
    {
        escaped_cur_page = NET_Escape(cur_page_url, URL_PATH);    
        if(!escaped_cur_page)
            return NULL;
    }

    rv = PR_smprintf("%s?Destination=%s", 
                     PICS_URL_PREFIX, 
                     escaped_cur_page ? escaped_cur_page : "none");

    XP_FREE(escaped_cur_page);

    if(!rs || !rs->service)
    {
        StrAllocCat(rv, "&NO_RATING");
        return(rv);
    }
    else
    {
		XP_List *list_ptr = rs->ratings;
		PICS_RatingValue *rating_value;
    	char *escaped_service = NET_Escape(rs->service, URL_PATH);    

    	if(!escaped_service)
        	return NULL;

        StrAllocCat(rv, "&Service=");
        StrAllocCat(rv, escaped_service);

        XP_FREE(escaped_service);

        while((rating_value = XP_ListNextObject(list_ptr)) != NULL)
        {

            char *add;
            char *escaped_name = NET_Escape(rating_value->name, URL_PATH);

            if(!escaped_name)
            {
                XP_FREE(rv); 
                return NULL;
            }
            
            add = PR_smprintf("&%s=%f", escaped_name, rating_value->value);

            XP_FREE(escaped_name);

            StrAllocCat(rv, add);

            XP_FREE(add);
        }

        return rv;
    }

    XP_ASSERT(0); /* should never get here */
    return NULL;
}

XP_List *pics_tree_ratings=NULL;

PRIVATE void
pics_add_rs_to_tree_ratings(PICS_RatingsStruct *rs)
{
	char *path;

	if(!pics_tree_ratings)
	{
		pics_tree_ratings = XP_ListNew();
		if(!pics_tree_ratings)
			return;
	}

	if(!rs->fur || !rs->generic)
		return;   /* doesn't belong here */

	/* make sure it's not in the list already */
	if(PICS_CheckForValidTreeRating(rs->fur))
		return;

	/* make sure the fur address smells like a URL and has
	 * a real host name (at least two dots) 
	 *
	 * reject "http://" or "http://www" 
	 */
	if(!NET_URL_Type(rs->fur))
		return;

	path = NET_ParseURL(rs->fur, GET_PATH_PART);

	/* if it has a path it's ok */
	if(!path || !*path)
	{
		/* if it doesn't have a path it needs at least two dots */
		char *ptr;
		char *hostname = NET_ParseURL(rs->fur, GET_HOST_PART);

		if(!hostname)
			return;

		if(!(ptr = XP_STRCHR(hostname, '.'))
			|| !XP_STRCHR(ptr+1, '.'))
		{
			XP_FREE(hostname);
			XP_FREEIF(path);
			return;
		} 

		XP_FREE(hostname);
	}

	XP_FREE(path);

	XP_ListAddObject(pics_tree_ratings, rs->fur);

	return;
}

PUBLIC XP_Bool
PICS_CheckForValidTreeRating(char *url_address)
{
	XP_List *list_ptr;
	char *valid_tree;

	if(!pics_tree_ratings)
		return FALSE;

	list_ptr = pics_tree_ratings;

	while((valid_tree = XP_ListNextObject(list_ptr)))
	{
		if(!XP_STRNCASECMP(url_address, valid_tree, XP_STRLEN(valid_tree)))
			return TRUE;
	}

	return FALSE;
}

/* returns TRUE if page should be censored
 * FALSE if page is allowed to be shown
 */
PUBLIC PICS_PassFailReturnVal
PICS_CompareToUserSettings(PICS_RatingsStruct *rs, char *cur_page_url)
{
	int32 int_pref;
	XP_Bool bool_pref;
	char * pref_prefix;
	char * pref_string=NULL;
	char * escaped_service;
	PICS_PassFailReturnVal rv = PICS_RATINGS_PASSED;
    XP_List *list_ptr;
    PICS_RatingValue *rating_value;

	if(!rs || !rs->service)
	{
		return PICS_NO_RATINGS;
	}

#define PICS_SERVICE_DOMAIN   PICS_DOMAIN"service."
#define PICS_SERVICE_ENABLED  "service_enabled"

	/* cycle through list of ratings and compare to the users prefs */
	list_ptr = rs->ratings;
	pref_prefix = XP_STRDUP(PICS_SERVICE_DOMAIN);

    /* need to deal with bad characters */
	escaped_service = XP_STRDUP(rs->service);
    escaped_service = illegal_to_underscore(escaped_service);
    escaped_service = lowercase_string(escaped_service);

	if(!escaped_service)
		return PICS_RATINGS_FAILED;

	StrAllocCat(pref_prefix, escaped_service);

	XP_FREE(escaped_service);

	if(!pref_prefix)
		return PICS_RATINGS_FAILED;

	/* verify that this type of rating system is enabled */
	pref_string = PR_smprintf("%s.%s", pref_prefix, PICS_SERVICE_ENABLED);
	
    if(!pref_string)
        goto cleanup;

	if(!PREF_GetBoolPref(pref_string, &bool_pref))
	{
		if(!bool_pref)
		{
			/* this is an unenabled ratings service */
			rv = PICS_NO_RATINGS;
            XP_FREE(pref_string);
			goto cleanup;
		}
	}
	else
	{
		/* this is an unsupported ratings service */
		rv = PICS_NO_RATINGS;
        XP_FREE(pref_string);
		goto cleanup;
	}

    XP_FREE(pref_string);

	while((rating_value = XP_ListNextObject(list_ptr)) != NULL)
	{
		/* compose pref lookup string */
		pref_string = PR_smprintf("%s.%s", 
                                pref_prefix, 
                                illegal_to_underscore(rating_value->name));

		if(!pref_string)
			goto cleanup;

		/* find the value in the prefs if it exists
		 * if it does compare it to the value given and if
		 * less than, censer the page.
		 */
		if(!PREF_GetIntPref(pref_string, &int_pref))
		{
			if(rating_value->value > int_pref)
			{
				rv = PICS_RATINGS_FAILED;
                XP_FREE(pref_string);
				goto cleanup;
			}
		}

		XP_FREE(pref_string);
	}

cleanup:

	XP_FREE(pref_prefix);

	/* make sure this rating applies to this page */
	if(rs->fur)
	{
		if(XP_STRNCASECMP(cur_page_url, rs->fur, XP_STRLEN(rs->fur)))
			rv = PICS_NO_RATINGS;
	}

	if(rv == PICS_RATINGS_PASSED && rs->generic)
	{
		/* rating should apply to a whole tree, add to list */
		pics_add_rs_to_tree_ratings(rs);		
	}

	return rv;
}

