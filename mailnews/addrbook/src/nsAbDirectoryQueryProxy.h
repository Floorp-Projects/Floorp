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

#ifndef nsAbDirectoryQueryProxy_h__
#define nsAbDirectoryQueryProxy_h__

#include "nsIAbDirectoryQueryProxy.h"
#include "nsCOMPtr.h"

class nsAbDirectoryQueryProxy : public nsIAbDirectoryQueryProxy
{
public:
    NS_DECL_ISUPPORTS
    NS_FORWARD_NSIABDIRECTORYQUERY(mDirectoryQuery->)
    NS_DECL_NSIABDIRECTORYQUERYPROXY

    nsAbDirectoryQueryProxy();
    virtual ~nsAbDirectoryQueryProxy();

protected:
    PRBool mInitiated;
    nsCOMPtr<nsIAbDirectoryQuery> mDirectoryQuery;
};

#endif
