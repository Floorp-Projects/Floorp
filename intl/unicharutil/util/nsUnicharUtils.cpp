/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Alec Flett <alecf@netscape.com>
 */
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtilCIID.h"
#include "nsICaseConversion.h"
#include "nsIServiceManager.h"

#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsObserverService.h"

// global cache of the case conversion service
static nsICaseConversion *gCaseConv = nsnull;
static NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);

class nsShutdownObserver : public nsIObserver
{
public:
    nsShutdownObserver() { NS_INIT_REFCNT(); }
    virtual ~nsShutdownObserver() {}
    NS_DECL_ISUPPORTS
    
    NS_IMETHOD Observe(nsISupports *aSubject, const char *aTopic,
                       const PRUnichar *aData)
    {
        if (nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)==0) {
            NS_IF_RELEASE(gCaseConv);
        }

        return NS_OK;
    }

};

NS_IMPL_ISUPPORTS1(nsShutdownObserver, nsIObserver)

static nsresult NS_InitCaseConversion() {
    if (gCaseConv) return NS_OK;

    nsresult rv;
    
    rv = nsServiceManager::GetService(kUnicharUtilCID,
                                      NS_GET_IID(nsICaseConversion),
                                      (nsISupports**)&gCaseConv);

    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIObserverService> obs =
            do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
            nsShutdownObserver *observer = new nsShutdownObserver();
            if (observer)
                obs->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
        }
    }
    
    return NS_OK;
}


// to be turned on for bug 100214
#if 0
class ConvertToLowerCase
{
public:
    typedef PRUnichar value_type;
    
    PRUint32 write( const PRUnichar* aSource, PRUint32 aSourceLength)
    {
        NS_InitCaseConversion();
        gCaseConv->ToLower(aSource, NS_CONST_CAST(PRUnichar*,aSource), aSourceLength);
        return aSourceLength;
    }
};

void
ToLowerCase( nsAString& aString)
  {
    nsAString::iterator fromBegin, fromEnd;
    ConvertToLowerCase converter;
    copy_string(aString.BeginWriting(fromBegin), aString.EndWriting(fromEnd), converter);
  }

class ConvertToUpperCase
{
public:
    typedef PRUnichar value_type;
    
    PRUint32 write( const PRUnichar* aSource, PRUint32 aSourceLength)
    {
        NS_InitCaseConversion();
        gCaseConv->ToUpper(aSource, NS_CONST_CAST(PRUnichar*,aSource), aSourceLength);
        return aSourceLength;
    }
};

void
ToUpperCase( nsAString& aString )
  {
    nsAString::iterator fromBegin, fromEnd;
    ConvertToUpperCase converter;
    copy_string(aString.BeginWriting(fromBegin), aString.EndWriting(fromEnd), converter);
  }

PRBool
CaseInsensitiveFindInReadable( const nsAString& aPattern, nsAString::const_iterator& aSearchStart, nsAString::const_iterator& aSearchEnd )
{
    return FindInReadable(aPattern, aSearchStart, aSearchEnd, nsCaseInsensitiveStringComparator());
}


int
nsCaseInsensitiveStringComparator::operator()( const PRUnichar* lhs, const PRUnichar* rhs, PRUint32 aLength ) const
  {
      NS_InitCaseConversion();
      PRInt32 result;
      gCaseConv->CaseInsensitiveCompare(lhs, rhs, aLength, &result);
      return result;
  }

int
nsCaseInsensitiveStringComparator::operator()( PRUnichar lhs, PRUnichar rhs ) const
  {
      if (lhs == rhs) return 0;
      NS_InitCaseConversion();

      gCaseConv->ToLower(lhs, &lhs);
      gCaseConv->ToLower(rhs, &rhs);

      if (lhs == rhs) return 0;
      if (lhs < rhs) return -1;
      return 1;
  }

#endif

PRUnichar
ToLowerCase(PRUnichar aChar)
{
    PRUnichar result;
    if (NS_FAILED(NS_InitCaseConversion()))
        return aChar;
    
    gCaseConv->ToLower(aChar, &result);
    return result;
}

PRUnichar
ToUpperCase(PRUnichar aChar)
{
    PRUnichar result;
    if (NS_FAILED(NS_InitCaseConversion()))
        return aChar;
        
    gCaseConv->ToUpper(aChar, &result);
    return result;
}

