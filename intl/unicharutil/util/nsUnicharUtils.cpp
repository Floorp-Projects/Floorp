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

#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtilCIID.h"
#include "nsICaseConversion.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"

#include "nsIObserver.h"
#include "nsIObserverService.h"

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
    
    rv = CallGetService(NS_UNICHARUTIL_CONTRACTID, &gCaseConv);

    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIObserverService> obs =
            do_GetService("@mozilla.org/observer-service;1", &rv);
        if (NS_SUCCEEDED(rv)) {
            nsShutdownObserver *observer = new nsShutdownObserver();
            if (observer)
                obs->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
        }
    }
    
    return NS_OK;
}

class ConvertToLowerCase
{
public:
    typedef PRUnichar value_type;
    
    ConvertToLowerCase()
    {
        NS_InitCaseConversion();
    }

    PRUint32 write( const PRUnichar* aSource, PRUint32 aSourceLength)
    {
        if (gCaseConv)
            gCaseConv->ToLower(aSource, NS_CONST_CAST(PRUnichar*,aSource), aSourceLength);
        else
            NS_WARNING("No case converter: no conversion done");

        return aSourceLength;
    }
};

void
ToLowerCase( nsAString& aString )
  {
    nsAString::iterator fromBegin, fromEnd;
    ConvertToLowerCase converter;
    copy_string(aString.BeginWriting(fromBegin), aString.EndWriting(fromEnd), converter);
  }

void
ToLowerCase( nsASingleFragmentString& aString )
  {
    ConvertToLowerCase converter;
    PRUnichar* start;
    converter.write(aString.BeginWriting(start), aString.Length());
  }

void
ToLowerCase( nsString& aString )
  {
    ConvertToLowerCase converter;
    PRUnichar* start;
    converter.write(aString.BeginWriting(start), aString.Length());
  }

class CopyToLowerCase
  {
    public:
      typedef PRUnichar value_type;
    
      CopyToLowerCase( nsAString::iterator& aDestIter )
        : mIter(aDestIter)
        {
          NS_InitCaseConversion();
        }

      PRUint32 write( const PRUnichar* aSource, PRUint32 aSourceLength )
        {
          PRUint32 len = PR_MIN(PRUint32(mIter.size_forward()), aSourceLength);
          PRUnichar* dest = mIter.get();
          if (gCaseConv)
              gCaseConv->ToLower(aSource, dest, len);
          else {
              NS_WARNING("No case converter: only copying");
              memcpy((void*)aSource, (void*)dest, len * sizeof(*aSource));
          }
          mIter.advance(len);
          return len;
        }

    protected:
      nsAString::iterator& mIter;
  };

void
ToLowerCase( const nsAString& aSource, nsAString& aDest )
  {
    nsAString::const_iterator fromBegin, fromEnd;
    nsAString::iterator toBegin;
    aDest.SetLength(aSource.Length());
    CopyToLowerCase converter(aDest.BeginWriting(toBegin));
    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter);
  }

class ConvertToUpperCase
{
public:
    typedef PRUnichar value_type;
    
    ConvertToUpperCase()
    {
        NS_InitCaseConversion();
    }
    
    PRUint32 write( const PRUnichar* aSource, PRUint32 aSourceLength)
    {
        if (gCaseConv)
            gCaseConv->ToUpper(aSource, NS_CONST_CAST(PRUnichar*,aSource), aSourceLength);
        else
            NS_WARNING("No case converter: no conversion done");
        
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

void
ToUpperCase( nsASingleFragmentString& aString )
  {
    ConvertToUpperCase converter;
    PRUnichar* start;
    converter.write(aString.BeginWriting(start), aString.Length());
  }

void
ToUpperCase( nsString& aString )
  {
    ConvertToUpperCase converter;
    PRUnichar* start;
    converter.write(aString.BeginWriting(start), aString.Length());
  }

class CopyToUpperCase
  {
    public:
      typedef PRUnichar value_type;
    
      CopyToUpperCase( nsAString::iterator& aDestIter )
        : mIter(aDestIter)
        {
          NS_InitCaseConversion();
        }

      PRUint32 write( const PRUnichar* aSource, PRUint32 aSourceLength )
        {
          PRUint32 len = PR_MIN(PRUint32(mIter.size_forward()), aSourceLength);
          PRUnichar* dest = mIter.get();
          if (gCaseConv)
              gCaseConv->ToUpper(aSource, dest, len);
          else {
              NS_WARNING("No case converter: only copying");
              memcpy((void*)aSource, (void*)dest, len * sizeof(*aSource));
          }
          mIter.advance(len);
          return len;
        }

    protected:
      nsAString::iterator& mIter;
  };

void
ToUpperCase( const nsAString& aSource, nsAString& aDest )
  {
    nsAString::const_iterator fromBegin, fromEnd;
    nsAString::iterator toBegin;
    aDest.SetLength(aSource.Length());
    CopyToUpperCase converter(aDest.BeginWriting(toBegin));
    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter);
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

