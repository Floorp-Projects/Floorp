/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Created by: Paul Sandoz   <paul.sandoz@sun.com> 
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

