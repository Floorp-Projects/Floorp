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
#if defined(CookieManagement)
#define TRUST_LABELS 1
#endif



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

        status = HASH_HashBuf(HASH_AlgSHA1, result,
                              (unsigned char *)pw, XP_STRLEN(pw));

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
            char *escaped_name = NET_Escape(
                                     illegal_to_underscore(rating_value->name),
                                     URL_PATH);

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

#ifdef TRUST_LABELS
/***************************************************************************/
/***************************************************************************/
/***********  TRUST.C  at a latter time   **********************************/
/*********   break from here down into trust.c  ****************************/
/***************************************************************************/
/***************************************************************************/


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
/*
 * This is another version of the thin layer between the W3C code - which
 * parses the PICS labels - and the HTTP parsing code, to digest PICS labels.
 * The original version was coded by Lou Montulli, this version by Paul Chek.
 *
 * This code is called when the HTTP header is being parsed and a PICS label
 * was found.
 *
 * The broad overview is:
 *  1.  The PICS parsing engine parses the ENTIRE label creating structures and
 *		lists of the different rating services and labels in the services.
 *  2.  After the label is completely parsed we iterate thru the rating 
 *		services and the labels for each service to see if there are any trust
 *		labels.
 *	3.	If trust label are found they are added to the TrustList which is part
 *		of the URL_Struct struct.
 * 
 *  The two main functions for the trust label processing are:
 *  ProcessSingleLabel -  this is where the parsing engine presents ONE label with 
 * 						all its options and extensions.  ProcessSingleLabel examines
 * 						the label and determines if it is a valid trust label.  If
 * 						it is the label is added to the TrustList.  This list
 *						is the list of all trust labels.
 * 
 *  MatchCookieToLabel -	here is where a cookie gets matched to a trust label.  The
 * 						entire header for the response has been read.  The 
 *						end-of-header processing is pulling out the "Set-Cookie"
 *						entries.  It has retrieved a cookie for processing.  The 
 *						cookie processing code see that the user is processing
 *						cookies using a script and inorder to fill out the cookie
 *						properties for the script it has called MatchCookieToLabel
 *						to see if there is a trust label associated with the cookie.
 *						MatchCookieToLabel will look thru the trust_label list and 
 *						match the best label to the cookie.
 */

#include "xp.h"
/* uncomment out these .h files when trust.c is seperated. 
#include "csllst.h"
#include "cslutils.h"
#include "csll.h"
#include "pics.h"
#include "prefapi.h"
#include "xpgetstr.h"
#include "sechash.h"			  
#include "base64.h"
*/
#include "jscookie.h"
#include "mkaccess.h"		/* use #include "trust.h" when it is broken out of mkaccess.h */
#include "prlong.h"
#include "mkgeturl.h"


#ifdef _DEBUG
#define HTTrace OutputDebugString 
#else
#define HTTrace
#endif


#define PICS_VERSION	1.1		/* the version of PICS labels that this code expects */

CSLabel_callback_t ProcessService;
CSLabel_callback_t ProcessLabel;
CSLabel_callback_t ProcessSingleLabel;

PRBool IsValidValue( FVal_t *Value, int MinValue, int MaxValue, int *TheValue );
PRBool IsExpired( DVal_t *Value, PRTime *ExpirationDate );
PRBool IsValidTrustService( char *ServiceName );
PRBool ISODateToLocalTime( DVal_t *Value, PRTime *LocalDate );
PRBool CheckOptions( LabelOptions_t  *LabelOptions, PRTime *ExpirationDate );
PRBool IsLabelSigned( Extension_t *AExt );
PRBool IsSignatureValid( Extension_t *AExt, LabelOptions_t  *LabelOptions );

extern int NET_SameDomain(char * currentHost, char * inlineHost);		  
extern char * lowercase_string(char *string);
extern char * illegal_to_underscore(char *string);


#ifdef _DEBUG
/**************    FOR DEBUGGINGG   ***********************************/ 
#include "csparse.h"
LabelTargetCallback_t targetCallback;
StateRet_t trustCallback(CSLabel_t * pCSMR, CSParse_t * pCSParse, CSLLTC_t target, PRBool closed, void * pVoid)
{
#ifdef NOPE									  /* for debuggin */
	static int Total = 0;
    char space[256];
    int change = closed ? -target : target;
    Total += change;
	sprintf( space, "(%2d:%3d)token-->|%s|  %s %s \n", 
		   Total, closed ? -target : target, pCSParse->token->data,
		   closed ? "  ending" : "starting", pCSParse->pTargetObject->note ); 
	sprintf(space, "%s %s (%d)\n", closed ? "  ending" : "starting", pCSParse->pParseState->note, closed ? -target : target); 
    HTTrace(space);
	if ( Total == 0 ) HTTrace( "All Done\n" );
#endif
    return StateRet_OK;
}

/* LLErrorHandler_t parseErrorHandler; */
StateRet_t trustErrorHandler(CSLabel_t * pCSLabel, CSParse_t * pCSParse, 
			     const char * token, char demark, 
			     StateRet_t errorCode)
{
    char space[256];
    sprintf(space, "%20s - %s: ", pCSParse->pTargetObject->note, 
	   pCSParse->currentSubState == SubState_X ? "SubState_X" : 
	   pCSParse->currentSubState == SubState_N ? "SubState_N" : 
	   pCSParse->currentSubState == SubState_A ? "SubState_A" : 
	   pCSParse->currentSubState == SubState_B ? "SubState_B" : 
	   pCSParse->currentSubState == SubState_C ? "SubState_C" : 
	   pCSParse->currentSubState == SubState_D ? "SubState_D" : 
	   pCSParse->currentSubState == SubState_E ? "SubState_E" : 
	   pCSParse->currentSubState == SubState_F ? "SubState_F" : 
	   pCSParse->currentSubState == SubState_G ? "SubState_G" : 
	   pCSParse->currentSubState == SubState_H ? "SubState_H" : 
	   "???");
    HTTrace(space);

    switch (errorCode) {
        case StateRet_WARN_NO_MATCH:
            if (token)
	        sprintf(space, "Unexpected token \"%s\".\n", token);
	    else
	        sprintf(space, "Unexpected lack of token.\n");
            break;
        case StateRet_WARN_BAD_PUNCT:
			if (token)
				sprintf(space, "Unexpected punctuation |%c| after token \"%s\".\n", demark, token);
			else
				sprintf(space, "Unexpected punctuation |%c| \n", demark);
            break;
        case StateRet_ERROR_BAD_CHAR:
            sprintf(space, "Unexpected character \"%c\" in token \"%s\".\n", 
			    *pCSParse->pParseContext->pTokenError, token);
            break;
        default:
            sprintf(space, "Internal error: demark:\"%c\" token:\"%s\".\n", demark, token);
            break;
    }
    HTTrace(space);
/*
    CSLabel_dump(pCSMR);
    HTTrace(pParseState->note);
*/
  return errorCode;
}

/***************  END OF FOR DEBUGGINGG   *********************************** */
#endif

/*----------------------------------------------------------------------------------------------------
 *  Purpose: given the value part of a PICS label parse it and determine
 *			if it is a valid trust label according to internet draft
 *			draft-ietf-http-trust-state-mgt-02.txt
 * 
 *---------------------------------------------------------------------------------------------------- */
PUBLIC void PICS_ExtractTrustLabel(URL_Struct *URL_s, char *value )
{
	CSParse_t * pCSParse = 0;
    CSDoMore_t status;

#ifdef _DEBUG				/* display the label for debug */
	HTTrace( "\n" );
	HTTrace( value );
	HTTrace( "\n" );
#endif

	/* validate input args */
	if ( !URL_s || !value || *value == '\0' ) return;

	/* parse the PICS label and extract the trust label information from it */
	/* ignoring the other rating information. */
	/* init the PICS parsing code */
#ifdef _DEBUG
	pCSParse = CSParse_newLabel( &trustCallback, &trustErrorHandler );
#else
	pCSParse = CSParse_newLabel( 0, 0 );
#endif
	if ( pCSParse ) {
	/* parse the label - the entire label is in value.  So one call 
		to parse chunk is all that is needed.*/
		status = CSParse_parseChunk(pCSParse, value, XP_STRLEN(value), 0);
		
		/* was the parse sucessfull? */
		if ( status == CSDoMore_done ) {
		/* yes - Iterate thru the services looking for valid trust labels.
		*  the PICS label parsing code is set up to iterate thru the labels.
		*  the hierarchy is:
		* 	Services
		* 		Labels
		* 			SingleLabels
		* 				Label Options
			*				Label Ratings			   */
			CSLabel_t * pCSLabel = CSParse_getLabel( pCSParse );
			/* make sure the PICS version is correct */
			if ( PICS_VERSION <= FVal_value(&CSLabel_getCSLLData(pCSLabel)->version) ) {
				/* iterate thru each of the service IDs in this label calling ProcessService for each*/
				CSLabel_iterateServices( pCSLabel, ProcessService, NULL, 0, (void *)(URL_s));
				/* IF there are valid trust labels in this PICS label they are added to  */
				/* the trust list at TrustList */
			}
			
		} else {
		/* either there was a error parsing the label or the label wasnt complete.
			In either case ignore the label */
			HTTrace( "Invalid label\n" );
		}
		CSParse_deleteLabel(pCSParse);	/* free the label parsing structures */
	}
}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: the call back to handle a single service ID
 * 			
 *  
 *----------------------------------------------------------------------------------------------------*/
CSError_t ProcessService(CSLabel_t * pCSLabel, State_Parms_t * pParms, const char * identifier, void * URL_s)
{
	CSError_t ret = CSError_OK;
	if ( IsValidTrustService( CSLabel_getServiceName( pCSLabel ) ) ) {
		/* yes - passed the first test for trust labels, i.e. a valid trust service
		   I dont care about any of the service level options at this point; but if I 
		   did I would use LabelOptions_t Opt =  pServiceInfo->pLabelOptions to check them
		   
		   So iterate thru the labels which if they have the purpose, Recipients and identifiable ratings
		   will be trust labels																	   */
		ret = CSLabel_iterateLabels(pCSLabel, ProcessLabel, pParms, 0, URL_s);
	} else {
		/* if this is not a trust label but a regular PICS label it will usually 
		   fail the rating service.  So this is an expected occurance. */
		HTTrace( "ProcessService: invalid trust rating service\n" );
	}
    return ret;
}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: the call back to process a label and iterate thru the SingleLabels
 * 			
 *  
 *----------------------------------------------------------------------------------------------------*/
 CSError_t ProcessLabel(CSLabel_t * pCSLabel, State_Parms_t * pParms, const char * identifier, void * URL_s)
{
    return CSLabel_iterateSingleLabels(pCSLabel, ProcessSingleLabel, pParms, 0, URL_s);
}

void TL_SetTrustAuthority( TrustLabel *ALabel, char *TrustAuthority );
void TL_SetByField( TrustLabel *ALabel, char *szBy );
void TL_ProcessForAttrib( TrustLabel *ALabel, char *szFor);

/*----------------------------------------------------------------------------------------------------
 *  Purpose: the call back get the options and the ratings for a single label.
 * 		Create a TrustLabel to contain the info about the label and put on the 
 * 		the trust label list
 *  
 *  Input:
 *    pCSLabel - current parsed label instance
 *    URL_s  - the URL_Struct associated with this trust label from the HTTP header
 *  NOTES:
 * 	1.  If the same rating is seen twice in one single label, the first VALID instance is used.
 * 
 * History:
 *   9/10/98 Paul Chek - switch recipient back to supporting a single value.
 *						 NOTE: this is now controlled by RECIPIENT_RANGE
 *   8/22/98 Paul Chek - switch recipient to support a range of values
 *---------------------------------------------------------------------------------------------------- */
CSError_t ProcessSingleLabel(CSLabel_t * pCSLabel, State_Parms_t * pParms, const char * identifier, void * URL_s)
{
/* tracks the ratings I have seen */
#define HAVE_PURPOSE	0x1
#define HAVE_RECIPIENTS	0x2
#define HAVE_ID			0x4
	typedef struct {
		char *szName;			/* the full name of the rating */
		int  Min, Max;			/* the valid ranges for the rating */
	} ValidRating;

	/* the ratings I am interested in for a trust label  */
	ValidRating PurposeRating	= { "purpose", 0, 5 };
	ValidRating RecipientsRating	= { "recipients", 0, 3 };
	ValidRating IDRating		= { "identifiable", 0, 1 };

    CSError_t ret = CSError_OK;
	int PurposeRange = 0;				/* bits corresponding to the purpose ranges seen */
	int RecpRange = 0;
	int  IDValue = 0;

	HTList * ranges;
    Range_t * pRange;
	int TempValue, MinValue, MaxValue;
	int i;
	TrustLabel *TheLabel;

	Extension_t *AExt;
	ExtensionData_t	*AData;
	char	*AName;				
	PRBool	bForgedLabel;
	URL_Struct *TheURL_s;

	/* march thru the ratings looking for purpose, Recipients and identification.  When found save their values. */
	int count = 0;
	if (!pCSLabel || !URL_s ) {
		/* oops got a bad one */
		ret = CSError_BAD_PARAM;
	} else {
		/* the current single label */
		SingleLabel_t * pSingleLabel = CSLabel_getSingleLabel(pCSLabel);
		/* get the options */
		LabelOptions_t  *LabelOptions = pSingleLabel->pLabelOptions;
		PRTime  ExpDate;						/* the expiration date */

		/* VALIDITY TESTS */
		/* Has the expiration date passed - there must be an expiration date - sec 3.4.2 */
		/* and these options are required "by", "generic", "on" - sec 3.1? */
		if ( CheckOptions( LabelOptions, &ExpDate ) ) {
			/* yes - check the ratings for purpose, Recipients and ID */
			/* get the Ratings list */
			HTList *LabelRatings = pSingleLabel->labelRatings;
			LabelRating_t *Rating;
			PRBool bContinue = TRUE;
			short AllRatings = HAVE_PURPOSE | HAVE_RECIPIENTS | HAVE_ID;
			/* look thru all the ratings until I have the 3 required ratings */
			while ( AllRatings != 0 && (Rating = (LabelRating_t *) HTList_nextObject(LabelRatings))) {
				/* is it the purpose rating? */
				if ( (AllRatings & HAVE_PURPOSE) &&
					 !XP_STRCASECMP( SVal_value(&Rating->identifier), PurposeRating.szName)) {
					/* Was a single value given?? */
					if ( IsValidValue( &Rating->value, PurposeRating.Min, PurposeRating.Max, &TempValue ) ) {
						PurposeRange = PurposeRange | ( 1 << TempValue );
						AllRatings &= (~HAVE_PURPOSE );

					/* Was a range or a list of values given?? */
					} else if ( Rating->ranges ) {
						ranges = Rating->ranges;
						while ((pRange = (Range_t *) HTList_nextObject(ranges))) {
							MinValue = MaxValue = -1;
							if ( IsValidValue( &pRange->min, PurposeRating.Min, PurposeRating.Max, &MinValue ) ) {
								PurposeRange = PurposeRange | ( 1 << MinValue );
								AllRatings &= (~HAVE_PURPOSE );
							}
							if ( IsValidValue( &pRange->max, PurposeRating.Min, PurposeRating.Max, &MaxValue ) ) {
								/* if there was also a min value then set all the bits between the min and the max */
								if ( 0 <= MinValue && MinValue <= MaxValue ) {
									for( i=MinValue; i<=MaxValue; i++ ) PurposeRange = PurposeRange | ( 1 << i );
								} else {
									PurposeRange = PurposeRange | ( 1 << MaxValue );
								}
								AllRatings &= (~HAVE_PURPOSE );
							}
						}
					}
				/* is it the Recipients rating? */
				} else if ( (AllRatings & HAVE_RECIPIENTS) &&
							!XP_STRCASECMP(SVal_value(&Rating->identifier), RecipientsRating.szName)) {
#ifndef RECIPIENT_RANGE
					/* yes - is the range valid?*/
					if ( IsValidValue( &Rating->value, RecipientsRating.Min, RecipientsRating.Max, &RecpRange ) ) {
						AllRatings &= (~HAVE_RECIPIENTS );
					}
#else
					/* Was a single value given?? */
					if ( IsValidValue( &Rating->value, RecipientsRating.Min, RecipientsRating.Max, &TempValue ) ) {
						RecpRange = RecpRange | ( 1 << TempValue );
						AllRatings &= (~HAVE_RECIPIENTS );

					/* Was a range or a list of values given?? */
					} else if ( Rating->ranges ) {
						ranges = Rating->ranges;
						while ((pRange = (Range_t *) HTList_nextObject(ranges))) {
							MinValue = MaxValue = -1;
							if ( IsValidValue( &pRange->min, RecipientsRating.Min, RecipientsRating.Max, &MinValue ) ) {
								RecpRange = RecpRange | ( 1 << MinValue );
								AllRatings &= (~HAVE_RECIPIENTS );
							}
							if ( IsValidValue( &pRange->max, RecipientsRating.Min, RecipientsRating.Max, &MaxValue ) ) {
								/* if there was also a min value then set all the bits between the min and the max */
								if ( 0 <= MinValue && MinValue <= MaxValue ) {
									for( i=MinValue; i<=MaxValue; i++ ) RecpRange = RecpRange | ( 1 << i );
								} else {
									RecpRange = RecpRange | ( 1 << MaxValue );
								}
								AllRatings &= (~HAVE_RECIPIENTS );
							}
						}
					}
#endif
				/* is it the identifiable rating   */
				} else if ( (AllRatings & HAVE_ID) &&
							!XP_STRCASECMP(SVal_value(&Rating->identifier), IDRating.szName)) {
					/* yes - is the range valid? */
					if ( IsValidValue( &Rating->value, IDRating.Min, IDRating.Max, &IDValue ) ) {
						AllRatings &= (~HAVE_ID );
					}
				}
			}		/* end while ( AllRatings  */
			/* Did I get the required ratings? */
			if ( !AllRatings ) {
				/* yes  */
				/*HTTrace( "  Valid trust label: purpose = 0x%0x, ID = %d, recipients = %d\n", PurposeRange, IDValue, RecpRange ); */
				/*HTTrace( "  for: \"%s\" \n", SVal_value(&LabelOptions->fur));		*/
				/*HTTrace( "  expires: %s\n", DVal_value( &LabelOptions->until ) ); */
				TheLabel = TL_Construct();			/* allocate a net trust label */
				if ( TheLabel ) {
					/* fill up the structure */
					TheLabel->purpose = PurposeRange;
					TheLabel->ID = IDValue;
					TheLabel->recipients = RecpRange;
					TheLabel->ExpDate = ExpDate;
					TL_SetTrustAuthority( TheLabel, CSLabel_getServiceName( pCSLabel ) );
					TL_SetByField( TheLabel, SVal_value(&LabelOptions->by) );
					bForgedLabel = FALSE;

					/* if this is a generic label then set the isGeneric flag */
					if ( BVal_value( &LabelOptions->generic )  ) TheLabel->isGeneric = TRUE;

					/* if there is a cookieinfo extension and it had a name(s) then save the name */
					if ( LabelOptions->extensions ) {
						while( !bForgedLabel && (AExt = (Extension_t *)XP_ListNextObject( LabelOptions->extensions )) ) {
							/* is this an extension for cookie info? */
							if ( IsValidTrustService( SVal_value(&AExt->url) ) ) {
								/* is there data attached to the extension - that is stuff besides the mandatory | optional and the URL */
								if ( AExt->extensionData ) {
									/* yes copy the list to the trust label */
									if ( !TheLabel->nameList ) TheLabel->nameList = XP_ListNew();
									if ( TheLabel->nameList ) {
										while( (AData = (ExtensionData_t *)HTList_nextObject( AExt->extensionData ) ) ) {
											AName = XP_STRDUP( AData->text );		/* copy it */
											XP_ListAddObjectToEnd( TheLabel->nameList, AName );		 /* add it to the trust label list */
										}
									}
								}
							/* Is this a digital signature extension?? */
							} else if ( IsLabelSigned( AExt ) ) {
								/* Yes - the label is signed, I must now check if the signature is valid.
								   If the signature is NOT valid then throw the label out. */
								if ( IsSignatureValid( AExt, LabelOptions ) ) {
									/* the label is signed with a good signature */
									TheLabel->isSigned = TRUE;
								} else {
									/* the label has a signature but the MIC on the label doesnt match 
									   the signature, so someone has fooled with the label */
									bForgedLabel = TRUE;
								}
							}			/* end of if ( IsValidTrustService */
						}
					}

					/* Was the label forged?? */
					if ( !bForgedLabel ) {
						/* seperate the path and domain out of the "for" label attribute */
						TL_ProcessForAttrib( TheLabel, SVal_value(&LabelOptions->fur) );

						/* add the trust label to the list of trust labels in this header.
						 * The list of trust labels is maintained in the URL_Struct that 
						 * was passed in.  */
						TheURL_s = (URL_Struct *)URL_s;
						if ( TheURL_s->TrustList == NULL ) {
							/* create the list */
							TheURL_s->TrustList = XP_ListNew();
						}
						if ( TheURL_s->TrustList ) {
							/* NOTE: this is the only place where trust label are added to the TrustList */
							XP_ListAddObjectToEnd( TheURL_s->TrustList, TheLabel );
							HTTrace( "  Valid trust label\n" );
						} else {
							HTTrace( "ERROR in ProcessSingleLabel: unable to allocate the trust list\n" );
							ret = CSError_APP;
						}

					} else {
						/* the label was forged - discard it */
						HTTrace( "Forged label\n" );
						TL_Destruct( TheLabel );
					}

				} else {
					/* memory allocation problems */
					HTTrace( "ERROR in ProcessSingleLabel: failed to allocate trust object\n" );
					ret = CSError_APP;
				}
			} else {
				/* no not a valid trust label, not a big deal, keep goin */
				HTTrace( "   Label not a trust label - not all ratings present.\n" );
			}
		} else {
			/* not a valid trust label - missing expiration date or for option */
			HTTrace( "   Label not a trust label - missing a required option\n" );
		}				/* if ( !IsExpired( LabelOptions  */
	}					/* if (!pCSLabel || */
	return ret;
}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: determine if the value for a rating falls within the specified range
 *  
 * ---------------------------------------------------------------------------------------------------- */
PRBool IsValidValue( FVal_t *Value, int MinValue, int MaxValue, int *TheValue )
{
	if ( FVal_initialized(Value) ) {
		/* NOTE: the conversion from a float to an int, since for trust labels we are  */
		/*  only dealing with ints. */
		*TheValue = (int)FVal_value(Value); 
		if ( MinValue <= (*TheValue) && (*TheValue) <= MaxValue ) {
			return TRUE;
		}
	}
	return FALSE;
}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: determine if the required label options are present.
 *  
 *  
 *  returns:  TRUE if all the required options are present
 *     ExpDate  - returns the label expiration date
 *  
 * ---------------------------------------------------------------------------------------------------- */
PRBool CheckOptions( LabelOptions_t  *LabelOptions, PRTime *ExpirationDate )
{
	PRBool Status = FALSE;
	/* According to the spec these options must be present:
	 *   "by", "gen", "for", "on" AND "exp" OR "until".  AND the expiration date must
	 *   not have passed.
	 */

   	if ( !IsExpired( &LabelOptions->until, ExpirationDate ) &&			/* expiration date */
		 SVal_initialized( &LabelOptions->fur )       &&			/* for option */
		 SVal_initialized( &LabelOptions->by )        &&			/* by option */
		 DVal_initialized( &LabelOptions->on )        &&			/* on option */
		 BVal_initialized( &LabelOptions->generic )         ) {		/* generic option */
		Status = TRUE;
	}
	return Status;
}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: determine if the expiration date has passed.  
 *  
 *  Note:
 * 		The PICS spec calls for only one form for the date, ISO 8601:1988 which is
 * 		YYYY.MM.DDTHH:MMSxxxx
 * 		where S = - or +
 * 			  xxxx = four digit time zone offset
 *  
 *  returns 
 * 	TRUE if the expiration date has PASSED or is not present
 *   FALSE if the expiration date is AFTER now.
 * ---------------------------------------------------------------------------------------------------- */
PRBool IsExpired( DVal_t *Value, PRTime *ExpirationDate )
{
	PRTime			CurrentTime;
	/* Has the expiration date been given?? */
    if (DVal_initialized( Value ) ) {
		/* yes - is it before now? */
		if ( ISODateToLocalTime( Value, ExpirationDate ) ) {
			/* get the exploded local time parameters  */
			CurrentTime = PR_Now();  

			/*HTTrace( "Cur Time = %s", ctime( &CurrentTime ) ); */
			/*HTTrace( "Exp Time = %s", ctime( &ExpTime) ); */
			if ( LL_CMP(CurrentTime,>,*ExpirationDate) ) {
				return TRUE;		/* Expiration time has passed */
			} else {
				return FALSE;		/* Expiration time has NOT passed */
			}
		}
	}
	return TRUE;
}


/*----------------------------------------------------------------------------------------------------
 *  Purpose: if the given service name is a valid trust service.
 * 
 *  Note: the valid trust services are stored in the user's preferences.
 *  They are of the form  "browser.trust.service.XXX.service_enabled" 
 *  where XXX is an escaped URL formed by coverting illegal preference characters to underscore
 *        and lowercasing the string  .
 *  So the preference for the default trust service 
 *     "http:  www.w3.org/PICS/systems/P3P-http-trust-state-01.rat"    is
 *  pref("browser.trust.service.http___www_w3_org_pics_systems_p3p_http_trust_state_01_rat.service_enabled", true);
 *  
 *  returns TRUE if the given service is a valid trust service
 * ----------------------------------------------------------------------------------------------------*/
PRBool IsValidTrustService( char *ServiceName )
{
	char	*PrefName;
	char	*escaped_service;

	if ( ServiceName && *ServiceName ) {
		/* Build the preference string - first escape the service name */
		escaped_service = XP_STRDUP(ServiceName);
		escaped_service = illegal_to_underscore(escaped_service);
		escaped_service = lowercase_string(escaped_service);
		if(!escaped_service) return FALSE;

		/* create the preference string */
		PrefName = XP_Cat( "browser.trust.service.", escaped_service, ".service_enabled", (char *)NULL );
		XP_FREE(escaped_service);
		if ( PrefName ) {
#ifndef	  FOR_PHASE2
#define FIRST_STR "browser.trust.service.http___www_w3_org_pics_systems_p3p_http_trust_state_01_rat.service_enabled" 
#define SECOND_STR "browser.trust.service.http___www_w3_org_pics_extensions_cookieinfo_1_0_html.service_enabled"
			if ( XP_STRCASECMP( PrefName, FIRST_STR ) == 0 ||
				 XP_STRCASECMP( PrefName, SECOND_STR ) == 0   ) {
				return TRUE;
			} else {
				return FALSE;
			}
#else
			/* Now make sure the preference both exists and is enabled */
			if(!PREF_GetBoolPref(PrefName, &bool_pref)) {
				/* the preference exists - this is a known rating service */
				XP_FREE( PrefName );
				return bool_pref;
			} else {
				/* the preference does not exist - this is an unknown rating service */
				XP_FREE( PrefName );
				return FALSE;
			}
#endif
		}
	}
	return FALSE;
}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: takes a parsed ISO date from the PICS label parser and converts
 * 			it to a local PRTime
 *  
 *  
 *  returns 
 * 	TRUE if the date  is valid
 *   FALSE if the date is not a valid ISO string
 * ---------------------------------------------------------------------------------------------------- */
PRBool ISODateToLocalTime( DVal_t *Value, PRTime *LocalDate )
{
    if (DVal_initialized( Value ) ) {
		/* the string was parsed into the different fields */
		PRExplodedTime tm;
		XP_MEMSET( (void *)&tm, 0, sizeof( PRExplodedTime ) );
		tm.tm_year   = Value->year;
		tm.tm_month  = Value->month - 1;
		tm.tm_mday	 = Value->day;
		tm.tm_hour	 = Value->hour;
		tm.tm_min	 = HTMIN(Value->minute, 59 );
		/* the adjustment for GMT offset to the local time zone in seconds */
	    tm.tm_params.tp_gmt_offset = Value->timeZoneHours *3600 + Value->timeZoneMinutes * 60;
		*LocalDate = PR_ImplodeTime(&tm);
		return TRUE;
	} else {
		return FALSE;
	}

}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: determine if path2 is a prefix of path1
 *  
 *  returns - TRUE if it is
 *---------------------------------------------------------------------------------------------------- */
PRBool IsPrefix( char *path1, char *path2 )
{
	int i;
	if ( path1 && path2 ) {
		int len1 = XP_STRLEN( path1 );
		int len2 = XP_STRLEN( path2 );
		/* Is path2's length <= path1's length?? */
		if ( len2 <= len1 ) {
			/* path1 characters must match path2 up to the length of path1 */
			for ( i=0; i<len2; i++ ) {
				if ( path1[i] != path2[i] ) {
					/* not a prefix */
					return FALSE;
				}
			}
			/* path2 is a prefix of path1 */
			return TRUE;
		}
	}
	return FALSE;

}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: given the path from a cookie and the list of trust labels in this header
 * 			try to match the cookie to a trust label.                             
 *  
 *  inputs:
 * 	TrustList	- the list of trust labels to search
 *  CookieData  - the data struct used for java script partially filled out 
 * 				  with the cookie name, path and domain
 *  CookieDomain  - the domain attribute from the cookie
 *  returns 
 * 	TRUE if a matching trust label is found
 *  FALSE if none found
 *  TheLabel - ptr to the matching trust label
 *----------------------------------------------------------------------------------------------------*/
PUBLIC PRBool MatchCookieToLabel( char *TargetURL, JSCFCookieData *CookieData, TrustLabel **TheLabel )
{
	PRBool Status = FALSE;
	if ( TargetURL && CookieData && TheLabel ) {
		Status = MatchCookieToLabel2( TargetURL, CookieData->name_from_header,
							  CookieData->path_from_header, CookieData->host_from_header, 
							  TheLabel );
	} 
	return Status;
}

/****************************************************************
 * Purpose: The actual matching code.
 * 			This implements part of section 3.3.1 of the trust label spec dealing 
 * 			with figuring out if a cookie and a trust label are "compatiable".
 *
 *  CookieName  - the name of the cookie 
 *  CookiePath  - the path for the cookie
 *  CookieHost  - the host for the cookie 
 *
 *  returns 
 * 	TRUE if a matching trust label is found
 *  FALSE if none found
 *  TheLabel - ptr to the matching trust label
 *
 *
 * History:
 *  Paul Chek - initial creation
 ****************************************************************/
PUBLIC PRBool MatchCookieToLabel2(char *TargetURL, char *CookieName,
								 char *CookiePath, char *CookieHost, 
								 TrustLabel **TheLabel )
{
	PRBool	Status = FALSE;
	TrustLabel  *ALabel;
	char	*AName;
	PRBool	bNameMatch;
	XP_List *TempList;
	TrustLabel	*LastMatch = NULL;
	PRBool		LastMatchNamed;
	XP_List *TempTrustList;

	/* make sure I have the data I need										 */
	if ( TargetURL && XP_STRLEN( TargetURL ) && 
		 CookieName && CookiePath && CookieHost &&
		 TheLabel ) {
		/* look thru the list of trust labels for one to match this cookie */
		/* First see if there is a named trust label that matches the cookie */
		TempTrustList = NET_GetTrustList( TargetURL );
		if ( TempTrustList ) {
		while( (ALabel = (TrustLabel *)XP_ListNextObject( TempTrustList )) ) {
				/* is this label for a specific cookie(s)?? */
				bNameMatch = FALSE;
				if ( ALabel->nameList != NULL ) {
					/* yes - do the names match -  CASE INSENSITIVE COMPARE ?? */
					TempList = ALabel->nameList;		/* always walk a list with a COPY of the list pointer */
					while( (AName = (char *)XP_ListNextObject( TempList ) ) ) {
						if ( XP_STRCASECMP (AName, CookieName ) == 0 ) {
							/* this label has a cookie name and it matches the current cookie */
							bNameMatch = TRUE;
							break;
						}
					}
				}

				/* do the domains match?? */
				if ( NET_SameDomain( ALabel->domainName, CookieHost ) == 1 ) {
					/* the domains match - is this label a "Generic" label?? */
					if ( ALabel->isGeneric ) {
						/* is the path from the label a prefix to the path in the cookie */
						if ( IsPrefix( CookiePath, ALabel->path ) ) {
							/* this label matches this cookie -- is it the best match */
							if ( !LastMatch ) {
								/* no previous match - take this one */
								LastMatch = ALabel;
								LastMatchNamed = bNameMatch;
							} else {
								/* there was a previus match - is this one more specific?? 
								   Pick named over unnamed labels; pick non-generic over generic labels.
								   
								   So since the current label is GENERIC, if it is named AND 
								   the previous label is NOT named use the current label
								   because it is a better match - named is better than not named.

								   An implied test is if neither are named, since this is a GENERIC
								   label then keep the previous label because it is either
								   a better match or an equivalent match. 
								*/
								if (  bNameMatch && (!LastMatchNamed) ) {
									LastMatch = ALabel;
									LastMatchNamed = bNameMatch;
								}
							}
						}
					} else {
						/* this is not a generic label - do the paths match exactly - CASE INSENSITIVE COMPARE ?? */
						if ( XP_STRCASECMP( ALabel->path, CookiePath ) == 0 ) {
							/* this label matches this cookie -- is it the best match */
							if ( !LastMatch ) {
								/* no previous match - take this one */
								LastMatch = ALabel;
								LastMatchNamed = bNameMatch;
							} else {
								/* there was a previus match - is this one more specific?? 
								   Pick named over unnamed labels; pick non-generic over generic labels.
								   
								   So since the current label is NON-GENERIC, if it is named 
								   use it because it is either a better match or an equivalent match.
								   If it is not named AND the previous label is not named then use
								   the current label because it is either a better match or an equivalent match. 
								*/
								if (  bNameMatch || (!LastMatchNamed) ) {
									/* both are named, the current label is non-generic, use it */
									LastMatch = ALabel;
									LastMatchNamed = bNameMatch;
								}
							}
						}
					}		/* end of if ( ALabel->isGeneric  */
				}		/* end of if ( NET_SameDomain */
		}			/* end of while( (ALabel */
		}			/* end of if ( tmpEntry && tmpEntry */
	}

	/* Was there a match?? */
	if ( LastMatch ) {
		/* yes */
		*TheLabel = LastMatch;
		return TRUE;
	} else {
		return FALSE;
	}

}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: returns true if the user preference for supporting trust labels is 
 *			 set to true.
 *  
 *  
 *---------------------------------------------------------------------------------------------------- */
PUBLIC PRBool IsTrustLabelsEnabled()
{
#ifndef FOR_PHASE2
	return TRUE;
#else
static int bEnabled = -1;
	if ( bEnabled == -1 ) {
		/* on the first call get it from the preferences */	
		PREF_GetBoolPref("browser.PICS.trust_labels_enabled", &bEnabled);
	}
	return (PRBool)bEnabled;
#endif
}


/*----------------------------------------------------------------------------------------------------
 *  Purpose: returns true if the given extension is the Digital Signature extension
 *  
 *---------------------------------------------------------------------------------------------------- */
PRBool IsLabelSigned( Extension_t *AExt )
{
	return FALSE;			/* not handling signed label right now */
}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: returns true if the given Digital Signature extension is a valid signature for 
 *			 the given label.  If this returns false then we have a forged label.
 *  
 *---------------------------------------------------------------------------------------------------- */
PRBool IsSignatureValid( Extension_t *AExt, LabelOptions_t  *LabelOptions )
{
	return TRUE;			/* not handling signed label right now */

}



/*\\//\\//\\//\\//  "MEMBERS" OF THE TRUST LABEL "CLASS" \\//\\//\\//\\//\\//\\//\\//\\//\\// */


/*----------------------------------------------------------------------------------------------------
 *  Purpose: construct a Trust Label and init its members
 *
 *  returns - the new initialized TrustLabel
 * ----------------------------------------------------------------------------------------------------*/
TrustLabel *TL_Construct()
{
	TrustLabel *ALabel = NULL;
	ALabel = XP_ALLOC( sizeof(TrustLabel) );
	if ( ALabel ) {
		ALabel->purpose	= 0;
		ALabel->ID = 0;
		ALabel->recipients = 0;	
		ALabel->szTrustAuthority = NULL;	
		ALabel->isGeneric = FALSE;	
		ALabel->isSigned = FALSE;
		#if defined(XP_MAC) || defined(PRINT64_IS_STRUCT)
		ALabel->ExpDate.hi = ALabel->ExpDate.lo = 0;
		#else	
		ALabel->ExpDate = 0;	
		#endif
		ALabel->signatory = NULL;	
		ALabel->domainName = NULL;	
		ALabel->path = NULL;  
		ALabel->nameList = NULL;
		ALabel->szBy = NULL;  
	}
	return ALabel;

}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: destroy a TrustLabel freeing allocated memory
 *  
 * ----------------------------------------------------------------------------------------------------*/
PUBLIC void TL_Destruct( TrustLabel *ALabel )
{
	char *AName;
	if ( ALabel ) {
		if ( ALabel->signatory ) XP_FREE( ALabel->signatory );
		if ( ALabel->domainName ) XP_FREE( ALabel->domainName );
		if ( ALabel->path ) XP_FREE( ALabel->path );
		if ( ALabel->szTrustAuthority ) XP_FREE( ALabel->szTrustAuthority );
		if ( ALabel->szBy ) XP_FREE( ALabel->szBy );
		if ( ALabel->nameList ) {
			while( (AName = (char *)XP_ListRemoveEndObject( ALabel->nameList )) ){
				XP_FREE( AName );
			}
			/* now delete the list */
			XP_ListDestroy( ALabel->nameList );
		}
		XP_FREE( ALabel );
	}
}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: decompose the "for" label attribute into a domain and path
 * 			and save in the trust label
 *  
 * ----------------------------------------------------------------------------------------------------*/
void TL_ProcessForAttrib( TrustLabel *ALabel, char *szFor)
{
	char *p;
	if ( ALabel && szFor ) {
		/* for the domain - skip to the "//" to jump over the protocol, */
		/* this is the beginning of the domain, then					*/
		/* look for the first ":" or "/" to mark the end of the domain	*/
		p = XP_STRCASESTR(szFor, "//");
		if ( p == NULL ) {
			NET_SACopy( &ALabel->domainName, szFor);
		} else {
			NET_SACopy( &ALabel->domainName, p+2);
		}
		for(p=ALabel->domainName; *p != '\0' && *p != ':' && *p != '/'; p++);  /* skip to end OR : or / */
		
		/* for the path it starts where the domain stopped */
		NET_SACopy( &ALabel->path, p );
		*p = '\0';			/* terminate the domain name */
	}

}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: set the signatory for a signed label
 *  
 * ----------------------------------------------------------------------------------------------------*/
void TL_SetSignatory( TrustLabel *ALabel, char *Signatory )
{
	if ( ALabel && Signatory ) {
		NET_SACopy( &ALabel->signatory, Signatory );
	}
}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: set the trust authority string
 *  
 * ----------------------------------------------------------------------------------------------------*/ 
void TL_SetTrustAuthority( TrustLabel *ALabel, char *TrustAuthority )
{
	if ( ALabel && TrustAuthority ) {
		NET_SACopy( &ALabel->szTrustAuthority, TrustAuthority );
	}
}

/*----------------------------------------------------------------------------------------------------
 *  Purpose: set the by field in the label from the "by" label option
 *  
 * ----------------------------------------------------------------------------------------------------*/ 
void TL_SetByField( TrustLabel *ALabel, char *szBy )
{
	if ( ALabel && szBy ) {
		NET_SACopy( &ALabel->szBy, szBy );
	}
}


#endif			/* end of #ifdef TRUST_LABELS */





