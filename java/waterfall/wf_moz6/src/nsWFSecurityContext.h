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
 * The Original Code is The Waterfall Java Plugin Module
 * 
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 *
 * $Id: nsWFSecurityContext.h,v 1.1 2001/05/09 18:51:36 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#ifndef nsWFSecurityContext_h___
#define nsWFSecurityContext_h___

#include "nsPluggableJVM.h"
#include "nsISecurityContext.h"
#include "nsAgg.h"

class nsWFSecurityContext : public nsISecurityContext
{
public:

    // from nsISupports and AggregatedQueryInterface:

    NS_DECL_AGGREGATED

    static NS_METHOD Create(nsISupports* outer, 
			    nsPluggableJVM* pIJavaVM, 
			    jobject ctx,
			    const char* url, 
			    const nsIID& aIID, 
			    void* *aInstancePtr);

    NS_IMETHOD Implies(const char* target, 
		       const char* action, 
		       PRBool* bActionAllowed);

    NS_IMETHOD GetOrigin(char* buf, int len);

    NS_IMETHOD GetCertificateID(char* buf, int len);

   
    nsWFSecurityContext(nsISupports *aOuter, 
			nsPluggableJVM* pIJavaVM, 
			jobject ctx, 
			const char* url);
    virtual ~nsWFSecurityContext(void);

protected:
    nsPluggableJVM* m_jvm;
    jobject         context;
    char*           m_url;
    int             m_olen;
};

#endif 
