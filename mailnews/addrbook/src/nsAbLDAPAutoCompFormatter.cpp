/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mosedale <dmose@netscape.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// Work around lack of conditional build logic in codewarrior's
// build system.  The MOZ_LDAP_XPCOM preprocessor symbol is only 
// defined on Mac because noone else needs this weirdness; thus 
// the XP_MAC check first.  This conditional encloses the entire
// file, so that in the case where the ldap option is turned off
// in the mac build, a dummy (empty) object will be generated.
//
#if !defined(XP_MAC) || defined(MOZ_LDAP_XPCOM)

#include "nsAbLDAPAutoCompFormatter.h"
#include "nsIAutoCompleteResults.h"
#include "nsIServiceManager.h"
#include "nsIMsgHeaderParser.h"
#include "nsILDAPMessage.h"
#include "nsLDAP.h"
#include "nsReadableUtils.h"
#include "nsIStringBundle.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIDNSService.h"
#include "nsNetError.h"

NS_IMPL_ISUPPORTS2(nsAbLDAPAutoCompFormatter, 
		   nsILDAPAutoCompFormatter, 
		   nsIAbLDAPAutoCompFormatter)

nsAbLDAPAutoCompFormatter::nsAbLDAPAutoCompFormatter() :
    mNameFormat(NS_LITERAL_STRING("[cn]")),
    mAddressFormat(NS_LITERAL_STRING("{mail}"))
{
}

nsAbLDAPAutoCompFormatter::~nsAbLDAPAutoCompFormatter()
{
}

NS_IMETHODIMP
nsAbLDAPAutoCompFormatter::Format(nsILDAPMessage *aMsg, 
                                  nsIAutoCompleteItem **aItem) 
{
    nsresult rv;

    nsCOMPtr<nsIMsgHeaderParser> msgHdrParser = 
        do_GetService("@mozilla.org/messenger/headerparser;1", &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsAbLDAPAutoCompFormatter::Format(): do_GetService()"
                 " failed trying to obtain"
                 " '@mozilla.org/messenger/headerparser;1'");
        return NS_ERROR_NOT_AVAILABLE;
    }

    // generate the appropriate string
    //
    nsCAutoString name;
    rv = ProcessFormat(mNameFormat, aMsg, &name, 0);
    if (NS_FAILED(rv)) {
        // Something went wrong lower down the stack; a message should
        // have already been logged there.  Return an error, rather
        // than trying to generate a bogus nsIAutoCompleteItem
        //
        return rv;
    }

    nsCAutoString address;
    rv = ProcessFormat(mAddressFormat, aMsg, &address, 0);
    if (NS_FAILED(rv)) {
        // Something went wrong lower down the stack; a message should have 
        // already been logged there.  Return an error, rather than trying to 
        // generate a bogus nsIAutoCompleteItem.
        //
        return rv;
    }

    nsXPIDLCString value;
    /* As far as I can tell, the documentation in nsIMsgHdrParser that 
     * nsnull means "US-ASCII" is actually wrong, it appears to mean UTF8
     */
    rv = msgHdrParser->MakeFullAddress(nsnull, name.get(), address.get(), 
                                       getter_Copies(value));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsAbLDAPAutoCompFormatter::Format(): call to"
                 " MakeFullAddress() failed");
        return rv;
    }

    // create an nsIAutoCompleteItem to hold the returned value
    //
    nsCOMPtr<nsIAutoCompleteItem> item = do_CreateInstance(
        NS_AUTOCOMPLETEITEM_CONTRACTID, &rv);

    if (NS_FAILED(rv)) {
        NS_ERROR("nsAbLDAPAutoCompFormatter::Format(): couldn't"
                 " create " NS_AUTOCOMPLETEITEM_CONTRACTID "\n");
        return NS_ERROR_NOT_AVAILABLE;
    }

    // this is that part that actually gets autocompleted to
    //
    rv = item->SetValue(NS_ConvertUTF8toUCS2(value));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsAbLDAPAutoCompFormatter::Format(): "
                 "item->SetValue failed");
        return rv;
    }

    // generate the appropriate string to appear as a comment off to the side
    //
    nsCAutoString comment;
    rv = ProcessFormat(mCommentFormat, aMsg, &comment, 0);
    if (NS_SUCCEEDED(rv)) {
        rv = item->SetComment(NS_ConvertUTF8toUCS2(comment).get());
        if (NS_FAILED(rv)) {
            NS_WARNING("nsAbLDAPAutoCompFormatter::Format():"
                       " item->SetComment() failed");
        }
    }

    rv = item->SetClassName("remote-abook");
    if (NS_FAILED(rv)) {
        NS_WARNING("nsAbLDAPAutoCompleteFormatter::Format():"
                   " item->SetClassName() failed");
    }

    // all done; return the item
    NS_IF_ADDREF(*aItem = item);
    return NS_OK;
}

NS_IMETHODIMP
nsAbLDAPAutoCompFormatter::FormatException(PRInt32 aState, 
                                           nsresult aErrorCode, 
                                           nsIAutoCompleteItem **aItem) 
{
    PRInt32 errorKey;
    nsresult rv;

    // create an nsIAutoCompleteItem to hold the returned value
    //
    nsCOMPtr<nsIAutoCompleteItem> item = do_CreateInstance(
        NS_AUTOCOMPLETEITEM_CONTRACTID, &rv);

    if (NS_FAILED(rv)) {
        NS_ERROR("nsAbLDAPAutoCompFormatter::FormatException(): couldn't"
                 " create " NS_AUTOCOMPLETEITEM_CONTRACTID "\n");
        return NS_ERROR_NOT_AVAILABLE;
    }

    // get the string bundle service
    //
    nsXPIDLString errMsg, ldapErrMsg, errCode, alertMsg, ldapHint;
    nsString errCodeNum;

    nsCOMPtr<nsIStringBundleService> stringBundleSvc(do_GetService(
                                            NS_STRINGBUNDLE_CONTRACTID, &rv)); 
    if (NS_FAILED(rv)) {
        NS_ERROR("nsAbLDAPAutoCompleteFormatter::FormatException():"
                 " error getting string bundle service");
        return rv;
    }

    // get the string bundles relevant here: the main LDAP bundle,
    // and the ldap AutoCompletion-specific bundle
    //
    nsCOMPtr<nsIStringBundle> ldapBundle, ldapACBundle;

    rv = stringBundleSvc->CreateBundle(
        "chrome://mozldap/locale/ldap.properties",
        getter_AddRefs(ldapBundle));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsAbLDAPAutoCompleteFormatter::FormatException():"
                 " error creating string bundle"
                 " chrome://mozldap/locale/ldap.properties");
        return rv;
    } 

    rv = stringBundleSvc->CreateBundle(
        "chrome://global/locale/ldapAutoCompErrs.properties",
        getter_AddRefs(ldapACBundle));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsAbLDAPAutoCompleteFormatter::FormatException():"
                 " error creating string bundle"
                 " chrome://global/locale/ldapAutoCompErrs.properties");
        return rv;
    }

    // get the general error that goes in the dropdown and the window
    // title
    //
    rv = ldapACBundle->GetStringFromID(aState, getter_Copies(errMsg));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsAbLDAPAutoCompleteFormatter::FormatException():"
                 " error getting general error from bundle"
                 " chrome://global/locale/ldapAutoCompErrs.properties");
        return rv;
    }

    // get the phrase corresponding to "Error code"
    //
    rv = ldapACBundle->GetStringFromName(NS_LITERAL_STRING("errCode").get(), 
                                         getter_Copies(errCode));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsAbLDAPAutoCompleteFormatter::FormatException"
                   "(): error getting 'errCode' string from bundle "
                   "chrome://mozldap/locale/ldap.properties");
        return rv;
    }

    // for LDAP errors
    //
    if (NS_ERROR_GET_MODULE(aErrorCode) == NS_ERROR_MODULE_LDAP) {

        errorKey = NS_ERROR_GET_CODE(aErrorCode);

        // put the number itself in string form
        //
        errCodeNum.AppendInt(errorKey);

        // get the LDAP error message itself
        //
        rv = ldapBundle->GetStringFromID(NS_ERROR_GET_CODE(aErrorCode), 
                                                getter_Copies(ldapErrMsg));
        if (NS_FAILED(rv)) {
            NS_ERROR("nsAbLDAPAutoCompleteFormatter::FormatException"
                     "(): error getting string 2 from bundle "
                     "chrome://mozldap/locale/ldap.properties");
            return rv;
        }
  
    } else {

        // put the entire nsresult in string form
        //
        errCodeNum.AppendLiteral("0x");
        errCodeNum.AppendInt(aErrorCode, 16);    

        // figure out the key to index into the string bundle
        //
        const PRInt32 HOST_NOT_FOUND_ERROR=5000;
        const PRInt32 GENERIC_ERROR=9999;
        errorKey = ( (aErrorCode == NS_ERROR_UNKNOWN_HOST) ? 
                     HOST_NOT_FOUND_ERROR : GENERIC_ERROR );

        // get the specific error message itself
        rv = ldapACBundle->GetStringFromID(errorKey,
                                           getter_Copies(ldapErrMsg));
        if (NS_FAILED(rv)) {
            NS_ERROR("nsAbLDAPAutoCompleteFormatter::FormatException"
                     "(): error getting specific non LDAP error-string "
                     "from bundle "
                     "chrome://mozldap/locale/ldap.properties");
            return rv;
        }
    }

    // and try to find a hint about what the user should do next
    //
    const PRInt32 HINT_BASE=10000;
    const PRInt32 GENERIC_HINT_CODE = 9999;
    rv = ldapACBundle->GetStringFromID(HINT_BASE + errorKey, 
                                       getter_Copies(ldapHint));
    if (NS_FAILED(rv)) {
        rv = ldapACBundle->GetStringFromID(HINT_BASE + GENERIC_HINT_CODE,
                                           getter_Copies(ldapHint));
        if (NS_FAILED(rv)) {
            NS_ERROR("nsAbLDAPAutoCompleteFormatter::FormatException()"
                     "(): error getting hint string from bundle "
                     "chrome://mozldap/locale/ldap.properties");
            return rv;
        }
    }
        
    const PRUnichar *stringParams[4] = { errCode.get(), errCodeNum.get(), 
                                         ldapErrMsg.get(), ldapHint.get() };

    rv = ldapACBundle->FormatStringFromName(
        NS_LITERAL_STRING("alertFormat").get(), stringParams, 4, 
        getter_Copies(alertMsg));
    if (NS_FAILED(rv)) {
        NS_WARNING("YYY");
    }

    // put the error message, between angle brackets, into the XPIDL |value|
    // attribute.  Note that the hardcoded string is only used since 
    // stringbundles have already failed us.
    //
    if (errMsg.Length()) {
        rv = item->SetValue(PromiseFlatString(NS_LITERAL_STRING("<") + errMsg
                                              + NS_LITERAL_STRING(">")));
    } else {
        rv = item->SetValue(
            NS_LITERAL_STRING("<Unknown LDAP autocompletion error>"));
    }

    if (NS_FAILED(rv)) {
        NS_ERROR("nsAbLDAPAutoCompFormatter::FormatException(): "
                 "item->SetValue failed");
        return rv;
    }
    
    // pass the alert message in as the param; if that fails, proceed anyway
    //
    nsCOMPtr<nsISupportsString> alert(do_CreateInstance(
                                           NS_SUPPORTS_STRING_CONTRACTID,
                                           &rv));
    if (NS_FAILED(rv)) {
        NS_WARNING("nsAbLDAPAutoCompFormatter::FormatException(): "
                   "could not create nsISupportsString");
    } else {
        rv = alert->SetData(alertMsg);
        if (NS_FAILED(rv)) {
            NS_WARNING("nsAbLDAPAutoCompFormatter::FormatException(): "
                     "alert.Set() failed");
        } else {
            rv = item->SetParam(alert);
            if (NS_FAILED(rv)) {
                NS_WARNING("nsAbLDAPAutoCompFormatter::FormatException(): "
                           "item->SetParam failed");
            }
        }
    }

    // this is a remote addressbook, set the class name so the autocomplete 
    // item can be styled to show this
    //
    rv = item->SetClassName("remote-err");
    if (NS_FAILED(rv)) {
        NS_WARNING("nsAbLDAPAutoCompleteFormatter::FormatException():"
                   " item->SetClassName() failed");
    }

    // all done; return the item
    //
    NS_IF_ADDREF(*aItem = item);
    return NS_OK;
}

NS_IMETHODIMP
nsAbLDAPAutoCompFormatter::GetAttributes(PRUint32 *aCount, char ** *aAttrs)
{
    if (!aCount || !aAttrs) {
        return NS_ERROR_INVALID_POINTER;
    }

    nsCStringArray mSearchAttrs;
    nsresult rv = ProcessFormat(mNameFormat, 0, 0, &mSearchAttrs);
    if (NS_FAILED(rv)) {
        NS_WARNING("nsAbLDAPAutoCompFormatter::SetNameFormat(): "
                   "ProcessFormat() failed");
        return rv;
    }
    rv = ProcessFormat(mAddressFormat, 0, 0, &mSearchAttrs);
    if (NS_FAILED(rv)) {
        NS_WARNING("nsAbLDAPAutoCompFormatter::SetNameFormat(): "
                   "ProcessFormat() failed");
        return rv;
    }
    rv = ProcessFormat(mCommentFormat, 0, 0, &mSearchAttrs);
    if (NS_FAILED(rv)) {
        NS_WARNING("nsAbLDAPAutoCompFormatter::SetNameFormat(): "
                   "ProcessFormat() failed");
        return rv;
    }

    // none of the formatting templates require any LDAP attributes
    //
    PRUint32 count = mSearchAttrs.Count();   // size of XPCOM array we'll need
    if (!count) {
        NS_ERROR("nsAbLDAPAutoCompFormatter::GetAttributes(): "
                 "current output format (if set) requires no search "
                 "attributes");
        return NS_ERROR_NOT_INITIALIZED;
    }

    // build up the raw XPCOM array to return
    //
    PRUint32 rawSearchAttrsSize = 0;        // grown as XPCOM array is built
    char **rawSearchAttrs = 
        NS_STATIC_CAST(char **, nsMemory::Alloc(count * sizeof(char *)));
    if (!rawSearchAttrs) {
        NS_ERROR("nsAbLDAPAutoCompFormatter::GetAttributes(): out of "
		 "memory");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Loop through the string array, and build up the C-array.
    //
    while (rawSearchAttrsSize < count) {
        if (!(rawSearchAttrs[rawSearchAttrsSize] = 
              ToNewCString(*(mSearchAttrs.CStringAt(rawSearchAttrsSize))))) {
            NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(rawSearchAttrsSize, 
                                                  rawSearchAttrs);
            NS_ERROR("nsAbLDAPAutoCompFormatter::GetAttributes(): out "
		     "of memory");
            return NS_ERROR_OUT_OF_MEMORY;
        }
        rawSearchAttrsSize++;
    }

    *aCount = rawSearchAttrsSize;
    *aAttrs = rawSearchAttrs;

    return NS_OK;

}
// parse and process a formatting attribute.  If aStringArray is
// non-NULL, return a list of the attributes from mNameFormat in
// aStringArray.  Otherwise, generate an autocomplete value from the
// information in aMessage and append it to aValue.  Any errors
// (including failure to find a required attribute while building up aValue) 
// return an NS_ERROR_* up the stack so that the caller doesn't try and
// generate an nsIAutoCompleteItem from this.
//
nsresult
nsAbLDAPAutoCompFormatter::ProcessFormat(const nsAString & aFormat,
                                         nsILDAPMessage *aMessage, 
                                         nsACString *aValue,
                                         nsCStringArray *aAttrs)
{
    nsresult rv;    // temp for return values

    // get some iterators to parse aFormat
    //
    nsReadingIterator<PRUnichar> iter, iterEnd;
    aFormat.BeginReading(iter);
    aFormat.EndReading(iterEnd);

    // get the console service for error logging
    //
    nsCOMPtr<nsIConsoleService> consoleSvc = 
        do_GetService("@mozilla.org/consoleservice;1", &rv);
    if (NS_FAILED(rv)) {
        NS_WARNING("nsAbLDAPAutoCompFormatter::ProcessFormat(): "
                   "couldn't get console service");
    }

    PRBool attrRequired = PR_FALSE;     // is this attr required or optional?
    nsCAutoString attrName;             // current attr to get

    // parse until we hit the end of the string
    //
    while (iter != iterEnd) {

        switch (*iter) {            // process the next char

        case PRUnichar('{'):

            attrRequired = PR_TRUE;  // this attribute is required

            /*FALLTHROUGH*/

        case PRUnichar('['):

            rv = ParseAttrName(iter, iterEnd, attrRequired, consoleSvc, 
                               attrName);
            if ( NS_FAILED(rv) ) {

                // something unrecoverable happened; stop parsing and 
                // propagate the error up the stack
                //
                return rv;
            }

            // if we're building an array
            if ( aAttrs ) { 

                // and it doesn't already contain this string
                if (aAttrs->IndexOfIgnoreCase(attrName) == -1) { 

                    // add it
                    if (!aAttrs->AppendCString(attrName)) {
                        
                        // current AppendCString always returns PR_TRUE;
                        // if we hit this error, something has changed in
                        // that code
                        //
                        NS_ERROR(
                            "nsAbLDAPAutoCompFormatter::ProcessFormat():"
                            " aAttrs->AppendCString(attrName) failed");
                        return NS_ERROR_UNEXPECTED;
                    }
                }
            } else {

                // otherwise, append the first value of this attr to aValue
                // XXXdmose should do better than this; bug 76595

                rv = AppendFirstAttrValue(attrName, aMessage, attrRequired, 
                                          *aValue);
                if ( NS_FAILED(rv) ) {

                    // something unrecoverable happened; stop parsing and 
                    // propagate the error up the stack
                    //
                    return rv;
                }
            }

            attrName.Truncate();     // clear out for next pass
            attrRequired = PR_FALSE; // reset to the default for the next pass

            break;

        case PRUnichar('\\'):

            // advance the iterator and be sure we haven't run off the end
            //
            ++iter;
            if (iter == iterEnd) {

                // abort; missing escaped char
                //
                if (consoleSvc) {
                    consoleSvc->LogStringMessage(
                        NS_LITERAL_STRING(
                            "LDAP addressbook autocomplete formatter: error parsing format string: premature end of string after \\ escape").get());

                    NS_ERROR("LDAP addressbook autocomplete formatter: error "
                             "parsing format string: premature end of string "
                             "after \\ escape");
                }

                return NS_ERROR_ILLEGAL_VALUE;
            }

            /*FALLTHROUGH*/

        default:
            
            // if we're not just building an array of attribute names, append
            // this character to the item we're generating.
            //
            if (!aAttrs) {

                // this character gets treated as a literal
                //
                (*aValue).Append(NS_ConvertUCS2toUTF8(nsDependentString(iter.get(), 1))); //XXXjag poke me about string generators
            }
        }

        ++iter; // advance the iterator
    }

    return NS_OK;
}

nsresult 
nsAbLDAPAutoCompFormatter::ParseAttrName(
    nsReadingIterator<PRUnichar> &aIter,        // iterators for mOutputString
    nsReadingIterator<PRUnichar> &aIterEnd, 
    PRBool aAttrRequired,                       // required?  or just optional?
    nsCOMPtr<nsIConsoleService> &aConsoleSvc,   // no need to reacquire this
    nsACString &aAttrName)                      // attribute token
{
    // reset attrname, and move past the opening brace
    //
    ++aIter;

    // get the rest of the attribute name
    //
    do {

        // be sure we haven't run off the end
        //
        if (aIter == aIterEnd) {

            // abort; missing closing delimiter
            //
            if (aConsoleSvc) {
                aConsoleSvc->LogStringMessage(
                    NS_LITERAL_STRING(
                        "LDAP address book autocomplete formatter: error parsing format string: missing } or ]").get());

                NS_ERROR("LDAP address book autocomplete formatter: error "
                         "parsing format string: missing } or ]");
            }

            return NS_ERROR_ILLEGAL_VALUE;

        } else if ( (aAttrRequired && *aIter == PRUnichar('}')) || 
                    (!aAttrRequired && *aIter == PRUnichar(']')) ) {

            // done with this attribute
            //
            break;

        } else {

            // this must be part of the attribute name
            //
            aAttrName.Append(NS_STATIC_CAST(char,*aIter));
        }

        ++aIter;

    } while (1);

    return NS_OK;
}

nsresult
nsAbLDAPAutoCompFormatter::AppendFirstAttrValue(
    const nsACString &aAttrName, // attr to get
    nsILDAPMessage *aMessage, // msg to get values from
    PRBool aAttrRequired, // is this a required value?
    nsACString &aValue)
{
    // get the attribute values for the field which will be used 
    // to fill in nsIAutoCompleteItem::value
    //
    PRUint32 numVals;
    PRUnichar **values;

    nsresult rv;
    rv = aMessage->GetValues(PromiseFlatCString(aAttrName).get(), &numVals, 
                             &values);
    if (NS_FAILED(rv)) {

        switch (rv) {
        case NS_ERROR_LDAP_DECODING_ERROR:
            // this may not be an error, per se; it could just be that the 
            // requested attribute does not exist in this particular message,
            // either because we didn't request it with the search operation,
            // or because it doesn't exist on the server.
            //
            break;

        case NS_ERROR_OUT_OF_MEMORY:
        case NS_ERROR_UNEXPECTED:
            break;

        default:
            NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchEntry(): "
                     "unexpected return code from aMessage->getValues()");
            rv = NS_ERROR_UNEXPECTED;
            break;
        }

        // if this was a required attribute, don't append anything to aValue
        // and return the error code
        //
        if (aAttrRequired) {
            return rv;
        } else {
            // otherwise forget about this attribute, but return NS_OK, which
            // will cause our caller to continue processing nameFormat in 
            // order to generate an nsIAutoCompleteItem.
            //
            return NS_OK;
        }
    }

    // append the value to our string; then free the array of results
    //
    aValue.Append(NS_ConvertUCS2toUTF8(values[0]));
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(numVals, values);

    // if this attribute wasn't required, we fall through to here, and return 
    // ok
    //
    return NS_OK;
}

// attribute AString nameFormat;
NS_IMETHODIMP 
nsAbLDAPAutoCompFormatter::GetNameFormat(nsAString & aNameFormat)
{
    aNameFormat  = mNameFormat;
    return NS_OK;
}
NS_IMETHODIMP 
nsAbLDAPAutoCompFormatter::SetNameFormat(const nsAString & aNameFormat)
{
    mNameFormat = aNameFormat; 
    return NS_OK;
}

// attribute AString addressFormat;
NS_IMETHODIMP 
nsAbLDAPAutoCompFormatter::GetAddressFormat(nsAString & aAddressFormat)
{
    aAddressFormat  = mAddressFormat;
    return NS_OK;
}
NS_IMETHODIMP 
nsAbLDAPAutoCompFormatter::SetAddressFormat(const nsAString & 
                                            aAddressFormat)
{
    mAddressFormat = aAddressFormat; 
    return NS_OK;
}

// attribute AString commentFormat;
NS_IMETHODIMP 
nsAbLDAPAutoCompFormatter::GetCommentFormat(nsAString & aCommentFormat)
{
    aCommentFormat  = mCommentFormat;
    return NS_OK;
}
NS_IMETHODIMP 
nsAbLDAPAutoCompFormatter::SetCommentFormat(const nsAString & 
                                            aCommentFormat)
{
    mCommentFormat = aCommentFormat; 

    return NS_OK;
}

#endif
