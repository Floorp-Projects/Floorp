/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *
 */

#ifndef nsP3PObserverUtils_h__
#define nsP3PObserverUtils_h__

#include "nsIP3PCService.h"

#include <nsCom.h>

#include <nsIURI.h>

#include <nsIDocShellTreeItem.h>

#include <nsVoidArray.h>


class nsP3PObserverUtils {
public:
  static
  NS_METHOD             ExamineLINKTag( nsIURI               *aReferencingURI,
                                        PRUint32              aNumOfAttributes,
                                        const PRUnichar      *aNameArray[],
                                        const PRUnichar      *aValueArray[],
                                        nsIDocShellTreeItem  *aDocShellTreeItem,
                                        nsIP3PCService       *aP3PService );

  static
  NS_METHOD             ExamineLINKTag( nsIURI               *aReferencingURI,
                                        const nsStringArray  *aNames,
                                        const nsStringArray  *aValues,
                                        nsIDocShellTreeItem  *aDocShellTreeItem,
                                        nsIP3PCService       *aP3PService );
};

#endif                                           /* nsP3PObserverUtils_h__    */
