/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * XPCTL : nsILE.h
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
 * The Original Code is mozilla.org code.
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc.  Portions created by SUN are Copyright (C) 2000 SUN
 * Microsystems, Inc. All Rights Reserved.
 *
 * This module 'XPCTL Interface' is based on Pango (www.pango.org)
 * by Red Hat Software. Portions created by Redhat are Copyright (C)
 * 1999 Red Hat Software.
 *
 * Contributor(s):
 *   Prabhat Hegde (prabhat.hegde@sun.com)
 */

#ifndef nsILE_h__
#define nsILE_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsCtlCIID.h"

/*
 * nsILE Interface declaration 
 */
class nsILE : public nsISupports {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILE_IID)
  
  NS_IMETHOD NeedsCTLFix(const PRUnichar*, const PRInt32,
                         const PRInt32, PRBool *) = 0;

  NS_IMETHOD GetPresentationForm(const PRUnichar*, PRUint32,
                                 const char*, char*, PRSize*,
                                 PRBool = PR_FALSE) = 0;

  NS_IMETHOD PrevCluster(const PRUnichar*, PRUint32, 
                         const PRInt32, PRInt32*) = 0;

  NS_IMETHOD NextCluster(const PRUnichar*, PRUint32,
                         const PRInt32, PRInt32*) = 0;

  NS_IMETHOD GetRangeOfCluster(const PRUnichar*, PRUint32,
                               const PRInt32, PRInt32*, PRInt32*) = 0;
};
#endif /* !nsILE_h */

