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

#ifndef nsP3PUIService_h__
#define nsP3PUIService_h__

#include <nsCOMPtr.h>
#include <nsISupports.h>
#include <nsIDOMWindowInternal.h>
#include <nsIDocShellTreeItem.h>
#include <nsIP3PUI.h>
#include <nsIPref.h>
#include <nsDeque.h>
#include <nsHashtable.h>
#include "nsIP3PCService.h"
#include "nsIP3PUIService.h"


class nsP3PUIService : public nsIP3PUIService {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIP3PUIService methods
  NS_DECL_NSIP3PUISERVICE

  nsP3PUIService( );
  virtual ~nsP3PUIService( );

  static
  NS_METHOD             Create( nsISupports * aOuter,
                                REFNSIID      aIID,
                                void       ** aResult );

  NS_METHOD             Init( );

  NS_METHOD             WarningNotPrivate(nsIDOMWindowInternal * aDOMWindowInternal);

  NS_METHOD             WarningPartialPrivacy(nsIDOMWindowInternal * aDOMWindowInternal);

protected:

  NS_METHOD                     DocShellTreeItemToDOMWindowInternal( nsIDocShellTreeItem * aDocShellTreeItem,
                                                                     nsIDOMWindowInternal ** aResult);

  PRBool                        mInitialized;
  nsCOMPtr<nsIP3PCService>      mP3PService;
  nsCOMPtr<nsIPref>             mPrefServ;
  nsSupportsHashtable           mP3PUIs;

};

#endif                  /* nsP3PUIService_h__          */
