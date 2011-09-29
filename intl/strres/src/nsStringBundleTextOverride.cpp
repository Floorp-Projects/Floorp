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
 * The Original Code is the string bundle override service.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
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


#include "nsStringBundleTextOverride.h"
#include "nsString.h"
#include "nsEscape.h"

#include "nsNetUtil.h"
#include "nsAppDirectoryServiceDefs.h"

static NS_DEFINE_CID(kPersistentPropertiesCID, NS_IPERSISTENTPROPERTIES_CID);


// first we need a simple class which wraps a nsIPropertyElement and
// cuts out the leading URL from the key
class URLPropertyElement : public nsIPropertyElement
{
public:
    URLPropertyElement(nsIPropertyElement *aRealElement, PRUint32 aURLLength) :
        mRealElement(aRealElement),
        mURLLength(aURLLength)
    { }
    virtual ~URLPropertyElement() {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROPERTYELEMENT
    
private:
    nsCOMPtr<nsIPropertyElement> mRealElement;
    PRUint32 mURLLength;
};

NS_IMPL_ISUPPORTS1(URLPropertyElement, nsIPropertyElement)

// we'll tweak the key on the way through, and remove the url prefix
NS_IMETHODIMP
URLPropertyElement::GetKey(nsACString& aKey)
{
    nsresult rv =  mRealElement->GetKey(aKey);
    if (NS_FAILED(rv)) return rv;

    // chop off the url
    aKey.Cut(0, mURLLength);
    
    return NS_OK;
}

// values are unaffected
NS_IMETHODIMP
URLPropertyElement::GetValue(nsAString& aValue)
{
    return mRealElement->GetValue(aValue);
}

// setters are kind of strange, hopefully we'll never be called
NS_IMETHODIMP
URLPropertyElement::SetKey(const nsACString& aKey)
{
    // this is just wrong - ideally you'd take the key, append it to
    // the url, and set that as the key. However, that would require
    // us to hold onto a copy of the string, and that's a waste,
    // considering nobody should ever be calling this.
    NS_ERROR("This makes no sense!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
URLPropertyElement::SetValue(const nsAString& aValue)
{
    return mRealElement->SetValue(aValue);
}


// this is a special enumerator which returns only the elements which
// are prefixed with a particular url
class nsPropertyEnumeratorByURL : public nsISimpleEnumerator
{
public:
    nsPropertyEnumeratorByURL(const nsACString& aURL,
                              nsISimpleEnumerator* aOuter) :
        mOuter(aOuter),
        mURL(aURL)
    {
        // prepare the url once so we can use its value later
        // persistent properties uses ":" as a delimiter, so escape
        // that character
        mURL.ReplaceSubstring(":", "%3A");
        // there is always a # between the url and the real key
        mURL.Append('#');
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR
    
    virtual ~nsPropertyEnumeratorByURL() {}
private:

    // actual enumerator of all strings from nsIProperties
    nsCOMPtr<nsISimpleEnumerator> mOuter;

    // the current element that is valid for this url
    nsCOMPtr<nsIPropertyElement> mCurrent;

    // the url in question, pre-escaped and with the # already in it
    nsCString mURL;
};

//
// nsStringBundleTextOverride implementation
//
NS_IMPL_ISUPPORTS1(nsStringBundleTextOverride,
                   nsIStringBundleOverride)

nsresult
nsStringBundleTextOverride::Init()
{
    nsresult rv;

    // check for existence of custom-strings.txt

    nsCOMPtr<nsIFile> customStringsFile;
    rv = NS_GetSpecialDirectory(NS_APP_CHROME_DIR,
                                getter_AddRefs(customStringsFile));

    if (NS_FAILED(rv)) return rv;

    // bail if not found - this will cause the service creation to
    // bail as well, and cause this object to go away

    customStringsFile->AppendNative(NS_LITERAL_CSTRING("custom-strings.txt"));

    bool exists;
    rv = customStringsFile->Exists(&exists);
    if (NS_FAILED(rv) || !exists)
        return NS_ERROR_FAILURE;

    NS_WARNING("Using custom-strings.txt to override string bundles.");
    // read in the custom bundle. Keys are in the form
    // chrome://package/locale/foo.properties:keyname

    nsCAutoString customStringsURLSpec;
    rv = NS_GetURLSpecFromFile(customStringsFile, customStringsURLSpec);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), customStringsURLSpec);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStream> in;
    rv = NS_OpenURI(getter_AddRefs(in), uri);
    if (NS_FAILED(rv)) return rv;

    mValues = do_CreateInstance(kPersistentPropertiesCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = mValues->Load(in);

    // turn this on to see the contents of custom-strings.txt
#ifdef DEBUG_alecf
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    mValues->Enumerate(getter_AddRefs(enumerator));
    NS_ASSERTION(enumerator, "no enumerator!\n");
    
    printf("custom-strings.txt contains:\n");
    printf("----------------------------\n");

    bool hasMore;
    enumerator->HasMoreElements(&hasMore);
    do {
        nsCOMPtr<nsISupports> sup;
        enumerator->GetNext(getter_AddRefs(sup));

        nsCOMPtr<nsIPropertyElement> prop = do_QueryInterface(sup);

        nsCAutoString key;
        nsAutoString value;
        prop->GetKey(key);
        prop->GetValue(value);

        printf("%s = '%s'\n", key.get(), NS_ConvertUTF16toUTF8(value).get());

        enumerator->HasMoreElements(&hasMore);
    } while (hasMore);
#endif
    
    return rv;
}

NS_IMETHODIMP
nsStringBundleTextOverride::GetStringFromName(const nsACString& aURL,
                                              const nsACString& key,
                                              nsAString& aResult)
{
    // concatenate url#key to get the key to read
    nsCAutoString combinedURL(aURL + NS_LITERAL_CSTRING("#") + key);

    // persistent properties uses ":" as a delimiter, so escape that character
    combinedURL.ReplaceSubstring(":", "%3A");

    return mValues->GetStringProperty(combinedURL, aResult);
}

NS_IMETHODIMP
nsStringBundleTextOverride::EnumerateKeysInBundle(const nsACString& aURL,
                                                  nsISimpleEnumerator** aResult)
{
    // enumerate all strings, and let the enumerator know
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    mValues->Enumerate(getter_AddRefs(enumerator));

    // make the enumerator wrapper and pass it off
    nsPropertyEnumeratorByURL* propEnum =
        new nsPropertyEnumeratorByURL(aURL, enumerator);

    if (!propEnum) return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(*aResult = propEnum);
    
    return NS_OK;
}


//
// nsPropertyEnumeratorByURL implementation
//


NS_IMPL_ISUPPORTS1(nsPropertyEnumeratorByURL, nsISimpleEnumerator)

NS_IMETHODIMP
nsPropertyEnumeratorByURL::GetNext(nsISupports **aResult)
{
    if (!mCurrent) return NS_ERROR_UNEXPECTED;

    // wrap mCurrent instead of returning it
    *aResult = new URLPropertyElement(mCurrent, mURL.Length());
    NS_ADDREF(*aResult);

    // release it so we don't return it twice
    mCurrent = nsnull;
    
    return NS_OK;
}

NS_IMETHODIMP
nsPropertyEnumeratorByURL::HasMoreElements(bool * aResult)
{
    bool hasMore;
    mOuter->HasMoreElements(&hasMore);
    while (hasMore) {

        nsCOMPtr<nsISupports> supports;
        mOuter->GetNext(getter_AddRefs(supports));

        mCurrent = do_QueryInterface(supports);

        if (mCurrent) {
            nsCAutoString curKey;
            mCurrent->GetKey(curKey);
        
            if (StringBeginsWith(curKey, mURL))
                break;
        }
        
        mOuter->HasMoreElements(&hasMore);
    }

    if (!hasMore)
        mCurrent = PR_FALSE;
    
    *aResult = mCurrent ? PR_TRUE : PR_FALSE;
    
    return NS_OK;
}
