/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the mozilla.org LDAP XPCOM component.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "nsLDAPInternal.h"
#include "nsLDAPMessage.h"
#include "nspr.h"
#include "nsDebug.h"
#include "nsCRT.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLDAPMessage, nsILDAPMessage);

// constructor
//
nsLDAPMessage::nsLDAPMessage()
{
    NS_INIT_ISUPPORTS();

    mMsgHandle = NULL;
    mConnectionHandle = NULL;
}

// destructor
//
nsLDAPMessage::~nsLDAPMessage(void)
{
    int rc;

    if (mMsgHandle != NULL) {
        rc = ldap_msgfree(mMsgHandle);

        switch(rc) {
        case LDAP_RES_BIND:
        case LDAP_RES_SEARCH_ENTRY:
        case LDAP_RES_SEARCH_RESULT:
        case LDAP_RES_MODIFY:
        case LDAP_RES_ADD:
        case LDAP_RES_DELETE:
        case LDAP_RES_MODRDN:
        case LDAP_RES_COMPARE:
        case LDAP_RES_SEARCH_REFERENCE:
        case LDAP_RES_EXTENDED:
        case LDAP_RES_ANY:
            // success
            break;
        case LDAP_SUCCESS:
            // timed out (dunno why LDAP_SUCCESS is used to indicate this) 
            PR_LOG(gLDAPLogModule, PR_LOG_WARNING, 
                   ("nsLDAPMessage::~nsLDAPMessage: ldap_msgfree() "
                    "timed out\n"));
            break;
        default:
            // other failure
            PR_LOG(gLDAPLogModule, PR_LOG_WARNING, 
                   ("nsLDAPMessage::~nsLDAPMessage: ldap_msgfree() "
                    "failed: %s\n", ldap_err2string(rc)));
            break;
        }
    }

}

// associate this message with an existing operation
//
NS_IMETHODIMP
nsLDAPMessage::Init(nsILDAPConnection *aConnection, LDAPMessage *aMsgHandle)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(aConnection);
    NS_ENSURE_ARG_POINTER(aMsgHandle);
    
    // initialize the appropriate member vars
    //
    mConnection = aConnection;
    mMsgHandle = aMsgHandle;

    // cache the connection handle associated with this operation
    //
    rv = mConnection->GetConnectionHandle(&mConnectionHandle);
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

// XXX - both this and GetErrorString should be based on a separately
// broken out ldap_parse_result
//
NS_IMETHODIMP
nsLDAPMessage::GetErrorCode(PRInt32 *aErrCode)
{
    PRInt32 rc;

    rc = ldap_parse_result(mConnectionHandle, mMsgHandle, aErrCode, 
                           NULL, NULL, NULL, NULL, 0);

    if (rc != LDAP_SUCCESS) {

#ifdef DEBUG
        PR_fprintf(PR_STDERR,
                   "nsLDAPMessage::ErrorToString: ldap_parse_result: %s\n",
                   ldap_err2string(rc));
#endif  
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

// XXX deal with extra params (make client not have to use ldap_memfree() on 
// result)
// XXX better error-handling than fprintf()
//
char *
nsLDAPMessage::GetErrorString(void)
{
  int errcode;
  char *matcheddn;
  char *errmsg;
  char **referrals;
  LDAPControl **serverctrls;

  int rc;

  rc = ldap_parse_result(mConnectionHandle, mMsgHandle, &errcode, &matcheddn,
                         &errmsg, &referrals, &serverctrls, 0);
  if (rc != LDAP_SUCCESS) {
      PR_fprintf(PR_STDERR, 
                 "nsLDAPMessage::ErrorToString: ldap_parse_result: %s\n",
                 ldap_err2string(rc));
      // XXX need real err handling here
  }

  if (matcheddn) ldap_memfree(matcheddn);
  if (errmsg) ldap_memfree(errmsg);
  if (referrals) ldap_memfree(referrals);
  if (serverctrls) ldap_memfree(serverctrls);

  return(ldap_err2string(errcode));
}

// we don't get to use exceptions, so we'll fake it.  this is an error
// handler for IterateAttributes().  
//
nsresult
nsLDAPMessage::IterateAttrErrHandler(PRInt32 aLderrno, PRUint32 *aAttrCount, 
                                     char** *aAttributes, BerElement *position)
{

    // if necessary, free the position holder used by 
    // ldap_{first,next}_attribute()  
    //
    if (position != nsnull) {
        ldap_ber_free(position, 0);
    }

    // deallocate any entries in the array that have been allocated, then
    // the array itself
    //
    if (*aAttributes) {
        NSLDAP_FREE_XPIDL_ARRAY(*aAttrCount, *aAttributes, nsMemory::Free);
    }

    // possibly spit out a debugging message, then return an appropriate
    // error code
    //
    switch (aLderrno) {

    case LDAP_PARAM_ERROR:
        NS_WARNING("nsLDAPMessage::IterateAttributes() failure; probable bug "
                   "or memory corruption encountered");
        return NS_ERROR_UNEXPECTED;
        break;

    case LDAP_DECODING_ERROR:
	    NS_WARNING("nsLDAPMessage::IterateAttributes(): decoding error");
        return NS_ERROR_LDAP_DECODING_ERROR;
        break;

    case LDAP_NO_MEMORY:
        return NS_ERROR_OUT_OF_MEMORY;
        break;

    }

    NS_WARNING("nsLDAPMessage::IterateAttributes(): LDAP C SDK returned "
               "unexpected value; possible bug or memory corruption");
    return NS_ERROR_UNEXPECTED;
}


// wrapper for ldap_first_attribute 
//
NS_IMETHODIMP
nsLDAPMessage::GetAttributes(PRUint32 *aAttrCount, char** *aAttributes)
{
    return IterateAttributes(aAttrCount, aAttributes, PR_TRUE);
}

// if getP is PR_TRUE, we get the attributes by recursing once
// (without getP set) in order to fill in *attrCount, then allocate
// and fill in the *aAttributes.  
// 
// if getP is PR_FALSE, just fill in *attrCount and return
// 
nsresult
nsLDAPMessage::IterateAttributes(PRUint32 *aAttrCount, char** *aAttributes, 
				 PRBool getP)
{
    BerElement *position;
    nsresult rv;

    if (aAttrCount == nsnull || aAttributes == nsnull ) {
        return NS_ERROR_INVALID_POINTER;
    }

    // if we've been called from GetAttributes, recurse once in order to
    // count the elements in this message.
    //
    if (getP) {
        *aAttributes = nsnull;
        *aAttrCount = 0;

        rv = IterateAttributes(aAttrCount, aAttributes, PR_FALSE);
        if (NS_FAILED(rv))
            return rv;

        // create an array of the appropriate size
        //
        *aAttributes = NS_STATIC_CAST(char **, 
                                      nsMemory::Alloc(*aAttrCount *
                                                      sizeof(char *)));
        if (!*aAttributes) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    } 

    // get the first attribute
    //
    char *attr = ldap_first_attribute(mConnectionHandle, mMsgHandle, 
                                      &position);
    if (attr == nsnull) {
        return IterateAttrErrHandler(ldap_get_lderrno(mConnectionHandle, 
                                                      NULL, NULL),
                                     aAttrCount, aAttributes, position);
    }

    // if we're getting attributes, try and fill in the first field
    //
    if (getP) {
        (*aAttributes)[0] = nsCRT::strdup(attr);
        if (!(*aAttributes)[0]) {
            ldap_memfree(attr);
            nsMemory::Free(*aAttributes);
            return NS_ERROR_OUT_OF_MEMORY;
        }

        // note that we start counting again, in order to keep our place in 
        // the array so that we can unwind gracefully and avoid leakage if
        // we hit an error as we're filling in the array
        //
        *aAttrCount = 1;
    } else {

        // otherwise just update the count
        //
        *aAttrCount = 1;
    }
    ldap_memfree(attr);

    while (1) {
	
        // get the next attribute
        //
        attr = ldap_next_attribute(mConnectionHandle, mMsgHandle, position);

        // check to see if there is an error, or if we're just done iterating
        //
        if (attr == nsnull) {
            
            // bail out if there's an error
            //
            PRInt32 lderrno = ldap_get_lderrno(mConnectionHandle, NULL, NULL);
            if (lderrno != LDAP_SUCCESS) {
                return IterateAttrErrHandler(lderrno, aAttrCount, aAttributes, 
                                             position);
            }

            // otherwise, there are no more attributes; we're done with
            // the while loop
            //
            break;

        } else if (getP) {

            // if ldap_next_attribute did return successfully, and 
            // we're supposed to fill in a value, do so.
            //
            (*aAttributes)[*aAttrCount] = nsCRT::strdup(attr);
            if (!(*aAttributes)[*aAttrCount]) {
                ldap_memfree(attr);
                return IterateAttrErrHandler(LDAP_NO_MEMORY, aAttrCount, 
                                             aAttributes, position);
            }
       
        }
        ldap_memfree(attr);

        // we're done using *aAttrCount as a c-style array index (ie starting
        // at 0).  update it to reflect the number of elements now in the array
        //
        *aAttrCount += 1;
    }

    // free the position pointer, if necessary
    //
    if (position != nsnull) {
        ldap_ber_free(position, 0);
    }

    return NS_OK;
}

// wrapper for ldap_msgtype()
//
int
nsLDAPMessage::Type(void)
{
    return (ldap_msgtype(mMsgHandle));
}

/*
 * Wrapper for ldap_get_dn().  Returns the Distinguished Name of the
 * entry associated with this message.
 * 
 * @exception NS_ERROR_OUT_OF_MEMORY            ran out of memory
 * @exception NS_ERROR_ILLEGAL_VALUE            null pointer passed in
 * @exception NS_ERROR_LDAP_DECODING_ERROR      problem during BER-decoding
 * @exception NS_ERROR_UNEXPECTED               bug or memory corruption hit
 *
 *   readonly attribute string dn;
 */
NS_IMETHODIMP
nsLDAPMessage::GetDn(char* *aDN)
{
    if (aDN == nsnull) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    char *dn = ldap_get_dn(mConnectionHandle, mMsgHandle);
    
    if (!dn) {
         PRInt32 lderrno = ldap_get_lderrno(mConnectionHandle, NULL, NULL);

         switch (lderrno) {

         case LDAP_DECODING_ERROR:
             NS_WARNING("nsLDAPMessage::GetDn(): ldap decoding error");
             return NS_ERROR_LDAP_DECODING_ERROR;
             break;

         case LDAP_PARAM_ERROR:
         default:
             NS_ERROR("nsLDAPMessage::GetDn(): internal error");
             return NS_ERROR_UNEXPECTED;
             break;
         }
    } 

    // get a copy made with the shared allocator, and dispose of the original
    //
    *aDN = nsCRT::strdup(dn); 
    ldap_memfree(dn);

    if (!*aDN) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

// wrapper for ldap_get_values()
//
NS_IMETHODIMP
nsLDAPMessage::GetValues(const char *aAttr, PRUint32 *aCount, 
                         char** *aValues)
{
    char **values;

    values = ldap_get_values(mConnectionHandle, mMsgHandle, aAttr);

    // bail out if there was a problem
    //
    if (!values) {
        PRInt32 lderrno = ldap_get_lderrno(mConnectionHandle, NULL, NULL);

        if ( lderrno == LDAP_DECODING_ERROR ) {
            NS_WARNING("nsLDAPMessage::GetValues(): Error decoding values");
            return NS_ERROR_LDAP_DECODING_ERROR;

        } else if ( lderrno == LDAP_PARAM_ERROR ) {
            NS_ERROR("nsLDAPMessage::GetValues(): internal error: 1");
            return NS_ERROR_UNEXPECTED;

        } else {
            NS_ERROR("nsLDAPMessage::GetValues(): internal error: 2");
            return NS_ERROR_UNEXPECTED;
        }
    }

    // count the values
    //
    PRUint32 numVals = ldap_count_values(values);

    // create an array of the appropriate size
    //
    *aValues = NS_STATIC_CAST(char **, 
                              nsMemory::Alloc(numVals * sizeof(char *)));
    if (!*aValues) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // clone the array (except for the trailing NULL entry) using the 
    // shared allocator for XPCOM correctness
    //
    PRUint32 i;
    for ( i = 0 ; i < numVals ; i++ ) {
        (*aValues)[i] = nsCRT::strdup(values[i]);
        if ((*aValues)[i] == nsnull ) {
            NSLDAP_FREE_XPIDL_ARRAY(i, aValues, nsMemory::Free);
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    *aCount = numVals;
    return NS_OK;
}

// returns an LDIF-like string representation of this message
//
// string toString();
//
NS_IMETHODIMP
nsLDAPMessage::ToString(char* *aString)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
