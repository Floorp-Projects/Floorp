/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Contributor(s):
 *   David Bienvenu <bienvenu@nventure.com>
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

#include "nsMsgThreadedDBView.h"
#include "nsHashtable.h"

class nsMsgGroupThread;

class nsMsgGroupView : public nsMsgThreadedDBView
{
public:
  nsMsgGroupView();
  virtual ~nsMsgGroupView();

  NS_IMETHOD Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder, nsMsgViewFlagsTypeValue viewFlags, PRInt32 *pCount);
  NS_IMETHOD OpenWithHdrs(nsISimpleEnumerator *aHeaders, nsMsgViewSortTypeValue aSortType, 
                                        nsMsgViewSortOrderValue aSortOrder, nsMsgViewFlagsTypeValue aViewFlags, 
                                        PRInt32 *aCount);
  NS_IMETHOD Close();
  NS_IMETHOD OnHdrDeleted(nsIMsgDBHdr *aHdrDeleted, nsMsgKey aParentKey, PRInt32 aFlags, 
                            nsIDBChangeListener *aInstigator);
  NS_IMETHOD OnHdrChange(nsIMsgDBHdr *aHdrChanged, PRUint32 aOldFlags, 
                                      PRUint32 aNewFlags, nsIDBChangeListener *aInstigator);

  NS_IMETHOD LoadMessageByViewIndex(nsMsgViewIndex aViewIndex);
  NS_IMETHOD GetCellProperties(PRInt32 aRow, nsITreeColumn *aCol, nsISupportsArray *aProperties);
  NS_IMETHOD GetRowProperties(PRInt32 aRow, nsISupportsArray *aProperties);
  NS_IMETHOD GetCellText(PRInt32 aRow, nsITreeColumn* aCol, nsAString& aValue);

protected:
  nsMsgGroupThread *AddHdrToThread(nsIMsgDBHdr *msgHdr);
  nsHashKey *AllocHashKeyForHdr(nsIMsgDBHdr *msgHdr); // caller must delete
  nsresult OnNewHeader(nsIMsgDBHdr *newHdr, nsMsgKey aParentKey, PRBool /*ensureListed*/);
  virtual nsresult GetThreadContainingMsgHdr(nsIMsgDBHdr *msgHdr, nsIMsgThread **pThread);
  virtual PRInt32 FindLevelInThread(nsIMsgDBHdr *msgHdr, nsMsgViewIndex startOfThread, nsMsgViewIndex viewIndex);
  nsMsgViewIndex ThreadIndexOfMsg(nsMsgKey msgKey, 
                                            nsMsgViewIndex msgIndex = nsMsgViewIndex_None,
                                            PRInt32 *pThreadCount = NULL,
                                            PRUint32 *pFlags = NULL);

  PRBool GroupViewUsesDummyRow(); // returns true if we are grouped by a sort attribute that uses a dummy row

  nsHashtable  m_groupsTable;

  static PRUnichar* kTodayString;
  static PRUnichar* kYesterdayString;
  static PRUnichar* kLastWeekString;
  static PRUnichar* kTwoWeeksAgoString;
  static PRUnichar* kOldMailString;
};

