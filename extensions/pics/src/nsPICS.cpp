/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#define NS_IMPL_IDS
#include "pratom.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsRepository.h"
#include "nsIPICS.h"
#include "nscore.h"
#include "cslutils.h"
#include "csll.h"
#include "csllst.h"
#include "htstring.h"
#include "prprf.h"
#include "plstr.h"
#include "prmem.h"
#include "net.h"
#include <stdio.h>

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID); 
static NS_DEFINE_IID(kIPICSIID, NS_IPICS_IID);

static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);



#define PICS_DOMAIN  			"browser.PICS."
#define PICS_ENABLED_PREF 		PICS_DOMAIN"ratings_enabled"
#define PICS_MUST_BE_RATED_PREF PICS_DOMAIN"pages_must_be_rated"
#define PICS_DISABLED_FOR_SESSION PICS_DOMAIN"disable_for_this_session"
#define PICS_REENABLE_FOR_SESSION PICS_DOMAIN"reenable_for_this_session"
#define PICS_URL_PREFIX "about:pics"

#define PICS_TO_UPPER(x) ((((unsigned int) (x)) > 0x7f) ? x : toupper(x))
#define PICS_TO_LOWER(x) ((((unsigned int) (x)) > 0x7f) ? x : tolower(x))

#define PICS_IS_ALPHA(x) ((((unsigned int) (x)) > 0x7f) ? 0 : isalpha(x))
#define PICS_IS_DIGIT(x) ((((unsigned int) (x)) > 0x7f) ? 0 : isdigit(x))
#define PICS_IS_SPACE(x) ((((unsigned int) (x)) > 0x7f) ? 0 : isspace(x))



#define JAVA_SECURITY_PASSWORD "signed.applets.capabilitiesDB.password"

typedef struct {
   char    *service;
   PRBool  generic;
   char    *fur;     /* means 'for' */
   HTList  *ratings;
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

typedef struct {
	PRBool ratings_passed;
	PICS_RatingsStruct *rs;
} TreeRatingStruct;


typedef struct {
	PICS_RatingsStruct * rs;
	PRBool rs_invalid;
} ClosureData;

////////////////////////////////////////////////////////////////////////////////

class nsPICS : public nsIPICS {
public:

    NS_IMETHOD
    ProcessPICSLabel(char *label);
	NS_IMETHOD Init();

    nsPICS();
    ~nsPICS(void);

    NS_DECL_ISUPPORTS

protected:
    void   GetUserPreferences();
    
private:

    nsIPref* mPrefs;

    PRBool mPICSRatingsEnabled ;
    PRBool mPICSPagesMustBeRatedPref;
    PRBool mPICSDisabledForThisSession;
    PRBool mPICSJavaCapabilitiesEnabled;

    HTList* mPICSTreeRatings;

    friend StateRet_t TargetCallback(CSLabel_t *pCSLabel,
		                        CSParse_t * pCSParse,
 		                        CSLLTC_t target, PRBool closed,
 		                        void * pClosure);

    friend StateRet_t ParseErrorHandlerCallback(CSLabel_t * pCSLabel, 
                                         CSParse_t * pCSParse,
                                         const char * token, char demark,
                                         StateRet_t errorCode);

    friend int PrefChangedCallback(const char*, void*);

    void   PreferenceChanged(const char* aPrefName);
    
    PICS_RatingsStruct * ParsePICSLabel(char * label);

    PRBool IsPICSEnabledByUser(void);

    PRBool AreRatingsRequired(void);

    char * GetURLFromRatingsStruct(PICS_RatingsStruct *rs, char *cur_page_url);

    void FreeRatingsStruct(PICS_RatingsStruct *rs);

    PICS_RatingsStruct * CopyRatingsStruct(PICS_RatingsStruct *rs);

    PRBool CanUserEnableAdditionalJavaCapabilities(void);

    /* returns TRUE if page should be censored
     * FALSE if page is allowed to be shown
     */
    PICS_PassFailReturnVal CompareToUserSettings(PICS_RatingsStruct *rs, char *cur_page_url);

    char * IllegalToUnderscore(char *string);

    char * LowercaseString(char *string);

    void AddPICSRatingsStructToTreeRatings(PICS_RatingsStruct *rs, PRBool ratings_passed);

    TreeRatingStruct * FindPICSGenericRatings(char *url_address);

    void CheckForGenericRatings(char *url_address, PICS_RatingsStruct **rs,
                                    PICS_PassFailReturnVal *status);

};

////////////////////////////////////////////////////////////////////////////////

class nsPICSFactory : public nsIFactory {
public:

    NS_DECL_ISUPPORTS

    // nsIFactory methods:

    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              REFNSIID aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

    // nsPICS methods:

    nsPICSFactory(void);
    virtual ~nsPICSFactory(void);

};

static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

StateRet_t 
TargetCallback(CSLabel_t *pCSLabel,
		CSParse_t * pCSParse,
 		CSLLTC_t target, PRBool closed,
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

		if(lo) {
			if(BVal_value(&lo->generic)) {
				cd->rs->generic = TRUE;
			}

			if(lo->fur.value && !cd->rs->fur) {
				StrAllocCopy(cd->rs->fur, lo->fur.value);
			}
		}
    } else if(target ==  CSLLTC_RATING) {
    	PICS_RatingsStruct *rating_struct = cd->rs;
        LabelOptions_t * lo = CSLabel_getLabelOptions(pCSLabel);

        ratingstr = CSLabel_getRatingStr(pCSLabel);
		ratingname =  CSLabel_getRatingName(pCSLabel);

		if(ratingname) {
			LabelRating_t * label_rating;
			ServiceInfo_t * service_info;

			service_info = CSLabel_getServiceInfo(pCSLabel);

			if(service_info && !rating_struct->service) {
			   rating_struct->service = PL_strdup(service_info->rating_service.value); 
			}

			label_rating = CSLabel_getLabelRating(pCSLabel);

			if(label_rating) {
                double value;
				PICS_RatingValue *rating_value = PR_NEWZAP(PICS_RatingValue);

                value = label_rating->value.value;
					
				if(rating_value) {
					rating_value->value = label_rating->value.value;
					rating_value->name = PL_strdup(label_rating->identifier.value);

					if(rating_value->name) {
						/* insert it into the list */
						HTList_addObject(rating_struct->ratings, rating_value); 
                    } else {
						/* error, cleanup */
						PR_Free(rating_value);
					}
				}
            }
		}

	}
	return StateRet_OK;
}

StateRet_t 
ParseErrorHandlerCallback(CSLabel_t * pCSLabel, CSParse_t * pCSParse,
                             const char * token, char demark,
                             StateRet_t errorCode)
{
	return errorCode;
}

int 
PrefChangedCallback(const char* aPrefName, void* instance_data)
{
    nsPICS*  pics = (nsPICS*)instance_data;

    NS_ASSERTION(nsnull != pics, "bad instance data");
    if (nsnull != pics) {
        pics->PreferenceChanged(aPrefName);
    }
    return 0;  // PREF_OK
}
////////////////////////////////////////////////////////////////////////////////
// nsPICS Implementation


NS_IMPL_ISUPPORTS(nsPICS, kIPICSIID);

nsPICS::nsPICS()
{
    PR_AtomicIncrement(&g_InstanceCount);
    NS_INIT_REFCNT();
    
    printf("  creating my service\n");
    mPICSRatingsEnabled = FALSE;
    mPICSPagesMustBeRatedPref = FALSE;
    mPICSDisabledForThisSession = FALSE;
    mPICSJavaCapabilitiesEnabled = TRUE;

    mPICSTreeRatings = nsnull;
   
}

nsPICS::~nsPICS(void)
{
    PR_AtomicDecrement(&g_InstanceCount);
    printf("  destroying my service\n");
}


nsresult
nsPICS::ProcessPICSLabel(char *label)
{
	PICS_PassFailReturnVal status = PICS_NO_RATINGS;
    printf("    invoking my service\n");
	PICS_RatingsStruct *rs;
/*	PICS_Init(); */
	rs = ParsePICSLabel(label);
    status = CompareToUserSettings(rs, "http://www.w3.org/");
	printf("PICS_PassFailReturnVal  %d", status);
	FreeRatingsStruct(rs);
	return status;
}

nsresult
nsPICS::Init()
{
	nsresult rv = NS_OK;
    nsresult res;
    nsIPref* aPrefs;
   
    static PRBool first_time=TRUE;

    res = nsServiceManager::GetService(kPrefCID, 
                                    nsIPref::GetIID(), 
                                    (nsISupports **)&aPrefs);

    if (NS_OK != res) {
          return res;
    }

    mPrefs = aPrefs;

    if(!first_time) {
        return rv;
    } else {
	  /*  char *password=NULL; */

        first_time = FALSE;

       
        mPrefs->Startup("C:\\Program Files\\Netscape\\Users\\neeti\\prefs.js");
        /* get the prefs */
	    PrefChangedCallback(PICS_DOMAIN, (void*)this);

        mPrefs->RegisterCallback(PICS_DOMAIN, PrefChangedCallback, (void*)this);

	   /*  check for security pref that password disables the enableing of
	     java permissions */
	    /*
        if(PREF_CopyCharPref(JAVA_SECURITY_PASSWORD, &password))
		    password = NULL;

	    if(password && *password)
	    {
		    // get prompt string from registry
		     
		    char *prompt_string = XP_GetString(XP_ALERT_PROMPT_JAVA_CAPIBILITIES_PASSWORD);
		    char *user_password;
		    char *hashed_password;

prompt_again:

		    // prompt the user for the password
		     
		    user_password = FE_PromptPassword(context, prompt_string);

		    // ### one-way hash password 
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
                mPICSJavaCapabilitiesEnabled = FALSE;
            }
            else if(!PL_strcmp(hashed_password, password))
		    {
			    mPICSJavaCapabilitiesEnabled = TRUE;
		    }
		    else
		    {
                PR_Free(user_password);
                PR_Free(hashed_password);
       		    prompt_string = XP_GetString(XP_ALERT_PROMPT_JAVA_CAPIBILITIES_PASSWORD_FAILED_ONCE);
                goto prompt_again;
		    }

		    PR_FREEIF(user_password);
		    PR_FREEIF(hashed_password);
		    
	    }
	    
	    PR_FREEIF(password);
		*/
    }

	return rv;
}

void
nsPICS::GetUserPreferences()
{
    PRBool bool_rv;

    if(!this->mPrefs->GetBoolPref(PICS_ENABLED_PREF, &bool_rv))
        mPICSRatingsEnabled = bool_rv;
    if(!mPrefs->GetBoolPref(PICS_MUST_BE_RATED_PREF, &bool_rv))
        mPICSPagesMustBeRatedPref = bool_rv;
    if(!mPrefs->GetBoolPref(PICS_DISABLED_FOR_SESSION, &bool_rv)) {
		if(bool_rv) {
        	mPICSDisabledForThisSession = TRUE;
    		mPrefs->SetBoolPref(PICS_DISABLED_FOR_SESSION, FALSE);
		}
	}
    if(!mPrefs->GetBoolPref(PICS_REENABLE_FOR_SESSION, &bool_rv)) {
		if(bool_rv) {
        	mPICSDisabledForThisSession = FALSE;
    		mPrefs->SetBoolPref(PICS_REENABLE_FOR_SESSION, FALSE);
		}
	}

}


/* return NULL or ratings struct */
PICS_RatingsStruct * 
nsPICS::ParsePICSLabel(char * label)
{
	CSParse_t *CSParse_handle;
    ClosureData *cd;
	PICS_RatingsStruct *rs;
	PICS_RatingsStruct *new_rs;
    CSDoMore_t status;
	HTList *list_ptr;
    PICS_RatingValue *rating_value;

	if(!label)
		return NULL;

	cd = PR_NEWZAP(ClosureData);

	if(!cd)
		return NULL;

	rs = PR_NEWZAP(PICS_RatingsStruct);

	if(!rs) {
		PR_Free(cd);
		return NULL;
	}

  	rs->ratings = HTList_new();

	cd->rs = rs;

	/* parse pics label using w3c api */

	CSParse_handle = CSParse_newLabel(&TargetCallback, &ParseErrorHandlerCallback);

	if(!CSParse_handle)
		return NULL;

	do {

		status = CSParse_parseChunk(CSParse_handle, label, PL_strlen(label), cd);

	} while(status == CSDoMore_more);

	if(cd->rs_invalid) {
		FreeRatingsStruct(rs);
		rs = NULL;
    } else {
		new_rs = CopyRatingsStruct(rs);
		FreeRatingsStruct(rs);
		rs = NULL;
	}


	list_ptr = new_rs->ratings;

	while((rating_value = (PICS_RatingValue *)HTList_nextObject(list_ptr)) != NULL) {

		if (rating_value->name) 
			printf(" rating_value->name: %s\n", rating_value->name);
		printf(" rating_value->value: %f\n\n", rating_value->value);

	}


	PR_Free(cd);

    CSParse_deleteLabel(CSParse_handle);

	return(new_rs);
}

void
nsPICS::FreeRatingsStruct(PICS_RatingsStruct *rs)
{
	if(rs) {
        PICS_RatingValue *rv;
        
        while((rv = (PICS_RatingValue *)HTList_removeFirstObject(rs->ratings)) != NULL) {
            PR_Free(rv->name);
            PR_Free(rv);
        }

		PR_FREEIF(rs->fur);
		PR_FREEIF(rs->service);
		PR_Free(rs);
	}
}

PICS_RatingsStruct *
nsPICS::CopyRatingsStruct(PICS_RatingsStruct *rs)
{
	PICS_RatingsStruct *new_rs = NULL;

	if(rs) {
        PICS_RatingValue *rv;
        PICS_RatingValue *new_rv;
        HTList *list_ptr;

		new_rs = PR_NEWZAP(PICS_RatingsStruct);

		if(!new_rs)
			return NULL;

      	new_rs->ratings = HTList_new();
        
        list_ptr = rs->ratings;
        while((rv = (PICS_RatingValue *)HTList_nextObject(list_ptr)) != NULL) {
			new_rv = (PICS_RatingValue*)PR_Malloc(sizeof(PICS_RatingValue));
			
			if(new_rv) {
				new_rv->name = PL_strdup(rv->name);
				new_rv->value = rv->value;

				if(new_rv->name) {
					HTList_addObject(new_rs->ratings, new_rv);
                } else {
					PR_Free(new_rv);
				}
			}
        }

		new_rs->generic= rs->generic;
		new_rs->fur = PL_strdup(rs->fur);
		new_rs->service = PL_strdup(rs->service);
	}

	return(new_rs);
}

PRBool
nsPICS::CanUserEnableAdditionalJavaCapabilities(void)
{
	return(mPICSJavaCapabilitiesEnabled);
}


PRBool
nsPICS::IsPICSEnabledByUser(void)
{
	
	if(mPICSDisabledForThisSession)
		return FALSE;

    return(mPICSRatingsEnabled);
}

PRBool
nsPICS::AreRatingsRequired(void)
{
	return mPICSPagesMustBeRatedPref;
}

/* returns a URL string from a RatingsStruct
 * that includes the service URL and rating info
 */
char *
nsPICS::GetURLFromRatingsStruct(PICS_RatingsStruct *rs, char *cur_page_url)
{
    char *rv; 
    char *escaped_cur_page=NULL;

    if(cur_page_url) {
        /* HACK --Neeti fix this
        escaped_cur_page = NET_Escape(cur_page_url, URL_PATH);   */ 
        if(!escaped_cur_page)
            return NULL;
    }

    rv = PR_smprintf("%s?Destination=%s", 
                     PICS_URL_PREFIX, 
                     escaped_cur_page ? escaped_cur_page : "none");

    PR_Free(escaped_cur_page);

    if(!rs || !rs->service) {
        StrAllocCat(rv, "&NO_RATING");
        return(rv);
    } else {
		HTList *list_ptr = rs->ratings;
		PICS_RatingValue *rating_value;
        char *escaped_service = NULL;
        /* HACK --Neeti fix this
    	escaped_service = NET_Escape(rs->service, URL_PATH);    */

    	if(!escaped_service)
        	return NULL;

        StrAllocCat(rv, "&Service=");
        StrAllocCat(rv, escaped_service);

        PR_Free(escaped_service);

        while((rating_value = (PICS_RatingValue *)HTList_nextObject(list_ptr)) != NULL) {

            char *add;
            char *escaped_name = NULL;
         
            /* HACK --Neeti fix this
            escaped_name = NET_Escape(
                               IllegalToUnderscore(rating_value->name),
                               URL_PATH); */
            if(!escaped_name) {
                PR_Free(rv); 
                return NULL;
            }
            
            add = PR_smprintf("&%s=%f", escaped_name, rating_value->value);

            PR_Free(escaped_name);

            StrAllocCat(rv, add);

            PR_Free(add);
        }

        return rv;
    }

    PR_ASSERT(0);// should never get here 
    return NULL;
}

/* returns TRUE if page should be censored
 * FALSE if page is allowed to be shown
 */

PICS_PassFailReturnVal
nsPICS::CompareToUserSettings(PICS_RatingsStruct *rs, char *cur_page_url)
{
	int32 int_pref;
	PRBool bool_pref;
	char * pref_prefix;
	char * pref_string=NULL;
	char * escaped_service;
	PICS_PassFailReturnVal rv = PICS_RATINGS_PASSED;
    HTList *list_ptr;
    PICS_RatingValue *rating_value;

	if(!rs || !rs->service) {
		return PICS_NO_RATINGS;
	}

#define PICS_SERVICE_DOMAIN   PICS_DOMAIN"service."
#define PICS_SERVICE_ENABLED  "service_enabled"

	/* cycle through list of ratings and compare to the users prefs */
	list_ptr = rs->ratings;
	pref_prefix = PL_strdup(PICS_SERVICE_DOMAIN);

    /* need to deal with bad characters */
	escaped_service = PL_strdup(rs->service);
    escaped_service = IllegalToUnderscore(escaped_service);
    escaped_service = LowercaseString(escaped_service);

	if(!escaped_service)
		return PICS_RATINGS_FAILED;

	StrAllocCat(pref_prefix, escaped_service);

	PR_Free(escaped_service);

	if(!pref_prefix)
		return PICS_RATINGS_FAILED;

	/* verify that this type of rating system is enabled */
	pref_string = PR_smprintf("%s.%s", pref_prefix, PICS_SERVICE_ENABLED);
	
    if(!pref_string)
        goto cleanup;

	if(!mPrefs->GetBoolPref(pref_string, &bool_pref)) {
		if(!bool_pref) {
			/* this is an unenabled ratings service */
			rv = PICS_NO_RATINGS;
            PR_Free(pref_string);
			goto cleanup;
		}
    } else {
		/* this is an unsupported ratings service */
		rv = PICS_NO_RATINGS;
        PR_Free(pref_string);
		goto cleanup;
	}

    PR_Free(pref_string);

	while((rating_value = (PICS_RatingValue *)HTList_nextObject(list_ptr)) != NULL) {
		/* compose pref lookup string */
		pref_string = PR_smprintf("%s.%s", 
                                pref_prefix, 
                                IllegalToUnderscore(rating_value->name));

		if(!pref_string)
			goto cleanup;

		/* find the value in the prefs if it exists
		 * if it does compare it to the value given and if
		 * less than, censer the page.
		 */
		 
		if(!mPrefs->GetIntPref(pref_string, &int_pref)) {
			if(rating_value->value > int_pref) {
				rv = PICS_RATINGS_FAILED;
                PR_Free(pref_string);
				goto cleanup;
			}
		}

		PR_Free(pref_string);
	}

cleanup:

	PR_Free(pref_prefix);

	/* make sure this rating applies to this page  */
	if(rs->fur) {
		char *new_url;
		PL_strcpy(cur_page_url, rs->fur);  /** HACK - remove this Neeti 
		new_url = NET_MakeAbsoluteURL(cur_page_url, rs->fur);
		if(new_url)
		{
			PR_Free(rs->fur);
			rs->fur = new_url;
		}
		**/
		
		if(PL_strncasecmp(cur_page_url, rs->fur, PL_strlen(rs->fur))) {
            /* HACK -Neeti fix this
			if(MAILBOX_TYPE_URL == NET_URL_Type(cur_page_url))
            { 
              // if it's a mailbox URL ignore the "for" directive,
               // and don't store the url in the tree rating 
               //
                return rv;
            }
            */
			
		   rv = PICS_NO_RATINGS;
        }
	}

	if(rv != PICS_NO_RATINGS) {
		PR_ASSERT(rv == PICS_RATINGS_PASSED || rv == PICS_RATINGS_FAILED);
		/* if there is no URL make it the current one */
		if(!rs->fur)
			rs->fur = PL_strdup(cur_page_url);

		/* rating should apply to a whole tree, add to list */
	 	AddPICSRatingsStructToTreeRatings(rs, rv == PICS_RATINGS_PASSED);		
	}

	return rv;
}

char *
nsPICS::IllegalToUnderscore(char *string)
{
    char* ptr = string;

    if(!string)
       return NULL;

    if(!PICS_IS_ALPHA(*ptr))
           *ptr = '_';

    for(ptr++; *ptr; ptr++)
        if(!PICS_IS_ALPHA(*ptr) && !PICS_IS_DIGIT(*ptr))
           *ptr = '_';

    return string;
}

char *
nsPICS::LowercaseString(char *string)
{
    char *ptr = string;

    if(!string)
        return NULL;

    for(; *ptr; ptr++)
        *ptr = PICS_TO_LOWER(*ptr);

    return string;
}

void
nsPICS::AddPICSRatingsStructToTreeRatings(PICS_RatingsStruct *rs, PRBool ratings_passed)
{
	char *path = NULL;
	TreeRatingStruct *trs;

	if(!mPICSTreeRatings)
	{
		mPICSTreeRatings = HTList_new();
		if(!mPICSTreeRatings)
			return;
	}

	if(!rs->fur || !rs->generic)
		return;   /* doesn't belong here */

	/* make sure it's not in the list already */
	if(FindPICSGenericRatings(rs->fur))
		return;

	/* make sure the fur address smells like a URL and has
	 * a real host name (at least two dots) 
	 * reject "http://" or "http://www" 
	 */
    /* HACK - Neeti fix this
	if(!NET_URL_Type(rs->fur))
		return;
    
	path = NET_ParseURL(rs->fur, GET_PATH_PART);
    */
	/* if it has a path it's ok */
	if(!path || !*path) {
		/* if it doesn't have a path it needs at least two dots */
		char *ptr;
		char *hostname = NULL;

        /* HACK - Neeti fix this
        hostname = NET_ParseURL(rs->fur, GET_HOST_PART);
        */
		if(!hostname)
			return;

		if(!(ptr = PL_strchr(hostname, '.'))
			|| !PL_strchr(ptr+1, '.')) {
			PR_Free(hostname);
			PR_FREEIF(path);
			return;
		} 

		PR_Free(hostname);
	}

	PR_Free(path);

	trs = (TreeRatingStruct*)PR_Malloc(sizeof(TreeRatingStruct)); 

	if(trs) {
		trs->ratings_passed = ratings_passed;
		trs->rs = CopyRatingsStruct(rs);
		if(trs->rs)
    		HTList_addObject(mPICSTreeRatings, trs);
	}

	return;
}


TreeRatingStruct *
nsPICS::FindPICSGenericRatings(char *url_address)
{

	HTList *list_ptr;
	TreeRatingStruct *trs;

	if(!mPICSTreeRatings)
		return NULL;

	list_ptr = mPICSTreeRatings;

	while((trs = (TreeRatingStruct *)HTList_nextObject(list_ptr))) {
		if(trs->rs && trs->rs->fur) {
			if(!PL_strncasecmp(url_address, trs->rs->fur, PL_strlen(trs->rs->fur)))
				return trs;
		}
	}

	return NULL;
}

void
nsPICS::CheckForGenericRatings(char *url_address, PICS_RatingsStruct **rs, PICS_PassFailReturnVal *status)
{
	TreeRatingStruct *trs = FindPICSGenericRatings(url_address);

	if(trs) {
		if(trs->ratings_passed)
			*status = PICS_RATINGS_PASSED;
		else
			*status = PICS_RATINGS_FAILED;

		*rs = trs->rs;
	}
}


void
nsPICS::PreferenceChanged(const char* aPrefName)
{
  // Initialize our state from the user preferences
  GetUserPreferences();
}



////////////////////////////////////////////////////////////////////////////////
// nsPICSFactory Implementation

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsPICSFactory, kIFactoryIID);

nsPICSFactory::nsPICSFactory(void)
{
    NS_INIT_REFCNT();
    printf("initializing my service factory\n");
    PR_AtomicIncrement(&g_InstanceCount);
}

nsPICSFactory::~nsPICSFactory(void)
{
    PR_AtomicDecrement(&g_InstanceCount);
    printf("finalizing my service factory\n");
}

nsresult
nsPICSFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;
    
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv;
    nsPICS* inst = new nsPICS();

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = inst->QueryInterface(aIID, aResult);

    if (NS_FAILED(rv)) {
        *aResult = NULL;
    }
    return rv;
}

nsresult
nsPICSFactory::LockFactory(PRBool aLock)
{
    if (aLock) {
      PR_AtomicIncrement(&g_LockCount);
    } else {
      PR_AtomicDecrement(&g_LockCount);
    }
    return NS_OK;
}



////////////////////////////////////////////////////////////////////////////////
// DLL Entry Points:

static NS_DEFINE_IID(kPICSCID, NS_PICS_CID);

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* servMgr, 
			 const nsCID &aClass, 
			 const char *aClassName,
             const char *aProgID,
			 nsIFactory **aFactory)
{
    if (! aFactory)
        return NS_ERROR_NULL_POINTER;
    
    if (aClass.Equals(kPICSCID)) {
        nsPICSFactory *factory = new nsPICSFactory();

        if (factory == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(factory);
        *aFactory = factory;
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* serviceMgr)
{
    return PRBool(g_InstanceCount == 0 && g_LockCount == 0);	
}

extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* serviceMgr, const char* aPath)
{
    return nsRepository::RegisterComponent(kPICSCID,
                                           "PICS", 
                                           NS_PICS_PROGID,
                                           aPath,PR_TRUE, PR_TRUE);
   
    return NS_OK;
}

extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* serviceMgr, const char* aPath)
{
    nsresult rv;

    rv = nsRepository::UnregisterComponent(kPICSCID,  aPath);
   
    return rv;
}



////////////////////////////////////////////////////////////////////////////////
