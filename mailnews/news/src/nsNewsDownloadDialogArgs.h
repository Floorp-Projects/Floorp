/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Seth Spitzer <sspitzer@netscape.com>
 */

#ifndef nsNewsDownloadDialogArgs_h__
#define nsNewsDownloadDialogArgs_h__

#include "nsINewsDownloadDialogArgs.h"
#include "nsString.h"

class nsNewsDownloadDialogArgs : public nsINewsDownloadDialogArgs
{
public:
  nsNewsDownloadDialogArgs();
  virtual ~nsNewsDownloadDialogArgs();

  NS_DECL_ISUPPORTS
  NS_DECL_NSINEWSDOWNLOADDIALOGARGS

private:
  nsCString mGroupName;
  PRInt32 mArticleCount;
  nsCString mServerKey;
  PRBool mHitOK;
  PRBool mDownloadAll;
};

#endif // nsNewsDownloadDialogArgs_h__
