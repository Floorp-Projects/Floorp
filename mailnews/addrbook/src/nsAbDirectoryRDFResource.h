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
 * Contributor(s): Paul Sandoz <paul.sandoz@sun.com>
 */

#ifndef nsAbDirectoryRDFResource_h__
#define nsAbDirectoryRDFResource_h__

#include "nsRDFResource.h"
#include "nsIAbBooleanExpression.h"
#include "nsString.h"

class nsAbDirectoryRDFResource : public nsRDFResource
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    nsAbDirectoryRDFResource ();
    virtual ~nsAbDirectoryRDFResource ();

    // nsIRDFResource methods:
    NS_IMETHOD Init(const char* aURI);
    
protected:
    PRBool mIsValidURI;
    PRBool mIsQueryURI;
    nsCString mPath;
    nsCString mQueryString;
    nsCString mURINoQuery;
};

#endif

