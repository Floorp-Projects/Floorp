/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#import <Foundation/Foundation.h>
#include "nsAboutBookmarks.h"

#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsStringStream.h"
#include "nsNetUtil.h"

#import "NSString+Utils.h"

NS_IMPL_ISUPPORTS1(nsAboutBookmarks, nsIAboutModule)

nsAboutBookmarks::nsAboutBookmarks(PRBool inIsBookmarks)
: mIsBookmarks(inIsBookmarks)
{
}

NS_IMETHODIMP
nsAboutBookmarks::NewChannel(nsIURI *aURI, nsIChannel **result)
{
    static NSString* const kBlankPageHTML = @"<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"> <html><head><title>%@</title></head><body></body></html>";

    nsresult rv;
    nsIChannel* channel;

    NSString* windowTitle = mIsBookmarks ? NSLocalizedString(@"BookmarksWindowTitle", @"")
                                         : NSLocalizedString(@"HistoryWindowTitle", @"");
    
    NSString* sourceString = [NSString stringWithFormat:kBlankPageHTML, windowTitle];
    nsAutoString pageSource;
    [sourceString assignTo_nsAString:pageSource];
    
    nsCOMPtr<nsIInputStream> in;
    rv = NS_NewCStringInputStream(getter_AddRefs(in),
                                  NS_ConvertUTF16toUTF8(pageSource));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_NewInputStreamChannel(&channel, aURI, in,
                                  NS_LITERAL_CSTRING("text/html"),
                                  NS_LITERAL_CSTRING("UTF8"));
    NS_ENSURE_SUCCESS(rv, rv);

    *result = channel;
    return rv;
}

NS_IMETHODIMP
nsAboutBookmarks::GetURIFlags(nsIURI *aURI, PRUint32 *result)
{
    *result = 0;
    return NS_OK;
}

NS_METHOD
nsAboutBookmarks::CreateBookmarks(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsAboutBookmarks* about = new nsAboutBookmarks(PR_TRUE);
    if (about == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(about);
    nsresult rv = about->QueryInterface(aIID, aResult);
    NS_RELEASE(about);
    return rv;
}

NS_METHOD
nsAboutBookmarks::CreateHistory(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsAboutBookmarks* about = new nsAboutBookmarks(PR_FALSE);
    if (about == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(about);
    nsresult rv = about->QueryInterface(aIID, aResult);
    NS_RELEASE(about);
    return rv;
}
