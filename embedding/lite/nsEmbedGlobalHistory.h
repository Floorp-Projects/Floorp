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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
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

#ifndef __nsEmbedGlobalHistory_h__
#define __nsEmbedGlobalHistory_h__

#include "nsIGlobalHistory.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsILocalFile.h"
#include "nsString.h"

class nsHashtable;
class nsHashKey;
class nsCStringKey;
class HistoryEntry;

//*****************************************************************************
// nsEmbedGlobalHistory
//*****************************************************************************   

// {2f977d51-5485-11d4-87e2-0010a4e75ef2}
#define NS_EMBEDGLOBALHISTORY_CID \
  { 0x2f977d51, 0x5485, 0x11d4, \
  { 0x87, 0xe2, 0x00, 0x10, 0xa4, 0xe7, 0x5e, 0xf2 } }

class nsEmbedGlobalHistory: public nsIGlobalHistory,
                            public nsIObserver,
                            public nsSupportsWeakReference
{
public:
                    nsEmbedGlobalHistory();
  virtual           ~nsEmbedGlobalHistory();
  
  NS_IMETHOD        Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLOBALHISTORY
  NS_DECL_NSIOBSERVER

protected:
  enum { kFlushModeAppend, kFlushModeFullWrite };
  
  nsresult          LoadData();
  nsresult          FlushData(PRIntn mode = kFlushModeFullWrite);
  nsresult          ResetData();

  nsresult          GetHistoryFile();

  PRBool            EntryHasExpired(HistoryEntry *entry);
  static PRIntn PR_CALLBACK enumRemoveEntryIfExpired(nsHashKey *aKey, void *aData, void* closure);

protected:
  PRBool            mDataIsLoaded;
  PRInt32           mEntriesAddedSinceFlush;
  nsCOMPtr<nsILocalFile>  mHistoryFile;
  nsHashtable       *mURLTable;
  PRInt64           mExpirationInterval;
};

#endif
