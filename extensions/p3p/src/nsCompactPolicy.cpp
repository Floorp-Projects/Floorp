/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Platform for Privacy Preferences.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Harish Dhurvasula <harishd@netscape.com>
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

#include "nsP3PService.h"
#include "nsCompactPolicy.h"
#include "nsNetCID.h"
#include "nsReadableUtils.h"                             

// define an array of all compact policy tags
#define CP_TOKEN(_token) #_token,
static const char* kTokens[] = {
#include "nsTokenList.h"
};
#undef CP_TOKEN

static nsHashtable* gTokenTable;

static 
Tokens Lookup(const char* aTag)
{
  Tokens rv = eToken_NULL;
  
  nsCStringKey key(aTag, -1, nsCStringKey::NEVER_OWN);

  void* val = (gTokenTable)? gTokenTable->Get(&key):0;
  if (val) {
    rv = Tokens(NS_PTR_TO_INT32(val));
  }

  return rv;
}

static 
PRBool FindCompactPolicy(nsACString::const_iterator& aStart,
                         nsACString::const_iterator& aEnd) {
  PRBool found = PR_FALSE;
  nsACString::const_iterator tmp = aEnd;

  if (CaseInsensitiveFindInReadable(NS_LITERAL_CSTRING("CP"), aStart, tmp)) {
    while (*tmp == ' ' && ++tmp != aEnd); // skip spaces
    
    if (tmp != aEnd && *tmp == '=') {

      while (++tmp != aEnd && *tmp == ' '); // skip spaces
      
      if (tmp != aEnd) {
        aStart = tmp; // Compact policy starts
        found = PR_TRUE;
      }
    }
  }

  return found;
}

static const
PRInt32 MapTokenToConsent(const nsACString::const_iterator& aStart,
                          const nsACString::const_iterator& aEnd) 
{
  PRInt32 rv = -1;

  if (aStart != aEnd) {
    PRInt32 len = Distance(aStart,aEnd);
    if (len > 2 && len < 5) {
      char  token[5] = {0};
      const char* begin = aStart.get();
      
      memcpy(token,begin,3);
     
      eTokens id = Lookup(token);
      switch (id) {
        case eToken_TST:
        case eToken_NULL: // Token not in CP vocabulary!
          rv = NS_INVALID_POLICY;
          break;
        // Tokens that indicate collection of
        // Personally Identifiable Information
        // without consent.
        case eToken_FIN: 
        case eToken_GOV:
        case eToken_ONL:
        case eToken_PHY:
        case eToken_UNI:
          rv = NS_PII_TOKEN;
          break;
        // Tokens that indicate collection of
        // Personally Identifiable Information 
        // with opt-in, opt-out options.
        case eToken_CON:
        case eToken_DEL:
        case eToken_IVA:
        case eToken_IVD:
        case eToken_OTP:
        case eToken_OTR:
        case eToken_PUB: 
        case eToken_SAM: 
        case eToken_TEL: 
        case eToken_UNR:
          {
            char attr = (len > 3)? *(begin+3):'\0';
            rv = NS_NO_CONSENT; // Tokens with or without attribute 'a'
            if (attr == 'o') {
              rv = NS_IMPLICIT_CONSENT;
            }
            else if (attr == 'i') {
              rv = NS_EXPLICIT_CONSENT;
            }
          }
          break;
        default:
          rv = NS_NON_PII_TOKEN;
          break;
      }
    }
    else {
      rv = NS_INVALID_POLICY;
    }
  }
  return rv;
}

static const
PRInt32 ParsePolicy(nsACString::const_iterator& aStart,
                    nsACString::const_iterator& aEnd)
{
  PRInt32 rv = NS_HAS_POLICY;
  
  if (aStart != aEnd) {
    // strip off the begining quote or apostrophe
    const char quoteChar = *aStart;
    if ((quoteChar == '"' || quoteChar == '\'') && ++aStart == aEnd) {
      return NS_NO_POLICY ;
    }

    nsACString::const_iterator tokenStart = aStart;
    while (aStart != aEnd) {
      if (*aStart != ' ' && *aStart != quoteChar) { 
        ++aStart;
      }
      else {
        PRInt32 consent = MapTokenToConsent(tokenStart,aStart);

        if (consent == -1) {
          if (!(rv & (NS_PII_TOKEN  |
                      NS_NO_CONSENT | 
                      NS_IMPLICIT_CONSENT | 
                      NS_EXPLICIT_CONSENT |  
                      NS_NON_PII_TOKEN))) {
            rv = NS_NO_POLICY;
          }
          break;
        }
        else if (consent == NS_INVALID_POLICY) {
          rv = NS_INVALID_POLICY;
          break;
        }
        else {
          rv |= consent;
          if (consent & NS_PII_TOKEN) {
            if (rv & NS_NO_CONSENT) {
              // PII is collected without consent.
              // No need to parse CP any further.
              break;
            }
          }
          else if (consent & NS_NO_CONSENT) {
            rv &= ~(NS_IMPLICIT_CONSENT | NS_EXPLICIT_CONSENT);
            if (rv & NS_PII_TOKEN) {
              // PII is collected without consent.
              // No need to parse CP any further.
              break;
            }
          }
          else if (consent & NS_IMPLICIT_CONSENT) {
            rv &= ~NS_EXPLICIT_CONSENT;
            if (rv & NS_NO_CONSENT) {
              rv &= ~NS_IMPLICIT_CONSENT;
            }
          }
          else if (consent & NS_EXPLICIT_CONSENT) {
            if (rv & (NS_NO_CONSENT | NS_IMPLICIT_CONSENT)) {
              rv &= ~NS_EXPLICIT_CONSENT;
            }
          }
          
          if (*aStart == quoteChar) {
            break; // done parsing CP
          }
         
          while (++aStart != aEnd && *aStart == ' '); // skip spaces
          tokenStart = aStart;
        }
      }
    }

    if (rv & NS_PII_TOKEN) {
      if (!(rv & (NS_NO_CONSENT | NS_IMPLICIT_CONSENT | NS_EXPLICIT_CONSENT))) {
        // CP did not contain any information about opt-in / opt-out
        rv = NS_NO_CONSENT; 
      }
    }
    else {
      // If a PII token is not present, in the CP, we can be sure
      // that no personally identifiable information is collected.
      if (rv & (NS_NO_CONSENT | NS_IMPLICIT_CONSENT | NS_EXPLICIT_CONSENT)) {
        rv = NS_NON_PII_TOKEN; // PII not collected.
      }
    }
  }
   
  NS_ASSERTION(rv > NS_HAS_POLICY,"compact policy error");
  return rv;
}

/*************************************
* nsCompactPolicy Implementation *
*************************************/

nsCompactPolicy::nsCompactPolicy( ) {
}

nsCompactPolicy::~nsCompactPolicy( ) {
}

nsresult
nsCompactPolicy::InitTokenTable(void) 
{
  gTokenTable = new nsHashtable();
  NS_ENSURE_TRUE(gTokenTable, NS_ERROR_OUT_OF_MEMORY);

  for (PRInt32 i = 0; i < NS_CP_TOKEN_MAX; i++) {
    nsCStringKey key(kTokens[i], -1, nsCStringKey::NEVER_OWN);
    gTokenTable->Put(&key, (void*)(i+1));
  }
  return NS_OK;
}

void
nsCompactPolicy::DestroyTokenTable(void) 
{
  delete gTokenTable;
}

nsresult
nsCompactPolicy::OnHeaderAvailable(const char* aP3PHeader,
                                   const char* aSpec)
{
  NS_ENSURE_ARG_POINTER(aP3PHeader);
  NS_ENSURE_ARG_POINTER(aSpec);

  nsresult result = NS_OK;
    
  nsDependentCString header(aP3PHeader);
  nsACString::const_iterator begin, end;

  header.BeginReading(begin);
  header.EndReading(end);

  if (FindCompactPolicy(begin,end)) {
    nsCStringKey key(aSpec);
    if (!mPolicyTable.Exists(&key)) {
      PRInt32 consent =  ParsePolicy(begin,end);
      mPolicyTable.Put(&key, NS_INT32_TO_PTR(consent));
    }
  }
  
  return result;
}

nsresult
nsCompactPolicy::GetConsent(const char* aURI,
                            PRInt32& aConsent)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsresult result = NS_OK;

  nsCStringKey key(aURI, -1, nsCStringKey::NEVER_OWN);
  if (mPolicyTable.Exists(&key)) {
    aConsent = NS_PTR_TO_INT32(mPolicyTable.Get(&key));
  }
  
  return result;
}

