/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001-2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Seth Spitzer <sspitzer@netscape.com>
 *  Dan Mosedale <dmose@netscape.com>
 *  David Bienvenu <bienvenu@mozilla.org>
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

#ifndef nsSpamSettings_h__
#define nsSpamSettings_h__

#include "nsCOMPtr.h"
#include "nsISpamSettings.h"
#include "nsIFileSpec.h"
#include "nsIOutputStream.h"
#include "nsIMsgIncomingServer.h"
#include "nsIUrlListener.h"

class nsSpamSettings : public nsISpamSettings, public nsIUrlListener
{
public:
  nsSpamSettings();
  virtual ~nsSpamSettings();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISPAMSETTINGS
  NS_DECL_NSIURLLISTENER

private:
  nsCOMPtr <nsIMsgIncomingServer> mServer;  // make a weak ref?
  nsCOMPtr <nsIOutputStream> mLogStream;

  PRInt32 mManualMarkMode;
  PRInt32 mLevel; 
  PRInt32 mPurgeInterval;
  PRInt32 mMoveTargetMode;

  PRBool mManualMark;
  PRBool mLoggingEnabled;
  PRBool mPurge;
  PRBool mUseWhiteList;
  PRBool mMoveOnSpam;
  PRBool mMarkAsReadOnSpam;
  
  nsCString mActionTargetAccount;
  nsCString mActionTargetFolder;
  nsCString mWhiteListAbURI;
  nsCString mLogURL;

  nsCString mServerFilterName;
  PRInt32  mServerFilterTrustFlags;

  nsresult GetLogFileSpec(nsIFileSpec **aFileSpec);
  nsresult TruncateLog();
};

#endif /* nsSpamSettings_h__ */
