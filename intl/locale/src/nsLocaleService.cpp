/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_QT
#include <QString>
#include <QtCore/QLocale>
#endif

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsLocale.h"
#include "nsLocaleCID.h"
#include "nsServiceManagerUtils.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "prprf.h"
#include "nsTArray.h"

#include <ctype.h>

#if defined(XP_WIN)
#  include "nsWin32Locale.h"
#elif defined(XP_OS2)
#  include "unidef.h"
#  include "nsIOS2Locale.h"
#elif defined(XP_MACOSX)
#  include <Carbon/Carbon.h>
#elif defined(XP_UNIX)
#  include <locale.h>
#  include <stdlib.h>
#  include "nsPosixLocale.h"
#endif

//
// implementation constants
const int LocaleListLength = 6;
const char* LocaleList[LocaleListLength] = 
{
	NSILOCALE_COLLATE,
	NSILOCALE_CTYPE,
	NSILOCALE_MONETARY,
	NSILOCALE_NUMERIC,
	NSILOCALE_TIME,
	NSILOCALE_MESSAGE
};

#define NSILOCALE_MAX_ACCEPT_LANGUAGE	16
#define NSILOCALE_MAX_ACCEPT_LENGTH		18

#if (defined(XP_UNIX) && !defined(XP_MACOSX)) || defined(XP_OS2)
static int posix_locale_category[LocaleListLength] =
{
  LC_COLLATE,
  LC_CTYPE,
  LC_MONETARY,
  LC_NUMERIC,
  LC_TIME,
#ifdef HAVE_I18N_LC_MESSAGES
  LC_MESSAGES
#else
  LC_CTYPE
#endif
};
#endif

//
// nsILocaleService implementation
//
class nsLocaleService: public nsILocaleService {

public:
	
	//
	// nsISupports
	//
	NS_DECL_THREADSAFE_ISUPPORTS

	//
	// nsILocaleService
	//
    NS_DECL_NSILOCALESERVICE


	nsLocaleService(void);
	virtual ~nsLocaleService(void);

protected:

	nsresult SetSystemLocale(void);
	nsresult SetApplicationLocale(void);

	nsCOMPtr<nsILocale>				mSystemLocale;
	nsCOMPtr<nsILocale>				mApplicationLocale;

};

//
// nsLocaleService methods
//
nsLocaleService::nsLocaleService(void) 
{
#ifdef XP_WIN
    nsAutoString        xpLocale;
    //
    // get the system LCID
    //
    LCID win_lcid = GetSystemDefaultLCID();
    NS_ENSURE_TRUE_VOID(win_lcid);
    nsWin32Locale::GetXPLocale(win_lcid, xpLocale);
    nsresult rv = NewLocale(xpLocale, getter_AddRefs(mSystemLocale));
    NS_ENSURE_SUCCESS_VOID(rv);

    //
    // get the application LCID
    //
    win_lcid = GetUserDefaultLCID();
    NS_ENSURE_TRUE_VOID(win_lcid);
    nsWin32Locale::GetXPLocale(win_lcid, xpLocale);
    rv = NewLocale(xpLocale, getter_AddRefs(mApplicationLocale));
    NS_ENSURE_SUCCESS_VOID(rv);
#endif
#if defined(XP_UNIX) && !defined(XP_MACOSX)
    nsRefPtr<nsLocale> resultLocale(new nsLocale());
    NS_ENSURE_TRUE_VOID(resultLocale);

#ifdef MOZ_WIDGET_QT
    const char* lang = QLocale::system().name().toUtf8();
#else
    // Get system configuration
    const char* lang = getenv("LANG");
#endif

    nsAutoString xpLocale, platformLocale;
    nsAutoString category, category_platform;
    int i;

    for( i = 0; i < LocaleListLength; i++ ) {
        nsresult result;
        // setlocale( , "") evaluates LC_* and LANG
        char* lc_temp = setlocale(posix_locale_category[i], "");
        CopyASCIItoUTF16(LocaleList[i], category);
        category_platform = category;
        category_platform.AppendLiteral("##PLATFORM");
        if (lc_temp != nullptr) {
            result = nsPosixLocale::GetXPLocale(lc_temp, xpLocale);
            CopyASCIItoUTF16(lc_temp, platformLocale);
        } else {
            if ( lang == nullptr ) {
                platformLocale.AssignLiteral("en_US");
                result = nsPosixLocale::GetXPLocale("en-US", xpLocale);
            } else {
                CopyASCIItoUTF16(lang, platformLocale);
                result = nsPosixLocale::GetXPLocale(lang, xpLocale);
            }
        }
        if (NS_FAILED(result)) {
            return;
        }
        resultLocale->AddCategory(category, xpLocale);
        resultLocale->AddCategory(category_platform, platformLocale);
    }
    mSystemLocale = do_QueryInterface(resultLocale);
    mApplicationLocale = do_QueryInterface(resultLocale);
       
#endif // XP_UNIX
#ifdef XP_OS2
    nsCOMPtr<nsIOS2Locale> os2Converter = do_GetService(NS_OS2LOCALE_CONTRACTID);
    nsAutoString xpLocale;
    if (os2Converter) {
        nsAutoString category;
        int i;

        nsRefPtr<nsLocale> resultLocale(new nsLocale());

        LocaleObject locale_object = NULL;
        int result = UniCreateLocaleObject(UNI_UCS_STRING_POINTER,
                                           (UniChar *)L"", &locale_object);
        if (result != ULS_SUCCESS) {
            int result = UniCreateLocaleObject(UNI_UCS_STRING_POINTER,
                                               (UniChar *)L"en_US", &locale_object);
        }
        char* lc_temp;
        for( i = 0; i < LocaleListLength; i++ ) {
            lc_temp = nullptr;
            UniQueryLocaleObject(locale_object,
                                 posix_locale_category[i],
                                 UNI_MBS_STRING_POINTER,
                                 (void **)&lc_temp);
            category.AssignASCII(LocaleList[i]);
            nsresult result;
            if (lc_temp != nullptr)
                result = os2Converter->GetXPLocale(lc_temp, xpLocale);
            else {
                char* lang = getenv("LANG");
                if ( lang == nullptr ) {
                    result = os2Converter->GetXPLocale("en-US", xpLocale);
                }
                else
                    result = os2Converter->GetXPLocale(lang, xpLocale); 
            }
            if (NS_FAILED(result)) {
                UniFreeMem(lc_temp);
                UniFreeLocaleObject(locale_object);
                return;
            }
            resultLocale->AddCategory(category, xpLocale);
            UniFreeMem(lc_temp);
        }
        UniFreeLocaleObject(locale_object);
        mSystemLocale = do_QueryInterface(resultLocale);
        mApplicationLocale = do_QueryInterface(resultLocale);
    }  // if ( NS_SUCCEEDED )...
#endif  // XP_OS2

#ifdef XP_MACOSX
    // Get string representation of user's current locale
    CFLocaleRef userLocaleRef = ::CFLocaleCopyCurrent();
    CFStringRef userLocaleStr = ::CFLocaleGetIdentifier(userLocaleRef);
    ::CFRetain(userLocaleStr);

    nsAutoTArray<UniChar, 32> buffer;
    int size = ::CFStringGetLength(userLocaleStr);
    if (buffer.SetLength(size + 1))
    {
        CFRange range = ::CFRangeMake(0, size);
        ::CFStringGetCharacters(userLocaleStr, range, buffer.Elements());
        buffer[size] = 0;

        // Convert the locale string to the format that Mozilla expects
        nsAutoString xpLocale(buffer.Elements());
        xpLocale.ReplaceChar('_', '-');

        nsresult rv = NewLocale(xpLocale, getter_AddRefs(mSystemLocale));
        if (NS_SUCCEEDED(rv)) {
            mApplicationLocale = mSystemLocale;
        }
    }

    ::CFRelease(userLocaleStr);
    ::CFRelease(userLocaleRef);

    NS_ASSERTION(mApplicationLocale, "Failed to create locale objects");
#endif // XP_MACOSX
}

nsLocaleService::~nsLocaleService(void)
{
}

NS_IMPL_ISUPPORTS1(nsLocaleService, nsILocaleService)

NS_IMETHODIMP
nsLocaleService::NewLocale(const nsAString &aLocale, nsILocale **_retval)
{
    nsresult result;

    *_retval = nullptr;

    nsRefPtr<nsLocale> resultLocale(new nsLocale());
    if (!resultLocale) return NS_ERROR_OUT_OF_MEMORY;

    for (int32_t i = 0; i < LocaleListLength; i++) {
      NS_ConvertASCIItoUTF16 category(LocaleList[i]);
      result = resultLocale->AddCategory(category, aLocale);
      if (NS_FAILED(result)) return result;
#if defined(XP_UNIX) && !defined(XP_MACOSX)
      category.AppendLiteral("##PLATFORM");
      result = resultLocale->AddCategory(category, aLocale);
      if (NS_FAILED(result)) return result;
#endif
    }

    NS_ADDREF(*_retval = resultLocale);
    return NS_OK;
}


NS_IMETHODIMP
nsLocaleService::GetSystemLocale(nsILocale **_retval)
{
	if (mSystemLocale) {
		NS_ADDREF(*_retval = mSystemLocale);
		return NS_OK;
	}

	*_retval = (nsILocale*)nullptr;
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocaleService::GetApplicationLocale(nsILocale **_retval)
{
	if (mApplicationLocale) {
		NS_ADDREF(*_retval = mApplicationLocale);
		return NS_OK;
	}

	*_retval=(nsILocale*)nullptr;
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocaleService::GetLocaleFromAcceptLanguage(const char *acceptLanguage, nsILocale **_retval)
{
  char* cPtr;
  char* cPtr1;
  char* cPtr2;
  int i;
  int j;
  int countLang = 0;
  char	acceptLanguageList[NSILOCALE_MAX_ACCEPT_LANGUAGE][NSILOCALE_MAX_ACCEPT_LENGTH];
  nsresult	result;

  nsAutoArrayPtr<char> input(new char[strlen(acceptLanguage)+1]);

  strcpy(input, acceptLanguage);
  cPtr1 = input-1;
  cPtr2 = input;

  /* put in standard form */
  while (*(++cPtr1)) {
    if      (isalpha(*cPtr1))  *cPtr2++ = tolower(*cPtr1); /* force lower case */
    else if (isspace(*cPtr1))  ;                           /* ignore any space */
    else if (*cPtr1=='-')      *cPtr2++ = '_';             /* "-" -> "_"       */
    else if (*cPtr1=='*')      ;                           /* ignore "*"       */
    else                       *cPtr2++ = *cPtr1;          /* else unchanged   */
  }
  *cPtr2 = '\0';

  countLang = 0;

  if (strchr(input,';')) {
    /* deal with the quality values */

    float qvalue[NSILOCALE_MAX_ACCEPT_LANGUAGE];
    float qSwap;
    float bias = 0.0f;
    char* ptrLanguage[NSILOCALE_MAX_ACCEPT_LANGUAGE];
    char* ptrSwap;

    cPtr = nsCRT::strtok(input,",",&cPtr2);
    while (cPtr) {
      qvalue[countLang] = 1.0f;
      /* add extra parens to get rid of warning */
      if ((cPtr1 = strchr(cPtr,';')) != nullptr) {
        PR_sscanf(cPtr1,";q=%f",&qvalue[countLang]);
        *cPtr1 = '\0';
      }
      if (strlen(cPtr)<NSILOCALE_MAX_ACCEPT_LANGUAGE) {     /* ignore if too long */
        qvalue[countLang] -= (bias += 0.0001f); /* to insure original order */
        ptrLanguage[countLang++] = cPtr;
        if (countLang>=NSILOCALE_MAX_ACCEPT_LANGUAGE) break; /* quit if too many */
      }
      cPtr = nsCRT::strtok(cPtr2,",",&cPtr2);
    }

    /* sort according to decending qvalue */
    /* not a very good algorithm, but count is not likely large */
    for ( i=0 ; i<countLang-1 ; i++ ) {
      for ( j=i+1 ; j<countLang ; j++ ) {
        if (qvalue[i]<qvalue[j]) {
          qSwap     = qvalue[i];
          qvalue[i] = qvalue[j];
          qvalue[j] = qSwap;
          ptrSwap        = ptrLanguage[i];
          ptrLanguage[i] = ptrLanguage[j];
          ptrLanguage[j] = ptrSwap;
        }
      }
    }
    for ( i=0 ; i<countLang ; i++ ) {
      PL_strncpyz(acceptLanguageList[i],ptrLanguage[i],NSILOCALE_MAX_ACCEPT_LENGTH);
    }

  } else {
    /* simple case: no quality values */

    cPtr = nsCRT::strtok(input,",",&cPtr2);
    while (cPtr) {
      if (strlen(cPtr)<NSILOCALE_MAX_ACCEPT_LENGTH) {        /* ignore if too long */
        PL_strncpyz(acceptLanguageList[countLang++],cPtr,NSILOCALE_MAX_ACCEPT_LENGTH);
        if (countLang>=NSILOCALE_MAX_ACCEPT_LENGTH) break; /* quit if too many */
      }
      cPtr = nsCRT::strtok(cPtr2,",",&cPtr2);
    }
  }

  //
  // now create the locale 
  //
  result = NS_ERROR_FAILURE;
  if (countLang>0) {
	  result = NewLocale(NS_ConvertASCIItoUTF16(acceptLanguageList[0]), _retval);
  }

  //
  // clean up
  //
  return result;
}


nsresult
nsLocaleService::GetLocaleComponentForUserAgent(nsAString& retval)
{
    nsCOMPtr<nsILocale>     system_locale;
    nsresult                result;

    result = GetSystemLocale(getter_AddRefs(system_locale));
    if (NS_SUCCEEDED(result))
    {
        result = system_locale->
                 GetCategory(NS_LITERAL_STRING(NSILOCALE_MESSAGE), retval);
        return result;
    }

    return result;
}



nsresult
NS_NewLocaleService(nsILocaleService** result)
{
  if(!result)
    return NS_ERROR_NULL_POINTER;
  *result = new nsLocaleService();
  if (! *result)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*result);
  return NS_OK;
}
