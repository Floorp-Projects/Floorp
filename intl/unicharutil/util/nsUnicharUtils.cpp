/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Unicode case conversion helpers.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsUnicharUtils.h"
#include "nsUnicharUtilCIID.h"
#include "nsICaseConversion.h"
#include "nsIServiceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsCRTGlue.h"
#include "nsXPCOMStrings.h"

#include "nsIObserver.h"
#include "nsIObserverService.h"

#include <ctype.h>

// global cache of the case conversion service
static nsICaseConversion *gCaseConv = nsnull;

class nsShutdownObserver : public nsIObserver
{
public:
    nsShutdownObserver() { }
    virtual ~nsShutdownObserver() {}
    NS_DECL_ISUPPORTS
    
    NS_IMETHOD Observe(nsISupports *aSubject, const char *aTopic,
                       const PRUnichar *aData)
    {
        if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)==0) {
            NS_IF_RELEASE(gCaseConv);
        }

        return NS_OK;
    }

};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsShutdownObserver, nsIObserver)

static nsresult NS_InitCaseConversion() {
    if (gCaseConv)
      return NS_OK;
     
    nsresult rv = CallGetService(NS_UNICHARUTIL_CONTRACTID, &gCaseConv);
    if (NS_FAILED(rv)) {
      gCaseConv = nsnull;
      return rv;
    }

    nsCOMPtr<nsIObserverService> obs =
        do_GetService("@mozilla.org/observer-service;1", &rv);
 
     if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIObserver> observer;
        NS_NEWXPCOM(observer, nsShutdownObserver);
        if (observer)
            obs->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                             PR_FALSE);
     }
     
     return NS_OK;
}

void
ToLowerCase( nsAString& aString )
  {
    NS_InitCaseConversion();
    if (gCaseConv) {
      PRUnichar *buf = aString.BeginWriting();
      gCaseConv->ToLower(buf, buf, aString.Length());
    }
    else
      NS_WARNING("No case converter: no conversion done");
  }

void
ToLowerCase( const nsAString& aSource, nsAString& aDest )
  {
    NS_InitCaseConversion();

    const PRUnichar *in;
    PRUint32 len = NS_StringGetData(aSource, &in);

    PRUnichar *out;
    NS_StringGetMutableData(aDest, len, &out);

    if (out && gCaseConv) {
      gCaseConv->ToLower(in, out, len);
    }
    else {
      NS_WARNING("No case converter: only copying");
      aDest.Assign(aSource);
    }
  }

void
ToUpperCase( nsAString& aString )
  {
    NS_InitCaseConversion();
    if (gCaseConv) {
      PRUnichar *buf = aString.BeginWriting();
      gCaseConv->ToUpper(buf, buf, aString.Length());
    }
    else
      NS_WARNING("No case converter: no conversion done");
  }

void
ToUpperCase( const nsAString& aSource, nsAString& aDest )
  {
    NS_InitCaseConversion();

    const PRUnichar *in;
    PRUint32 len = NS_StringGetData(aSource, &in);

    PRUnichar *out;
    NS_StringGetMutableData(aDest, len, &out);

    if (out && gCaseConv) {
      gCaseConv->ToUpper(in, out, len);
    }
    else {
      NS_WARNING("No case converter: only copying");
      aDest.Assign(aSource);
    }
  }

#ifdef MOZILLA_INTERNAL_API
int
nsCaseInsensitiveStringComparator::operator()( const PRUnichar* lhs, const PRUnichar* rhs, PRUint32 aLength ) const
  {
      NS_InitCaseConversion();
      PRInt32 result;
      if (gCaseConv) {
          gCaseConv->CaseInsensitiveCompare(lhs, rhs, aLength, &result);
      }
      else {
          NS_WARNING("No case converter: using default");
          nsDefaultStringComparator comparator;
          result = comparator(lhs, rhs, aLength);
      }
      return result;
  }

int
nsCaseInsensitiveStringComparator::operator()( PRUnichar lhs, PRUnichar rhs ) const
  {
      // see if they're an exact match first
      if (lhs == rhs) return 0;
      
      NS_InitCaseConversion();

      if (gCaseConv) {
          gCaseConv->ToLower(lhs, &lhs);
          gCaseConv->ToLower(rhs, &rhs);
      } else {
          if (lhs < 256)
              lhs = tolower(char(lhs));
          if (rhs < 256)
              rhs = tolower(char(rhs));
          NS_WARNING("No case converter: no conversion done");
      }
      
      if (lhs == rhs) return 0;
      if (lhs < rhs) return -1;
      return 1;
  }
#else
PRInt32
CaseInsensitiveCompare(const PRUnichar *a, const PRUnichar *b, PRUint32 len)
{
    if (NS_FAILED(NS_InitCaseConversion()))
        return NS_strcmp(a, b);

    PRInt32 result;
    gCaseConv->CaseInsensitiveCompare(a, b, len, &result);
    return result;
}

#endif // MOZILLA_INTERNAL_API

PRUnichar
ToLowerCase(PRUnichar aChar)
{
    PRUnichar result;
    if (NS_FAILED(NS_InitCaseConversion()))
        return aChar;

    if (gCaseConv)
        gCaseConv->ToLower(aChar, &result);
    else {
        NS_WARNING("No case converter: no conversion done");
        if (aChar < 256)
            result = tolower(char(aChar));
        else
            result = aChar;
    }
    return result;
}

PRUnichar
ToUpperCase(PRUnichar aChar)
{
    PRUnichar result;
    if (NS_FAILED(NS_InitCaseConversion()))
        return aChar;

    if (gCaseConv)
        gCaseConv->ToUpper(aChar, &result);
    else {
        NS_WARNING("No case converter: no conversion done");
        if (aChar < 256)
            result = toupper(char(aChar));
        else
            result = aChar;
    }
    return result;
}

