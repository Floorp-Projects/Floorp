/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define NS_IMPL_IDS
#include "pratom.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIPICS.h"
#include "nsIPref.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIHTMLContentSink.h"
#include "nsIObserverService.h"
#include "nsObserverService.h"
#include "nsPICSElementObserver.h"
#include "nsFileSpec.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentLoader.h"
#include "nsIWebProgressListener.h"
#include "nsIWebProgress.h"
#include "nsCURILoader.h"
//#include "nsIWebShell.h"
#include "nsIWebShellServices.h"
#include "nsIContentViewerContainer.h"
#include "nsHashtable.h"
#include "nsVoidArray.h"
#include "nscore.h"
#include "cslutils.h"
#include "csll.h"
#include "csllst.h"
#include "csparse.h"
#include "htstring.h"
#include "prprf.h"
#include "plstr.h"
#include "prmem.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include <stdio.h>

static NS_DEFINE_CID(kPrefCID,                    NS_PREF_CID);

static NS_DEFINE_IID(kDocLoaderServiceCID,        NS_DOCUMENTLOADER_SERVICE_CID);
static NS_DEFINE_IID(kIDocumentLoaderIID,         NS_IDOCUMENTLOADER_IID);

static NS_DEFINE_IID(kIWebShellServicesIID,       NS_IWEB_SHELL_SERVICES_IID);
//static NS_DEFINE_IID(kIWebShellIID,               NS_IWEB_SHELL_IID);

static NS_DEFINE_IID(kIStreamListenerIID,         NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIDocumentLoaderObserverIID, NS_IDOCUMENTLOADEROBSERVER_IID);

#define PICS_DOMAIN  			"browser.PICS."
#define PICS_ENABLED_PREF 		PICS_DOMAIN"ratings_enabled"
#define PICS_MUST_BE_RATED_PREF PICS_DOMAIN"pages_must_be_rated"
#define PICS_DISABLED_FOR_SESSION PICS_DOMAIN"disable_for_this_session"
#define PICS_REENABLE_FOR_SESSION PICS_DOMAIN"reenable_for_this_session"
#define PICS_URL_PREFIX "about:pics"

#define PICS_TO_UPPER(x) ((((PRUint32) (x)) > 0x7f) ? x : toupper(x))
#define PICS_TO_LOWER(x) ((((PRUint32) (x)) > 0x7f) ? x : tolower(x))

#define PICS_IS_ALPHA(x) ((((PRUint32) (x)) > 0x7f) ? 0 : isalpha(x))
#define PICS_IS_DIGIT(x) ((((PRUint32) (x)) > 0x7f) ? 0 : isdigit(x))
#define PICS_IS_SPACE(x) ((((PRUint32) (x)) > 0x7f) ? 0 : isspace(x))



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

typedef struct {
	nsIURI* url;
  PRBool notified;
} PICS_URLData;

////////////////////////////////////////////////////////////////////////////////

class nsPICS : public nsIPICS, 
               public nsIWebProgressListener {
public:

   // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER

  // nsIPICS
  static nsresult GetPICS(nsIPICS** aPICS);
  NS_IMETHOD
  ProcessPICSLabel(char *label);
 
  NS_IMETHOD GetWebShell(PRUint32 key, nsIWebShellServices*& aResult);

  NS_IMETHOD SetNotified(nsIWebShellServices* aResult, nsIURI* aURL, PRBool notified);

  nsPICS();
  virtual ~nsPICS(void);

protected:
  void   GetUserPreferences();
    
private:

  nsIPref* mPrefs;

//  nsPICSElementObserver* mPICSElementObserver;
  nsIObserver* mPICSElementObserver;
  nsCOMPtr<nsIWebShellServices> mWebShellServices;

  nsCOMPtr<nsIDocumentLoader> mDocLoaderService;
//  nsVoidArray* currentURLList;
  nsHashtable* mWebShellServicesURLTable;

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

  friend PRInt32 PrefChangedCallback(const char*, void*);

  nsresult Init();
  void   PreferenceChanged(const char* aPrefName);

  PICS_RatingsStruct * ParsePICSLabel(char * label);

  PRBool IsPICSEnabledByUser(void);

  PRBool AreRatingsRequired(void);

  char * GetURLFromRatingsStruct(PICS_RatingsStruct *rs, char *cur_page_url);

  void FreeRatingsStruct(PICS_RatingsStruct *rs);

  PICS_RatingsStruct * CopyRatingsStruct(PICS_RatingsStruct *rs);

  PRBool CanUserEnableAdditionalJavaCapabilities(void);

  /* returns PR_TRUE if page should be censored
   * PR_FALSE if page is allowed to be shown
   */
  PICS_PassFailReturnVal CompareToUserSettings(PICS_RatingsStruct *rs, char *cur_page_url);

  char * IllegalToUnderscore(char *string);

  char * LowercaseString(char *string);

  void AddPICSRatingsStructToTreeRatings(PICS_RatingsStruct *rs, PRBool ratings_passed);

  TreeRatingStruct * FindPICSGenericRatings(char *url_address);

  void CheckForGenericRatings(char *url_address, PICS_RatingsStruct **rs,
                                  PICS_PassFailReturnVal *status);

#if 0   // XXX no longer called?
  nsresult GetRootURL(nsIURI* aURL);
#endif
};

static nsPICS* gPICS = nsnull; // The one-and-only PICSService

////////////////////////////////////////////////////////////////////////////////


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
		    cd->rs->generic = PR_TRUE;
	    }

	    if(lo->fur.value && !cd->rs->fur) {
		    StrAllocCopy(cd->rs->fur, lo->fur.value);
	    }
    }
  } else if(target ==  CSLLTC_RATING) {
    PICS_RatingsStruct *rating_struct = cd->rs;
    // LabelOptions_t * lo = CSLabel_getLabelOptions(pCSLabel);

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

PRInt32 
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


NS_IMPL_ISUPPORTS2(nsPICS, nsIPICS, nsIWebProgressListener)

NS_EXPORT nsresult NS_NewPICS(nsIPICS** aPICS)
{
  return nsPICS::GetPICS(aPICS);
}

nsPICS::nsPICS()
{
  NS_INIT_REFCNT();

  mPrefs = nsnull;
  mPICSElementObserver = nsnull;
  mWebShellServicesURLTable = nsnull;
//  currentURLList = nsnull;

  mPICSRatingsEnabled = PR_FALSE;
  mPICSPagesMustBeRatedPref = PR_FALSE;
  mPICSDisabledForThisSession = PR_FALSE;
  mPICSJavaCapabilitiesEnabled = PR_TRUE;

  mPICSTreeRatings = nsnull;

  Init();
   
}

nsPICS::~nsPICS(void)
{
  if(mPICSTreeRatings)
    HTList_free(mPICSTreeRatings);
  if(mWebShellServicesURLTable)
    delete mWebShellServicesURLTable;
  gPICS = nsnull;
}

nsresult nsPICS::GetPICS(nsIPICS** aPICS)
{
  if (! gPICS) {
    nsPICS* it = new nsPICS();
    if (! it)
      return NS_ERROR_OUT_OF_MEMORY;
    gPICS = it;
  }

  NS_ADDREF(gPICS);
  *aPICS = gPICS;
  return NS_OK;
}

nsresult
nsPICS::ProcessPICSLabel(char *label)
{
	PICS_PassFailReturnVal status = PICS_RATINGS_PASSED;
  PICS_RatingsStruct *rs = nsnull;
	if (mPICSRatingsEnabled) {
    if (!label) {
      status = PICS_NO_RATINGS;
      return status;
    }
    while(label[0] == SQUOTE || label[0] == DQUOTE) {
      nsString theLabel(label);
	  char*  quoteValue = PL_strndup(label, 1);
  //	nsString value2(theValue2);
	  theLabel.Trim(quoteValue);
	  char *lab = ToNewCString(theLabel);
      PL_strcpy(label, lab);
    }
  //	rv = GetRootURL();
	  rs = ParsePICSLabel(label);
    status = CompareToUserSettings(rs, "http://www.w3.org/");
    if(status == PICS_NO_RATINGS) {
#ifdef DEBUG
        printf("PICS_PassFailReturnVal  %d", status);
#endif
        if(mPICSPagesMustBeRatedPref)
            status = PICS_RATINGS_FAILED;
        else
            status = PICS_RATINGS_PASSED;
    }
  }
#ifdef DEBUG
	printf("\nPICS_PassFailReturnVal  %d\n", status);
#endif
	FreeRatingsStruct(rs);
	return status;
}

nsresult
nsPICS::Init()
{
	nsresult rv;
    nsresult res;
    nsIPref* aPrefs;
    nsIObserverService *anObserverService = nsnull;
 //   nsFileSpec mySpec("C:\\Program Files\\Netscape\\Users\\neeti\\prefs.js");

    
    static PRBool first_time=PR_TRUE;

    res = nsServiceManager::GetService(kPrefCID, 
                                    NS_GET_IID(nsIPref), 
                                    (nsISupports **)&aPrefs);

    if (NS_OK != res) {
      return res;
    }

    mPrefs = aPrefs;

    if(!mPrefs)
      return NS_ERROR_NULL_POINTER;

    if(!first_time) {
      return NS_OK;
    } else {
	  /*  char *password=NULL; */

      first_time = PR_FALSE;
       
     //  mPrefs->StartUpWith(mySpec);
  
        /* get the prefs */
	    PrefChangedCallback(PICS_DOMAIN, (void*)this);

      mPrefs->RegisterCallback(PICS_DOMAIN, PrefChangedCallback, (void*)this);

     

      // Get the global document loader service...  
      mDocLoaderService = do_GetService(kDocLoaderServiceCID, &rv);
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIWebProgress> progress(do_QueryInterface(mDocLoaderService, &rv));
        if (NS_SUCCEEDED(rv)) {
            (void) process->AddProgressListener((nsIWebProgressListener*)this);
        }
      }

      if(mPICSRatingsEnabled) {
        if (NS_FAILED(rv = NS_NewPICSElementObserver(&mPICSElementObserver)))
          return rv;
  
        rv = nsServiceManager::GetService(NS_OBSERVERSERVICE_CONTRACTID, 
                                NS_GET_IID(nsIObserverService), 
                                (nsISupports **)&anObserverService);

        if(rv == NS_OK) {
          rv = anObserverService->AddObserver(mPICSElementObserver, "htmlparser", PR_FALSE);
          nsServiceManager::ReleaseService( NS_OBSERVERSERVICE_CONTRACTID, anObserverService );
          if (NS_FAILED(rv))
              return rv;

        }

      }

        

	   /*  check for security pref that password disables the enableing of
	     java permissions */
	    /*
        if(prefs->CopyCharPref(JAVA_SECURITY_PASSWORD, &password))
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
                mPICSJavaCapabilitiesEnabled = PR_FALSE;
            }
            else if(!PL_strcmp(hashed_password, password))
		    {
			    mPICSJavaCapabilitiesEnabled = PR_TRUE;
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

  if(mPrefs) {
    if(!this->mPrefs->GetBoolPref(PICS_ENABLED_PREF, &bool_rv)) 
      mPICSRatingsEnabled = bool_rv;
    if(!mPrefs->GetBoolPref(PICS_MUST_BE_RATED_PREF, &bool_rv))
      mPICSPagesMustBeRatedPref = bool_rv;
    if(!mPrefs->GetBoolPref(PICS_DISABLED_FOR_SESSION, &bool_rv)) {
      if(bool_rv) {
        mPICSDisabledForThisSession = PR_TRUE;
        mPrefs->SetBoolPref(PICS_DISABLED_FOR_SESSION, PR_FALSE);
      }
    }
    if(!mPrefs->GetBoolPref(PICS_REENABLE_FOR_SESSION, &bool_rv)) {
      if(bool_rv) {
        mPICSDisabledForThisSession = PR_FALSE;
        mPrefs->SetBoolPref(PICS_REENABLE_FOR_SESSION, PR_FALSE);
      }
    }
  }

}

NS_IMETHODIMP
nsPICS::GetWebShell(PRUint32 key, nsIWebShellServices*& aResult)
{
  nsresult rv;
  nsIContentViewerContainer* cont = nsnull;
  rv = mDocLoaderService->GetContentViewerContainer(key, &cont);
  if (NS_SUCCEEDED(rv) && nsnull != cont) {
    rv = cont->QueryInterface(kIWebShellServicesIID, (void **)&aResult);
    NS_RELEASE(cont);
  }
  return rv;
}

NS_IMETHODIMP
nsPICS::SetNotified(nsIWebShellServices* ws, nsIURI* aURL, PRBool notified)
{
  nsVoidArray* currentURLList;
  if(ws == nsnull)
    return NS_ERROR_NULL_POINTER;
  nsVoidKey key((void*)ws);

  if(mWebShellServicesURLTable == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  if(mWebShellServicesURLTable->Exists(&key)) {
    currentURLList = (nsVoidArray *) mWebShellServicesURLTable->Get(&key);
    if (currentURLList != NULL) {
      PRInt32 count = currentURLList->Count();
      for (PRInt32 i = 0; i < count; i++) {
        PICS_URLData* urlData = (PICS_URLData*)currentURLList->ElementAt(i);
        if(aURL == urlData->url) {
          urlData->notified = notified;
        }
      }
    }
  }
  return NS_OK;
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

#ifdef DEBUG
		if (rating_value->name) 
			printf(" rating_value->name: %s\n", rating_value->name);
		printf(" rating_value->value: %f\n\n", rating_value->value);
#endif
	}


	PR_Free(cd);

  CSParse_deleteLabel(CSParse_handle);

	return(new_rs);
}


// nsIWebProgressListener implementation
NS_IMETHODIMP
nsPICS::OnStateChange(nsIWebProgress* aWebProgress, 
                      nsIRequest *aRequest, 
                      PRInt32 progressStateFlags, 
                      nsresult aStatus)
{
  nsresult rv = NS_OK;

  // XXX HACK-ALERT XXX
  // I'm not adding this usage, just moving it around and I thought
  // I'd note that the fact that you can get a nsIWebShellServices
  // from here is an implementation detail and will likely fail at
  // some point in the future.
  nsCOMPtr<nsIDOMWindow> domWin;
  rv = aWebProgress->GetDOMWindow(getter_AddRefs(domWin));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIWebShellServices> wsServices(do_GetInterface(domWin));
  NS_ASSERTION(wsServices, "no nsIWebShellService available");

  // top-level URL loads
  if (progressStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT) {
    if (progressStateFlags & nsIWebProgressListener::STATE_STOP) {
      if(!mPICSRatingsEnabled) return rv;
      nsCOMPtr<nsISupports> cont;
      if(wsServices) {
          mWebShellServices = wsServices;
          mWebShellServices->SetRendering(PR_TRUE);
      }
    }
  } // END - STATE_IS_DOCUMENT

  // inline URL loads
  if (progressStateFlags & nsIWebProgressListener::STATE_IS_REQUEST) {
      rv = NS_OK;
      if (progressStateFlags & nsIWebProgressListener::STATE_START) {
          nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
          if (!channel) return NS_ERROR_FAILURE;

          nsCOMPtr<nsIURI> uri;
          rv = channel->GetURI(getter_AddRefs(uri));
          if (NS_FAILED(rv)) return rv;

          nsXPIDLCString aContentType;
          rv = channel->GetContentType(getter_Copies(aContentType));
          if (NS_FAILED(rv)) return rv;

          if(!mPICSRatingsEnabled) return rv;
 
          PICS_URLData* urlData;
          nsVoidArray* currentURLList;
  
          if(0 == PL_strcmp("text/html", aContentType)) {
             if (wsServices) {
               mWebShellServices = wsServices;
               mWebShellServices->SetRendering(PR_FALSE);
             }

             if (nsnull != uri) {
              urlData = PR_NEWZAP(PICS_URLData);
              urlData->url = (nsIURI*)PR_Malloc(sizeof(uri));
              urlData->url = uri;
              urlData->notified = PR_FALSE;
             }

            nsVoidKey key((void*)ws);

            if(mWebShellServicesURLTable == nsnull) {
                mWebShellServicesURLTable = new nsHashtable(256, PR_TRUE);
                if (mWebShellServicesURLTable == nsnull) return NS_ERROR_OUT_OF_MEMORY;
            }

            if(mWebShellServicesURLTable->Exists(&key)) {
              currentURLList = (nsVoidArray *) mWebShellServicesURLTable->Get(&key);
              if (currentURLList != NULL) {
	              currentURLList->AppendElement(urlData);   
                mWebShellServicesURLTable->Put(&key, currentURLList);
              } else {
                currentURLList = new nsVoidArray();
                if(!currentURLList) return NS_ERROR_OUT_OF_MEMORY;

                mWebShellServicesURLTable->Put(&key, currentURLList);
              }
            } else {
              currentURLList = new nsVoidArray();
              if (!currentURLList) return NS_ERROR_OUT_OF_MEMORY;

              mWebShellServicesURLTable->Put(&key, currentURLList);
            }
  
          }
          return rv;      
      }

      if (progressStateFlags & nsIWebProgressListener::STATE_STOP) {
          nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

          nsCOMPtr<nsIURI> aURL;
          rv = channel->GetURI(getter_AddRefs(aURL));
          if (NS_FAILED(rv)) return rv;

          nsVoidArray* currentURLList;

          if(!mPICSRatingsEnabled) return NS_OK;
 
          mWebShellServices = wsServices;

          nsVoidKey key((void*)wsServices);

          if(mWebShellServicesURLTable == nsnull) {
            return NS_ERROR_NULL_POINTER;
          }

          if(mWebShellServicesURLTable->Exists(&key)) {
            currentURLList = (nsVoidArray *) mWebShellServicesURLTable->Get(&key);
            if (currentURLList != NULL) {
              PRInt32 count = currentURLList->Count();
              for (PRInt32 i = 0; i < count; i++) {
                PICS_URLData* urlData = (PICS_URLData*)currentURLList->ElementAt(i);
                if(urlData == nsnull)
                  continue;
                nsXPIDLCString spec1;
                nsXPIDLCString spec2;
        
                if(aURL == nsnull)
                  continue;
                aURL->GetSpec(getter_Copies(spec1));
                if(spec1 == nsnull)
                  continue;

                if(urlData->url == nsnull) 
                  continue;

                (urlData->url)->GetSpec(getter_Copies(spec2));
                if(spec2 == nsnull) 
                  continue;

                if(0 == PL_strcmp(spec1, spec2)) {
                  if(!urlData->notified) {
                    currentURLList->RemoveElementAt(i);
                    if (nsnull != aURL) {
                      nsXPIDLCString uProtocol;
                      nsXPIDLCString uHost;
                      nsXPIDLCString uFile;

                      aURL->GetScheme(getter_Copies(uProtocol));
                      aURL->GetHost(getter_Copies(uHost));
                      aURL->GetPath(getter_Copies(uFile));
                      if ((0 != PL_strcmp("/", uFile)) && (0 != PL_strcmp("/index.html", uFile))) {
                        if (0 != PL_strcmp("file", uProtocol)) {
                          nsAutoString protocolStr(uProtocol);
                          nsAutoString hostStr(uHost);

                          // Construct a chrome URL and use it to look up a resource.
                          nsAutoString rootStr(protocolStr + "://" + hostStr + "/");
                
                          // XXX if we're no longer calling GetRootURL, these calls can go away
                          // rv = NS_NewURI(&rootURL, rootStr);
                       //   rv = GetRootURL(rootURL);
                        }
                      }
                    }
                  }
                }
              }
            }
          }
      }
  } // END - STATE_IS_REQUEST

  return rv;
}

NS_IMETHODIMP
nsPICS::OnProgressChange(nsIWebProgress *aWebProgress,
                                     nsIRequest *aRequest,
                                     PRInt32 aCurSelfProgress,
                                     PRInt32 aMaxSelfProgress,
                                     PRInt32 aCurTotalProgress,
                                     PRInt32 aMaxTotalProgress) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPICS::OnLocationChange(nsIWebProgress* aWebProgress,
                         nsIRequest* aRequest,
                         nsIURI *location) {
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsPICS::OnStatusChange(nsIWebProgress* aWebProgress,
                       nsIRequest* aRequest,
                       nsresult aStatus,
                       const PRUnichar* aMessage) {
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsPICS::OnSecurityChange(nsIWebProgress *aWebProgress, 
                         nsIRequest *aRequest, 
                         PRInt32 state) {
    return NS_ERROR_NOT_IMPLEMENTED;
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
	  return PR_FALSE;

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

/* returns PR_TRUE if page should be censored
 * PR_FALSE if page is allowed to be shown
 */

PICS_PassFailReturnVal
nsPICS::CompareToUserSettings(PICS_RatingsStruct *rs, char *cur_page_url)
{
  PRInt32 int_pref;
  PRBool bool_pref;
  char * pref_prefix = NULL;
  char * pref_string=NULL;
  char * escaped_service = NULL;
  PICS_PassFailReturnVal rv = PICS_RATINGS_PASSED;
  HTList *list_ptr;
  PICS_RatingValue *rating_value;

  if(!rs){
    return PICS_NO_RATINGS;
  }

  if(!*rs->service) {
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

  PR_FREEIF(escaped_service);

  if(!pref_prefix)
    return PICS_RATINGS_FAILED;

  /* verify that this type of rating system is enabled */
  pref_string = PR_smprintf("%s.%s", pref_prefix, PICS_SERVICE_ENABLED);

  if(!pref_string)
    goto cleanup;

  if(mPrefs) {
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

    if(mPrefs) {
      if(!mPrefs->GetIntPref(pref_string, &int_pref)) {
      if(rating_value->value > int_pref) {
        rv = PICS_RATINGS_FAILED;
        PR_Free(pref_string);
        goto cleanup;
      }
    }
  }

  PR_Free(pref_string);
}

cleanup:

  PR_Free(pref_prefix);

  /* make sure this rating applies to this page  */
  if(rs->fur) {
 //   char *new_url;
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

#if 0
//#define UA_CSS_URL "resource:/res/ua.css"
#define UA_CSS_URL "http://www.w3.org/"
nsresult
nsPICS::GetRootURL(nsIURI* aURL)
{
	static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
	static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

	nsresult rv;
	nsIParser *mParser;
    nsIContentSink* sink;
    nsIStreamListener* lsnr = nsnull;

	
	rv = nsComponentManager::CreateInstance(kCParserCID, 
									  nsnull, 
									  kCParserIID, 
									  (void **)&mParser);

	if (NS_OK == rv) { 
		
	  NS_NewHTMLNullSink(&sink);

    if (sink) {
        mParser->SetContentSink(sink);
        if (NS_FAILED(rv = mParser->QueryInterface(kIStreamListenerIID, (void**) &lsnr)))
            return rv;
        mParser->Parse(aURL);
        rv = NS_OpenURI(lsnr, 
                        nsnull,     // null context 
                        aURL,       // nsIURI
                        nsnull);    // null load group
	  }
	  NS_RELEASE(sink);
	}
	return rv;

}
#endif

//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.

static NS_IMETHODIMP       
CreateNewPICS(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{                                                         
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }                                                                
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }                                                                
    nsIPICS* inst;                                                
    nsresult rv = NS_NewPICS(&inst);                              
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
        return rv;                                                   
    }                                                                
    rv = inst->QueryInterface(aIID, aResult);                        
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;                                                       
}

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//
// NS_GENERIC_FACTORY_CONSTRUCTOR(nsPICS)

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static nsModuleComponentInfo components[] = {
    { "PICS", NS_PICS_CID, NS_PICS_CONTRACTID, CreateNewPICS, },
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
NS_IMPL_NSGETMODULE(nsPICSModule, components)
