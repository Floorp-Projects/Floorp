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
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc.  Portions created by Sun are
 * Copyright (C) 2001 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Created by: Paul Sandoz   <paul.sandoz@sun.com> 
 *
 * Contributor(s): 
 */

#include "nsAbDirSearchListener.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsXPIDLString.h"


NS_IMPL_THREADSAFE_ISUPPORTS1(nsAbDirSearchListener, nsIAbDirectoryQueryResultListener)

nsAbDirSearchListener::nsAbDirSearchListener (nsAbDirSearchListenerContext* searchContext) :
    mSearchContext (searchContext)
{
    NS_INIT_ISUPPORTS();
}

nsAbDirSearchListener::~nsAbDirSearchListener ()
{
}


/* void onQueryItem (in nsIAbDirectoryQueryResult result); */
NS_IMETHODIMP nsAbDirSearchListener::OnQueryItem(nsIAbDirectoryQueryResult* result)
{
    nsresult rv;

    PRInt32 resultType;
    rv = result->GetType (&resultType);
    NS_ENSURE_SUCCESS(rv, rv);

    if (resultType != nsIAbDirectoryQueryResult::queryResultMatch)
    {
        rv = mSearchContext->OnSearchFinished (resultType);
        return rv;
    }

    nsCOMPtr<nsISupportsArray> properties;
    rv = result->GetResult (getter_AddRefs (properties));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 count;
    rv = properties->Count (&count);
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (count != 1)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsISupports> item;
    rv = properties->GetElementAt (0, getter_AddRefs (item));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAbDirectoryQueryPropertyValue> property(do_QueryInterface(item, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLCString name;
    rv = property->GetName (getter_Copies (name));
    NS_ENSURE_SUCCESS(rv, rv);

    if (nsCRT::strcasecmp (name, "card:nsIAbCard") != 0)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsISupports> cardSupports;
    rv = property->GetValueISupports (getter_AddRefs (cardSupports));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAbCard> card(do_QueryInterface(cardSupports, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mSearchContext->OnSearchFoundCard (card);

    return rv;
}

