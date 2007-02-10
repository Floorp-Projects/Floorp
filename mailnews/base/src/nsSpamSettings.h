/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001-2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Dan Mosedale <dmose@netscape.com>
 *   David Bienvenu <bienvenu@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsSpamSettings_h__
#define nsSpamSettings_h__

#include "nsCOMPtr.h"
#include "nsISpamSettings.h"
#include "nsString.h"
#include "nsIOutputStream.h"
#include "nsIMsgIncomingServer.h"
#include "nsIUrlListener.h"
#include "nsIDateTimeFormat.h"

class nsSpamSettings : public nsISpamSettings, public nsIUrlListener
{
public:
  nsSpamSettings();
  virtual ~nsSpamSettings();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISPAMSETTINGS
  NS_DECL_NSIURLLISTENER

private:
  nsCOMPtr <nsIOutputStream> mLogStream;
  nsCOMPtr<nsIFile> mLogFile;

  PRInt32 mLevel; 
  PRInt32 mPurgeInterval;
  PRInt32 mMoveTargetMode;

  PRBool mPurge;
  PRBool mUseWhiteList;
  PRBool mMoveOnSpam;
  PRBool mUseServerFilter;
  
  nsCString mActionTargetAccount;
  nsCString mActionTargetFolder;
  nsCString mWhiteListAbURI;
  nsCString mCurrentJunkFolderURI; // used to detect changes to the spam folder in ::initialize

  nsCString mServerFilterName;
  nsCOMPtr<nsIFile> mServerFilterFile;
  PRInt32  mServerFilterTrustFlags;

  nsCOMPtr<nsIDateTimeFormat> mDateFormatter;

  // helper routine used by Initialize which unsets the junk flag on the previous junk folder
  // for this account, and sets it on the new junk folder.
  nsresult UpdateJunkFolderState();
};

#endif /* nsSpamSettings_h__ */
