/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jan Varga (varga@utcru.sk)
 * Håkan Waara (hwaara@chello.se)
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

#include "msgCore.h"
#include "nsMsgDBView.h"
#include "nsISupports.h"
#include "nsIMsgFolder.h"
#include "nsIDBFolderInfo.h"
#include "nsIMsgDatabase.h"
#include "nsIMsgFolder.h"
#include "MailNewsTypes2.h"
#include "nsMsgUtils.h"
#include "nsXPIDLString.h"
#include "nsQuickSort.h"
#include "nsIMsgImapMailFolder.h"
#include "nsImapCore.h"
#include "nsMsgFolderFlags.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIDOMElement.h"
#include "nsDateTimeFormatCID.h"
#include "nsMsgMimeCID.h"

/* Implementation file */
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kDateTimeFormatCID,    NS_DATETIMEFORMAT_CID);

nsrefcnt nsMsgDBView::gInstanceCount	= 0;

nsIAtom * nsMsgDBView::kHighestPriorityAtom	= nsnull;
nsIAtom * nsMsgDBView::kHighPriorityAtom	= nsnull;
nsIAtom * nsMsgDBView::kLowestPriorityAtom	= nsnull;
nsIAtom * nsMsgDBView::kLowPriorityAtom	= nsnull;

nsIAtom * nsMsgDBView::kUnreadMsgAtom	= nsnull;
nsIAtom * nsMsgDBView::kNewMsgAtom = nsnull;
nsIAtom * nsMsgDBView::kReadMsgAtom	= nsnull;
nsIAtom * nsMsgDBView::kOfflineMsgAtom	= nsnull;
nsIAtom * nsMsgDBView::kFlaggedMsgAtom = nsnull;
nsIAtom * nsMsgDBView::kNewsMsgAtom = nsnull;
nsIAtom * nsMsgDBView::kImapDeletedMsgAtom = nsnull;
nsIAtom * nsMsgDBView::kAttachMsgAtom = nsnull;
nsIAtom * nsMsgDBView::kHasUnreadAtom = nsnull;

PRUnichar * nsMsgDBView::kHighestPriorityString = nsnull;
PRUnichar * nsMsgDBView::kHighPriorityString = nsnull;
PRUnichar * nsMsgDBView::kLowestPriorityString = nsnull;
PRUnichar * nsMsgDBView::kLowPriorityString = nsnull;
PRUnichar * nsMsgDBView::kNormalPriorityString = nsnull;
PRUnichar * nsMsgDBView::kReadString = nsnull;
PRUnichar * nsMsgDBView::kRepliedString = nsnull;
PRUnichar * nsMsgDBView::kForwardedString = nsnull;
PRUnichar * nsMsgDBView::kNewString = nsnull;

NS_IMPL_ADDREF(nsMsgDBView)
NS_IMPL_RELEASE(nsMsgDBView)

NS_INTERFACE_MAP_BEGIN(nsMsgDBView)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMsgDBView)
   NS_INTERFACE_MAP_ENTRY(nsIMsgDBView)
   NS_INTERFACE_MAP_ENTRY(nsIDBChangeListener)
   NS_INTERFACE_MAP_ENTRY(nsIOutlinerView)
   NS_INTERFACE_MAP_ENTRY(nsIMsgCopyServiceListener)
NS_INTERFACE_MAP_END

nsMsgDBView::nsMsgDBView()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  m_sortValid = PR_FALSE;
  m_sortOrder = nsMsgViewSortOrder::none;
  m_viewFlags = nsMsgViewFlagsType::kNone;
  m_cachedMsgKey = nsMsgKey_None;
  m_currentlyDisplayedMsgKey = nsMsgKey_None;
  mNumSelectedRows = 0;
  mSupressMsgDisplay = PR_FALSE;
  mOfflineMsgSelected = PR_FALSE;
  mIsSpecialFolder = PR_FALSE;
  mIsNews = PR_FALSE;
  mDeleteModel = nsMsgImapDeleteModels::MoveToTrash;
  m_deletingMsgs = PR_FALSE;
  mRemovingRow = PR_FALSE;
  // initialize any static atoms or unicode strings
  if (gInstanceCount == 0) 
  {
    InitializeAtomsAndLiterals();
  }
  
  gInstanceCount++;
}

void nsMsgDBView::InitializeAtomsAndLiterals()
{
  kUnreadMsgAtom = NS_NewAtom("unread");
  kNewMsgAtom = NS_NewAtom("new");
  kReadMsgAtom = NS_NewAtom("read");
  kOfflineMsgAtom = NS_NewAtom("offline");
  kFlaggedMsgAtom = NS_NewAtom("flagged");
  kNewsMsgAtom = NS_NewAtom("news");
  kImapDeletedMsgAtom = NS_NewAtom("imapdeleted");
  kAttachMsgAtom = NS_NewAtom("attach");
  kHasUnreadAtom = NS_NewAtom("hasUnread");

  kHighestPriorityAtom = NS_NewAtom("priority-highest");
  kHighPriorityAtom = NS_NewAtom("priority-high");
  kLowestPriorityAtom = NS_NewAtom("priority-lowest");
  kLowPriorityAtom = NS_NewAtom("priority-low");

  // priority strings
  kHighestPriorityString = GetString(NS_LITERAL_STRING("priorityHighest").get());
  kHighPriorityString = GetString(NS_LITERAL_STRING("priorityHigh").get());
  kLowestPriorityString =  GetString(NS_LITERAL_STRING("priorityLowest").get());
  kLowPriorityString = GetString(NS_LITERAL_STRING("priorityLow").get());
  kNormalPriorityString = GetString(NS_LITERAL_STRING("priorityNormal").get());

  kReadString = GetString(NS_LITERAL_STRING("read").get());
  kRepliedString = GetString(NS_LITERAL_STRING("replied").get());
  kForwardedString = GetString(NS_LITERAL_STRING("forwarded").get());
  kNewString = GetString(NS_LITERAL_STRING("new").get());
}

nsMsgDBView::~nsMsgDBView()
{
  if (m_db)
	  m_db->RemoveListener(this);

  gInstanceCount--;
  if (gInstanceCount <= 0) 
  {
    NS_IF_RELEASE(kUnreadMsgAtom);
    NS_IF_RELEASE(kNewMsgAtom);
    NS_IF_RELEASE(kReadMsgAtom);
    NS_IF_RELEASE(kOfflineMsgAtom);
    NS_IF_RELEASE(kFlaggedMsgAtom);
    NS_IF_RELEASE(kNewsMsgAtom);
    NS_IF_RELEASE(kImapDeletedMsgAtom);
    NS_IF_RELEASE(kAttachMsgAtom);
    NS_IF_RELEASE(kHasUnreadAtom);

    NS_IF_RELEASE(kHighestPriorityAtom);
    NS_IF_RELEASE(kHighPriorityAtom);
    NS_IF_RELEASE(kLowestPriorityAtom);
    NS_IF_RELEASE(kLowPriorityAtom);

    nsCRT::free(kHighestPriorityString);
    nsCRT::free(kHighPriorityString);
    nsCRT::free(kLowestPriorityString);
    nsCRT::free(kLowPriorityString);
    nsCRT::free(kNormalPriorityString);
    
    nsCRT::free(kReadString);
    nsCRT::free(kRepliedString);
    nsCRT::free(kForwardedString);
    nsCRT::free(kNewString);
  }
}

// helper function used to fetch strings from the messenger string bundle
PRUnichar * nsMsgDBView::GetString(const PRUnichar *aStringName)
{
	nsresult    res = NS_OK;
	PRUnichar   *ptrv = nsnull;

	if (!mMessengerStringBundle)
	{
		char    *propertyURL = MESSENGER_STRING_URL;
    nsCOMPtr<nsIStringBundleService> sBundleService = do_GetService(kStringBundleServiceCID, &res);
		if (NS_SUCCEEDED(res) && sBundleService) 
			res = sBundleService->CreateBundle(propertyURL, getter_AddRefs(mMessengerStringBundle));
	}

	if (mMessengerStringBundle)
		res = mMessengerStringBundle->GetStringFromName(aStringName, &ptrv);

	if ( NS_SUCCEEDED(res) && (ptrv) )
		return ptrv;
	else
		return nsCRT::strdup(aStringName);
}

///////////////////////////////////////////////////////////////////////////
// nsIOutlinerView Implementation Methods (and helper methods)
///////////////////////////////////////////////////////////////////////////

nsresult nsMsgDBView::FetchAuthor(nsIMsgHdr * aHdr, PRUnichar ** aSenderString)
{
  nsXPIDLString unparsedAuthor;
  if (!mHeaderParser)
    mHeaderParser = do_CreateInstance(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID);

  nsresult rv = NS_OK;
  if (mIsSpecialFolder)
    rv = aHdr->GetMime2DecodedRecipients(getter_Copies(unparsedAuthor));
  else
    rv = aHdr->GetMime2DecodedAuthor(getter_Copies(unparsedAuthor));
  
  // *sigh* how sad, we need to convert our beautiful unicode string to utf8 
  // so we can extract the name part of the address...then convert it back to 
  // unicode again.
  if (mHeaderParser)
  {
    nsXPIDLCString name;
    rv = mHeaderParser->ExtractHeaderAddressName("UTF-8", NS_ConvertUCS2toUTF8(unparsedAuthor).get(), getter_Copies(name));
    if (NS_SUCCEEDED(rv) && (const char*)name)
    {
      *aSenderString = nsCRT::strdup(NS_ConvertUTF8toUCS2(name).get());
      return NS_OK;
    }
  }

  // if we got here then just return the original string
  *aSenderString = nsCRT::strdup(unparsedAuthor);
  return NS_OK;
}

nsresult nsMsgDBView::FetchSubject(nsIMsgHdr * aMsgHdr, PRUint32 aFlags, PRUnichar ** aValue)
{
  if (aFlags & MSG_FLAG_HAS_RE)
  {
    nsXPIDLString subject;
    aMsgHdr->GetMime2DecodedSubject(getter_Copies(subject));
    nsAutoString reSubject;
    reSubject.Assign(NS_LITERAL_STRING("Re: "));
    reSubject.Append(subject);
    *aValue = reSubject.ToNewUnicode();
  }
  else
    aMsgHdr->GetMime2DecodedSubject(aValue);

  return NS_OK;
}

// in case we want to play around with the date string, I've broken it out into
// a separate routine. 
nsresult nsMsgDBView::FetchDate(nsIMsgHdr * aHdr, PRUnichar ** aDateString)
{
  PRTime dateOfMsg;
  nsAutoString formattedDateString;

  if (!mDateFormater)
    mDateFormater = do_CreateInstance(kDateTimeFormatCID);

  NS_ENSURE_TRUE(mDateFormater, NS_ERROR_FAILURE);
  nsresult rv = aHdr->GetDate(&dateOfMsg);

  PRTime currentTime = PR_Now();
	PRExplodedTime explodedCurrentTime;
  PR_ExplodeTime(currentTime, PR_LocalTimeParameters, &explodedCurrentTime);
  PRExplodedTime explodedMsgTime;
  PR_ExplodeTime(dateOfMsg, PR_LocalTimeParameters, &explodedMsgTime);

  // if the message is from today, don't show the date, only the time. (i.e. 3:15 pm)
  // if the message is from the last week, show the day of the week.   (i.e. Mon 3:15 pm)
  // in all other cases, show the full date (03/19/01 3:15 pm)
  nsDateFormatSelector dateFormat = kDateFormatShort;
  if (explodedCurrentTime.tm_year == explodedMsgTime.tm_year &&
      explodedCurrentTime.tm_month == explodedMsgTime.tm_month &&
      explodedCurrentTime.tm_mday == explodedMsgTime.tm_mday)
  {
    // same day...
    dateFormat = kDateFormatNone;
  } 
  // the following chunk of code causes us to show a day instead of a number if the message was received
  // within the last 7 days. i.e. Mon 5:10pm. We need to add a preference so folks to can enable this behavior
  // if they want it. 
/*
  else if (LL_CMP(currentTime, >, dateOfMsg))
  {
    PRInt64 microSecondsPerSecond, secondsInDays, microSecondsInDays;
	  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
    LL_UI2L(secondsInDays, 60 * 60 * 24 * 7); // how many seconds in 7 days.....
	  LL_MUL(microSecondsInDays, secondsInDays, microSecondsPerSecond); // turn that into microseconds

    PRInt64 diff;
    LL_SUB(diff, currentTime, dateOfMsg);
    if (LL_CMP(diff, <=, microSecondsInDays)) // within the same week 
      dateFormat = kDateFormatWeekday;
  }
*/
  if (NS_SUCCEEDED(rv)) 
    rv = mDateFormater->FormatPRTime(nsnull /* nsILocale* locale */,
                                      dateFormat,
                                      kTimeFormatNoSeconds,
                                      dateOfMsg,
                                      formattedDateString);

  if (NS_SUCCEEDED(rv))
    *aDateString = formattedDateString.ToNewUnicode();
  
  return rv;
}

nsresult nsMsgDBView::FetchStatus(PRUint32 aFlags, PRUnichar ** aStatusString)
{
  const PRUnichar * statusString = nsnull;

  if(aFlags & MSG_FLAG_REPLIED)
    statusString = kRepliedString;
	else if(aFlags & MSG_FLAG_FORWARDED)
		statusString = kForwardedString;
	else if(aFlags & MSG_FLAG_NEW)
		statusString = kNewString;
	else if(aFlags & MSG_FLAG_READ)
		statusString = kReadString;

  if (statusString)
    *aStatusString = nsCRT::strdup(statusString);
  else 
    *aStatusString = nsnull;

  return NS_OK;
}

nsresult nsMsgDBView::FetchSize(nsIMsgHdr * aHdr, PRUnichar ** aSizeString)
{
  nsAutoString formattedSizeString;
  PRUint32 msgSize = 0;

  // for news, show the line count not the size
  if (mIsNews) {
    aHdr->GetLineCount(&msgSize);
    formattedSizeString.AppendInt(msgSize);
  }
  else {
    aHdr->GetMessageSize(&msgSize);

	if(msgSize < 1024)
		msgSize = 1024;
	
    PRUint32 sizeInKB = msgSize/1024;
  
    formattedSizeString.AppendInt(sizeInKB);
    // XXX todo, fix this hard coded string?
    formattedSizeString.Append(NS_LITERAL_STRING("KB"));
  }

  *aSizeString = formattedSizeString.ToNewUnicode();
  return NS_OK;
}

nsresult nsMsgDBView::FetchPriority(nsIMsgHdr *aHdr, PRUnichar ** aPriorityString)
{
  nsMsgPriorityValue priority = nsMsgPriority::notSet;
  const PRUnichar * priorityString = nsnull;
  aHdr->GetPriority(&priority);

  switch (priority)
  {
  case nsMsgPriority::highest:
    priorityString = kHighestPriorityString;
    break;
  case nsMsgPriority::high:
    priorityString = kHighPriorityString;
    break;
  case nsMsgPriority::low:
    priorityString = kLowPriorityString;
    break;
  case nsMsgPriority::lowest:
    priorityString = kLowestPriorityString;
    break;
  case nsMsgPriority::normal:
    priorityString = kNormalPriorityString;
    break;
  default:
    break;
  }

  if (priorityString)
    *aPriorityString = nsCRT::strdup(priorityString);
  else
    *aPriorityString = nsnull;

  return NS_OK;
}

nsresult nsMsgDBView::SaveSelection(nsMsgKeyArray *aMsgKeyArray)
{
  if (!mOutlinerSelection)
    return NS_OK;

  // first, freeze selection.
  mOutlinerSelection->SetSelectEventsSuppressed(PR_TRUE);

  // second, get an array of view indices for the selection..
  nsUInt32Array selection;
  GetSelectedIndices(&selection);
  PRInt32 numIndices = selection.GetSize();

  // now store the msg key for each selected item.
  nsMsgKey msgKey;
  for (PRInt32 index = 0; index < numIndices; index++)
  {
    msgKey = m_keys.GetAt(selection.GetAt(index));
    aMsgKeyArray->Add(msgKey);
  }

  return NS_OK;
}

nsresult nsMsgDBView::RestoreSelection(nsMsgKeyArray * aMsgKeyArray)
{
  if (!mOutlinerSelection)  // don't assert.
    return NS_OK;

  // unfreeze selection.
  mOutlinerSelection->ClearSelection(); // clear the existing selection.
  
  // turn our message keys into corresponding view indices
  PRInt32 arraySize = aMsgKeyArray->GetSize();
  nsMsgViewIndex	currentViewPosition = nsMsgViewIndex_None;
  nsMsgViewIndex	newViewPosition;

  // if we are threaded, we need to do a little more work
  // we need to find (and expand) all the threads that contain messages 
  // that we had selected before.
  if (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay)
  {
    for (PRInt32 index = 0; index < arraySize; index ++) 
    {
      FindKey(aMsgKeyArray->GetAt(index), PR_TRUE /* expand */);
    }
  }

  // make sure the currentView was preserved....
  if (m_currentlyDisplayedMsgKey != nsMsgKey_None)
  {
    currentViewPosition = FindKey(m_currentlyDisplayedMsgKey, PR_FALSE);
    if (currentViewPosition != nsMsgViewIndex_None)
    {
      mOutlinerSelection->SetCurrentIndex(currentViewPosition);
      mOutlinerSelection->RangedSelect(currentViewPosition, currentViewPosition, PR_TRUE /* augment */);
        
      // make sure the current message is once again visible in the thread pane
      // so we don't have to go search for it in the thread pane
      if (mOutliner) mOutliner->EnsureRowIsVisible(currentViewPosition);
    }
  }

  for (PRInt32 index = 0; index < arraySize; index ++)
  {
    newViewPosition = FindKey(aMsgKeyArray->GetAt(index), PR_FALSE);  
    // check to make sure newViewPosition is valid.
    // add the index back to the selection.
    if (newViewPosition != currentViewPosition) // don't re-add the current view
      mOutlinerSelection->RangedSelect(newViewPosition, newViewPosition, PR_TRUE /* augment */);
  }

  mOutlinerSelection->SetSelectEventsSuppressed(PR_FALSE);
  return NS_OK;
}

nsresult nsMsgDBView::GenerateURIForMsgKey(nsMsgKey aMsgKey, nsIMsgFolder *folder, char ** aURI)
{
  NS_ENSURE_ARG(folder);
	return(folder->GenerateMessageURI(aMsgKey, aURI));
}

nsresult nsMsgDBView::CycleThreadedColumn(nsIDOMElement * aElement)
{
  nsAutoString currentView;

  // toggle threaded/unthreaded mode
  aElement->GetAttribute(NS_LITERAL_STRING("currentView"), currentView);
  if (currentView.Equals(NS_LITERAL_STRING("threaded")))
  {
    aElement->SetAttribute(NS_LITERAL_STRING("currentView"), NS_LITERAL_STRING("unthreaded"));

  }
  else
  {
     aElement->SetAttribute(NS_LITERAL_STRING("currentView"), NS_LITERAL_STRING("threaded"));
     // we must be unthreaded view. create a threaded view and replace ourself.

  }

  // i think we need to create a new view and switch it in this circumstance since
  // we are toggline between threaded and non threaded mode.


  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::IsEditable(PRInt32 index, const PRUnichar *colID, PRBool * aReturnValue)
{
  * aReturnValue = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::SetCellText(PRInt32 row, const PRUnichar *colID, const PRUnichar * value)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::GetRowCount(PRInt32 *aRowCount)
{
  *aRowCount = GetSize();
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::GetSelection(nsIOutlinerSelection * *aSelection)
{
  *aSelection = mOutlinerSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::SetSelection(nsIOutlinerSelection * aSelection)
{
  mOutlinerSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::ReloadMessage()
{
  if (!mSupressMsgDisplay && m_currentlyDisplayedMsgKey != nsMsgKey_None)
  {
    nsMsgKey currentMsgToReload = m_currentlyDisplayedMsgKey;
    m_currentlyDisplayedMsgKey = nsMsgKey_None;
    LoadMessageByMsgKey(currentMsgToReload);
  }

  return NS_OK;
}

nsresult nsMsgDBView::UpdateDisplayMessage(nsMsgKey aMsgKey)
{
  nsresult rv;
  if (mCommandUpdater)
  {
    // get the subject and the folder for the message and inform the front end that
    // we changed the message we are currently displaying.
    nsMsgViewIndex viewPosition = FindViewIndex(aMsgKey);
    if (viewPosition != nsMsgViewIndex_None)
    {
      nsCOMPtr <nsIMsgDBHdr> msgHdr;
      rv = GetMsgHdrForViewIndex(viewPosition, getter_AddRefs(msgHdr));
      NS_ENSURE_SUCCESS(rv,rv);
      nsXPIDLString subject;
      FetchSubject(msgHdr, m_flags[viewPosition], getter_Copies(subject));
      mCommandUpdater->DisplayMessageChanged(m_folder, subject);
    } // if view position is valid
  } // if we have an updater
  return NS_OK;
}

// given a msg key, we will load the message for it.
NS_IMETHODIMP nsMsgDBView::LoadMessageByMsgKey(nsMsgKey aMsgKey)
{
  NS_ASSERTION(aMsgKey != nsMsgKey_None,"trying to load nsMsgKey_None");
  if (aMsgKey == nsMsgKey_None) return NS_ERROR_UNEXPECTED;

  if (!mSupressMsgDisplay && (m_currentlyDisplayedMsgKey != aMsgKey))
  {
    nsXPIDLCString uri;
    nsresult rv = GenerateURIForMsgKey(aMsgKey, m_folder, getter_Copies(uri));
    NS_ENSURE_SUCCESS(rv,rv);
    mMessengerInstance->OpenURL(uri);
    m_currentlyDisplayedMsgKey = aMsgKey;
    UpdateDisplayMessage(aMsgKey);
  }

  return NS_OK;
}


NS_IMETHODIMP nsMsgDBView::SelectionChanged()
{
  // if the currentSelection changed then we have a message to display
  PRUint32 numSelected = 0;

  GetNumSelected(&numSelected);
  nsUInt32Array selection;
  GetSelectedIndices(&selection);
  nsMsgViewIndex *indices = selection.GetData();

  PRBool offlineMsgSelected = (indices) ? OfflineMsgSelected(indices, numSelected) : PR_FALSE;
  // if only one item is selected then we want to display a message
  if (numSelected == 1)
  {
    PRInt32 startRange;
    PRInt32 endRange;
    nsresult rv = mOutlinerSelection->GetRangeAt(0, &startRange, &endRange);
    NS_ENSURE_SUCCESS(rv, NS_OK); // outliner doesn't care if we failed

    if (startRange >= 0 && startRange == endRange && startRange < GetSize())
    {
      // get the msgkey for the message
      nsMsgKey msgkey = m_keys.GetAt(startRange);
      if (!mRemovingRow)
      {
        if (!mSupressMsgDisplay)
          LoadMessageByMsgKey(msgkey);
        else
          UpdateDisplayMessage(msgkey);
      }
    }
    else
      numSelected = 0; // selection seems bogus, so set to 0.
  }

  // determine if we need to push command update notifications out to the UI or not.

  // we need to push a command update notification iff, one of the following conditions are met
  // (1) the selection went from 0 to 1
  // (2) it went from 1 to 0
  // (3) it went from 1 to many
  // (4) it went from many to 1 or 0
  // (5) a different msg was selected - perhaps it was offline or not...

  if ((numSelected == mNumSelectedRows || 
      (numSelected > 1 && mNumSelectedRows > 1)) && mOfflineMsgSelected == offlineMsgSelected)
  {

  }
  else if (mCommandUpdater) // o.t. push an updated
  {
    mCommandUpdater->UpdateCommandStatus();
  }
  
  mOfflineMsgSelected = offlineMsgSelected;
  mNumSelectedRows = numSelected;
  return NS_OK;
}

nsresult nsMsgDBView::GetSelectedIndices(nsUInt32Array *selection)
{
  if (mOutlinerSelection)
  {
    PRInt32 selectionCount; 
    nsresult rv = mOutlinerSelection->GetRangeCount(&selectionCount);
    for (PRInt32 i = 0; i < selectionCount; i++)
    {
      PRInt32 startRange;
      PRInt32 endRange;
      rv = mOutlinerSelection->GetRangeAt(i, &startRange, &endRange);
      NS_ENSURE_SUCCESS(rv, NS_OK); 
      PRInt32 viewSize = GetSize();
      if (startRange >= 0 && startRange < viewSize)
      {
        for (PRInt32 rangeIndex = startRange; rangeIndex <= endRange && rangeIndex < viewSize; rangeIndex++)
          selection->Add(rangeIndex);
      }
    }
  }
  else
  {
    // if there is no outliner selection object then we must be in stand alone message mode.
    // in that case the selected indices are really just the current message key.
    nsMsgViewIndex viewIndex = FindViewIndex(m_currentlyDisplayedMsgKey);
    if (viewIndex != nsMsgViewIndex_None)
      selection->Add(viewIndex);
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::GetRowProperties(PRInt32 index, nsISupportsArray *properties)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::GetColumnProperties(const PRUnichar *colID, nsIDOMElement * aElement, nsISupportsArray *properties)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::GetCellProperties(PRInt32 aRow, const PRUnichar *colID, nsISupportsArray *properties)
{
  if (!IsValidIndex(aRow))
    return NS_MSG_INVALID_DBVIEW_INDEX; 

  // this is where we tell the outliner to apply styles to a particular row
  // i.e. if the row is an unread message...

  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  nsresult rv = NS_OK;

  rv = GetMsgHdrForViewIndex(aRow, getter_AddRefs(msgHdr));

  if (NS_FAILED(rv) || !msgHdr) {
    ClearHdrCache();
    return NS_MSG_INVALID_DBVIEW_INDEX;
  }

  PRUint32 flags = m_flags.GetAt(aRow);
  if (!(flags & MSG_FLAG_READ))
    properties->AppendElement(kUnreadMsgAtom);  
  else 
    properties->AppendElement(kReadMsgAtom);  

  if (flags & MSG_FLAG_NEW)
    properties->AppendElement(kNewMsgAtom);  

  if (flags & MSG_FLAG_OFFLINE)
    properties->AppendElement(kOfflineMsgAtom);  
  
  if (flags & MSG_FLAG_ATTACHMENT) 
    properties->AppendElement(kAttachMsgAtom);

  if ((mDeleteModel == nsMsgImapDeleteModels::IMAPDelete) && (flags & MSG_FLAG_IMAP_DELETED)) 
    properties->AppendElement(kImapDeletedMsgAtom);

  if (mIsNews)
    properties->AppendElement(kNewsMsgAtom);
    
  // add special styles for priority
  nsMsgPriorityValue priority;
  msgHdr->GetPriority(&priority);
  switch (priority)
  {
  case nsMsgPriority::highest:
    properties->AppendElement(kHighestPriorityAtom);  
    break;
  case nsMsgPriority::high:
    properties->AppendElement(kHighPriorityAtom);  
    break;
  case nsMsgPriority::low:
    properties->AppendElement(kLowPriorityAtom);  
    break;
  case nsMsgPriority::lowest:
    properties->AppendElement(kLowestPriorityAtom);  
    break;
  default:
    break;
  }

  if (colID[0] == 'f')  
  {
    if (m_flags[aRow] & MSG_FLAG_MARKED) 
    {
      properties->AppendElement(kFlaggedMsgAtom); 
    }
  }

  if (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay)
  {
    if (m_flags[aRow] & MSG_VIEW_FLAG_ISTHREAD)
    {
      nsCOMPtr <nsIMsgThread> thread;
      rv = GetThreadContainingIndex(aRow, getter_AddRefs(thread));
      if (NS_SUCCEEDED(rv) && thread)
      {
        PRUint32 numUnreadChildren;
        thread->GetNumUnreadChildren(&numUnreadChildren);
        if (numUnreadChildren > 0)
        {
            properties->AppendElement(kHasUnreadAtom);
        }   
      }
    }
  }     

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::IsContainer(PRInt32 index, PRBool *_retval)
{
  if (!IsValidIndex(index))
      return NS_MSG_INVALID_DBVIEW_INDEX; 

  if (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay)
  {
    PRUint32 flags = m_flags[index];
    *_retval = (flags & MSG_VIEW_FLAG_HASCHILDREN);
  }
  else
    *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::IsContainerOpen(PRInt32 index, PRBool *_retval)
{
  if (!IsValidIndex(index))
      return NS_MSG_INVALID_DBVIEW_INDEX; 

  if (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay)
  {
    PRUint32 flags = m_flags[index];
    *_retval = (flags & MSG_VIEW_FLAG_HASCHILDREN) && !(flags & MSG_FLAG_ELIDED);
  }
  else
    *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::IsContainerEmpty(PRInt32 index, PRBool *_retval)
{
  if (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay)
  {
    PRUint32 flags = m_flags[index];
    *_retval = !(flags & MSG_VIEW_FLAG_HASCHILDREN);
  }
  else
    *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::GetParentIndex(PRInt32 rowIndex, PRInt32 *_retval)
{  
  *_retval = -1;

  PRInt32 rowIndexLevel;
  GetLevel(rowIndex, &rowIndexLevel);

  PRInt32 i;
  for(i = rowIndex; i >= 0; i--) {
    PRInt32 l;
    GetLevel(i, &l);
    if (l < rowIndexLevel) {
      *_retval = i;
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::HasNextSibling(PRInt32 rowIndex, PRInt32 afterIndex, PRBool *_retval)
{
  *_retval = PR_FALSE;

  PRInt32 rowIndexLevel;
  GetLevel(rowIndex, &rowIndexLevel);

  PRInt32 i;
  PRInt32 count;
  GetRowCount(&count);
  for(i = afterIndex + 1; i < count; i++) {
    PRInt32 l;
    GetLevel(i, &l);
    if (l < rowIndexLevel)
      break;
    if (l == rowIndexLevel) {
      *_retval = PR_TRUE;
      break;
    }
  }                                                                       

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::GetLevel(PRInt32 index, PRInt32 *_retval)
{
  if (!IsValidIndex(index))
    return NS_MSG_INVALID_DBVIEW_INDEX;

  if (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay)
    *_retval = m_levels[index];
  else
    *_retval = 0;
  return NS_OK;
}

// search view will override this since headers can span db's
nsresult nsMsgDBView::GetMsgHdrForViewIndex(nsMsgViewIndex index, nsIMsgDBHdr **msgHdr)
{
  nsresult rv = NS_OK;
  nsMsgKey key = m_keys.GetAt(index);
  if (key == nsMsgKey_None || !m_db)
    return NS_MSG_INVALID_DBVIEW_INDEX;
  
  if (key == m_cachedMsgKey)
  {
    *msgHdr = m_cachedHdr;
    NS_IF_ADDREF(*msgHdr);
  }
  else
  {
    rv = m_db->GetMsgHdrForKey(key, msgHdr);
    if (NS_SUCCEEDED(rv))
    {
      m_cachedHdr = *msgHdr;
      m_cachedMsgKey = key;
    }
  }

  return rv;
}

nsresult nsMsgDBView::GetFolderForViewIndex(nsMsgViewIndex index, nsIMsgFolder **aFolder)
{
  *aFolder = m_folder;
  NS_IF_ADDREF(*aFolder);
  return NS_OK;
}

nsresult nsMsgDBView::GetDBForViewIndex(nsMsgViewIndex index, nsIMsgDatabase **db)
{
  *db = m_db;
  NS_IF_ADDREF(*db);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::GetCellText(PRInt32 aRow, const PRUnichar * aColID, PRUnichar ** aValue)
{
  nsresult rv = NS_OK;

  if (!IsValidIndex(aRow))
    return NS_MSG_INVALID_DBVIEW_INDEX;

  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  rv = GetMsgHdrForViewIndex(aRow, getter_AddRefs(msgHdr));
  
  if (NS_FAILED(rv) || !msgHdr) {
    ClearHdrCache();
    return NS_MSG_INVALID_DBVIEW_INDEX;
  }

  *aValue = 0;
  // just a hack
  nsXPIDLCString dbString;
  nsCOMPtr <nsIMsgThread> thread;

  switch (aColID[0])
  {
  case 's':
    if (aColID[1] == 'u') // subject
      rv = FetchSubject(msgHdr, m_flags[aRow], aValue);
    else if (aColID[1] == 'e') // sender
      rv = FetchAuthor(msgHdr, aValue);
    else if (aColID[1] == 'i') // size
      rv = FetchSize(msgHdr, aValue);
    else
      rv = FetchStatus(m_flags[aRow], aValue);
    break;
  case 'd':  // date
    rv = FetchDate(msgHdr, aValue);
    break;
  case 'p': // priority
    rv = FetchPriority(msgHdr, aValue);
    break;
  case 't':   
    // total msgs in thread column
    if (aColID[1] == 'o' && (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay))
    {
      if (m_flags[aRow] & MSG_VIEW_FLAG_ISTHREAD)
      {
        rv = GetThreadContainingIndex(aRow, getter_AddRefs(thread));
        if (NS_SUCCEEDED(rv) && thread)
        {
          nsAutoString formattedCountString;
          PRUint32 numChildren;
          thread->GetNumChildren(&numChildren);
          formattedCountString.AppendInt(numChildren);
          *aValue = formattedCountString.ToNewUnicode();
        }
      }
    }
    break;
  case 'u':
    // unread msgs in thread col
    if (aColID[6] == 'C' && (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay))
    {
      if (m_flags[aRow] & MSG_VIEW_FLAG_ISTHREAD)
      {
        rv = GetThreadContainingIndex(aRow, getter_AddRefs(thread));
        if (NS_SUCCEEDED(rv) && thread)
        {
          nsAutoString formattedCountString;
          PRUint32 numUnreadChildren;
          thread->GetNumUnreadChildren(&numUnreadChildren);
          if (numUnreadChildren > 0)
          {
            formattedCountString.AppendInt(numUnreadChildren);
            *aValue = formattedCountString.ToNewUnicode();
          }
        }
      }
    }
    break;
  default:
    break;
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::SetOutliner(nsIOutlinerBoxObject *outliner)
{
  mOutliner = outliner;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::ToggleOpenState(PRInt32 index)
{
  PRUint32 numChanged;
  nsresult rv = ToggleExpansion(index, &numChanged);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::CycleHeader(const PRUnichar * aColID, nsIDOMElement * aElement)
{
    // let HandleColumnClick() in threadPane.js handle it
    // since it will set / clear the sort indicators.
    return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::CycleCell(PRInt32 row, const PRUnichar *colID)
{
  if (!IsValidIndex(row))
    return NS_MSG_INVALID_DBVIEW_INDEX;

  switch (colID[0])
  {
  case 'u': // unreadButtonColHeader
    if (colID[6] == 'B') 
      ApplyCommandToIndices(nsMsgViewCommandType::toggleMessageRead, (nsMsgViewIndex *) &row, 1);
   break;
  case 't': // threaded cell or total cell
    if (colID[1] == 'h') 
    {
      ExpandAndSelectThreadByIndex(row);
    }
    break;
  case 'f': // flagged column
    // toggle the flagged status of the element at row.
    if (m_flags[row] & MSG_FLAG_MARKED)
      ApplyCommandToIndices(nsMsgViewCommandType::unflagMessages, (nsMsgViewIndex *) &row, 1);
    else
      ApplyCommandToIndices(nsMsgViewCommandType::flagMessages, (nsMsgViewIndex *) &row, 1);
    break;
  default:
    break;

  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::PerformAction(const PRUnichar *action)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::PerformActionOnCell(const PRUnichar *action, PRInt32 row, const PRUnichar *colID)
{
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////
// end nsIOutlinerView Implementation Methods
///////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsMsgDBView::Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder, nsMsgViewFlagsTypeValue viewFlags, PRInt32 *pCount)
{

  m_viewFlags = viewFlags;
  m_sortOrder = sortOrder;
  m_sortType = sortType;
  nsMsgViewTypeValue viewType;

  if (folder) // search view will have a null folder
  {
    nsCOMPtr <nsIDBFolderInfo> folderInfo;
    nsresult rv = folder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(m_db));
    NS_ENSURE_SUCCESS(rv,rv);
	  m_db->AddListener(this);
    m_folder = folder;
    // save off sort type and order, view type and flags
    folderInfo->SetSortType(sortType);
    folderInfo->SetSortOrder(sortOrder);
    folderInfo->SetViewFlags(viewFlags);
    GetViewType(&viewType);
    folderInfo->SetViewType(viewType);

    // determine if we are in a news folder or not.
    // if yes, we'll show lines instead of size, and special icons in the thread pane
    nsCOMPtr <nsIMsgIncomingServer> server;
    rv = folder->GetServer(getter_AddRefs(server));
    NS_ENSURE_SUCCESS(rv,rv);
    nsXPIDLCString type;
    rv = server->GetType(getter_Copies(type));
    NS_ENSURE_SUCCESS(rv,rv);
    mIsNews = !nsCRT::strcmp("nntp",type.get());
    GetImapDeleteModel(nsnull);
    // for sent, unsent and draft folders, be sure to set mIsSpecialFolder so we'll show the recipient field
    // in place of the author.
    PRUint32 folderFlags = 0;
    m_folder->GetFlags(&folderFlags);
    if ( (folderFlags & MSG_FOLDER_FLAG_DRAFTS) || (folderFlags & MSG_FOLDER_FLAG_SENTMAIL)
          || (folderFlags & MSG_FOLDER_FLAG_QUEUE))
      mIsSpecialFolder = PR_TRUE;
  }
#ifdef HAVE_PORT
	CacheAdd ();
#endif

	return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::Close()
{
  if (mOutliner) 
    mOutliner->RowCountChanged(0, -GetSize());
  // this is important, because the outliner will ask us for our
  // row count, which get determine from the number of keys.
  m_keys.RemoveAll();
  // be consistent
  m_flags.RemoveAll();
  m_levels.RemoveAll();
  ClearHdrCache();
  if (m_db)
  {
  	m_db->RemoveListener(this);
    m_db = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::Init(nsIMessenger * aMessengerInstance, nsIMsgWindow * aMsgWindow, nsIMsgDBViewCommandUpdater *aCmdUpdater)
{
  mMsgWindow = aMsgWindow;
  mMessengerInstance = aMessengerInstance;
  mCommandUpdater = aCmdUpdater;

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::SetSupressMsgDisplay(PRBool aSupressDisplay)
{
  PRBool forceDisplay = PR_FALSE;
  if (mSupressMsgDisplay && (mSupressMsgDisplay != aSupressDisplay))
    forceDisplay = PR_TRUE;

  mSupressMsgDisplay = aSupressDisplay;
  if (forceDisplay)
  {
    // get the messae key for the currently selected message
    nsMsgKey msgKey;
    nsCOMPtr<nsIMsgDBHdr> dbHdr;
    GetHdrForFirstSelectedMessage(getter_AddRefs(dbHdr));
    if (dbHdr)
    { 
      nsresult rv = dbHdr->GetMessageKey(&msgKey);
      if (NS_SUCCEEDED(rv))
       LoadMessageByMsgKey(msgKey);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::GetSupressMsgDisplay(PRBool * aSupressDisplay)
{
  *aSupressDisplay = mSupressMsgDisplay;
  return NS_OK;
}

nsresult nsMsgDBView::AddKeys(nsMsgKey *pKeys, PRInt32 *pFlags, const char *pLevels, nsMsgViewSortTypeValue sortType, PRInt32 numKeysToAdd)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

int PR_CALLBACK CompareViewIndices (const void *v1, const void *v2, void *)
{
	nsMsgViewIndex i1 = *(nsMsgViewIndex*) v1;
	nsMsgViewIndex i2 = *(nsMsgViewIndex*) v2;
	return i1 - i2;
}

NS_IMETHODIMP nsMsgDBView::GetIndicesForSelection(nsMsgViewIndex **indices,  PRUint32 *length)
{
  NS_ENSURE_ARG_POINTER(length);
  *length = 0;
  NS_ENSURE_ARG_POINTER(indices);
  *indices = nsnull;

  nsUInt32Array selection;
  GetSelectedIndices(&selection);
  *length = selection.GetSize();
  PRUint32 numIndicies = *length;
  if (!numIndicies) return NS_OK;

  *indices = (nsMsgViewIndex *)nsMemory::Alloc(numIndicies * sizeof(nsMsgViewIndex));
  if (!indices) return NS_ERROR_OUT_OF_MEMORY;
  for (PRUint32 i=0;i<numIndicies;i++) {
    (*indices)[i] = selection.GetAt(i);
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::GetURIsForSelection(char ***uris, PRUint32 *length)
{
  nsresult rv = NS_OK;

#ifdef DEBUG_mscott
  printf("inside GetURIsForSelection\n");
#endif
  NS_ENSURE_ARG_POINTER(length);
  *length = 0;
  NS_ENSURE_ARG_POINTER(uris);
  *uris = nsnull;

  nsUInt32Array selection;
  GetSelectedIndices(&selection);
  *length = selection.GetSize();
  PRUint32 numIndicies = *length;
  if (!numIndicies) return NS_OK;

  nsCOMPtr <nsIMsgFolder> folder = m_folder;
  char **outArray, **next;
  next = outArray = (char **)nsMemory::Alloc(numIndicies * sizeof(char *));
  if (!outArray) return NS_ERROR_OUT_OF_MEMORY;
  for (PRUint32 i=0;i<numIndicies;i++) 
  {
    nsMsgViewIndex selectedIndex = selection.GetAt(i);
    if (!folder)
      GetFolderForViewIndex(selectedIndex, getter_AddRefs(folder));
    rv = GenerateURIForMsgKey(m_keys[selectedIndex], folder, next);
    NS_ENSURE_SUCCESS(rv,rv);
    if (!*next) return NS_ERROR_OUT_OF_MEMORY;
    next++;
  }

  *uris = outArray;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::GetURIForViewIndex(nsMsgViewIndex index, char **result)
{
  nsresult rv;
  nsCOMPtr <nsIMsgFolder> folder = m_folder;
  if (!folder) {
    rv = GetFolderForViewIndex(index, getter_AddRefs(folder));
    NS_ENSURE_SUCCESS(rv,rv);
  }
  rv = GenerateURIForMsgKey(m_keys[index], folder, result);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::DoCommandWithFolder(nsMsgViewCommandTypeValue command, nsIMsgFolder *destFolder)
{
  nsUInt32Array selection;

  NS_ENSURE_ARG_POINTER(destFolder);

  GetSelectedIndices(&selection);

  nsMsgViewIndex *indices = selection.GetData();
  PRInt32 numIndices = selection.GetSize();

  nsresult rv = NS_OK;
  switch (command) {
    case nsMsgViewCommandType::copyMessages:
    case nsMsgViewCommandType::moveMessages:
        // since the FE could have constructed the list of indices in
        // any order (e.g. order of discontiguous selection), we have to
        // sort the indices in order to find out which nsMsgViewIndex will
        // be deleted first.
        if (numIndices > 1) {
            NS_QuickSort (indices, numIndices, sizeof(nsMsgViewIndex), CompareViewIndices, nsnull);
        }
        NoteStartChange(nsMsgViewNotificationCode::none, 0, 0);
        rv = ApplyCommandToIndicesWithFolder(command, indices, numIndices, destFolder);
        NoteEndChange(nsMsgViewNotificationCode::none, 0, 0);
        break;
    default:
        NS_ASSERTION(PR_FALSE, "invalid command type");
        rv = NS_ERROR_UNEXPECTED;
        break;
  }
  return rv;

}

NS_IMETHODIMP nsMsgDBView::DoCommand(nsMsgViewCommandTypeValue command)
{
  nsUInt32Array selection;

  GetSelectedIndices(&selection);

  nsMsgViewIndex *indices = selection.GetData();
  PRInt32 numIndices = selection.GetSize();

  nsresult rv = NS_OK;
  switch (command)
  {

  case nsMsgViewCommandType::downloadSelectedForOffline:
    return DownloadForOffline(mMsgWindow, indices, numIndices);
  case nsMsgViewCommandType::downloadFlaggedForOffline:
    return DownloadFlaggedForOffline(mMsgWindow);
  case nsMsgViewCommandType::markMessagesRead:
  case nsMsgViewCommandType::markMessagesUnread:
  case nsMsgViewCommandType::toggleMessageRead:
  case nsMsgViewCommandType::flagMessages:
  case nsMsgViewCommandType::unflagMessages:
  case nsMsgViewCommandType::deleteMsg:
  case nsMsgViewCommandType::deleteNoTrash:
  case nsMsgViewCommandType::markThreadRead:
		// since the FE could have constructed the list of indices in
		// any order (e.g. order of discontiguous selection), we have to
		// sort the indices in order to find out which nsMsgViewIndex will
		// be deleted first.
		if (numIndices > 1)
			NS_QuickSort (indices, numIndices, sizeof(nsMsgViewIndex), CompareViewIndices, nsnull);
    NoteStartChange(nsMsgViewNotificationCode::none, 0, 0);
		rv = ApplyCommandToIndices(command, indices, numIndices);
    NoteEndChange(nsMsgViewNotificationCode::none, 0, 0);
    break;
  case nsMsgViewCommandType::selectAll:
    if (mOutlinerSelection && mOutliner) {
        // if in threaded mode, we need to expand all before selecting
        if (m_sortType == nsMsgViewSortType::byThread) {
            rv = ExpandAll();
        }
        mOutlinerSelection->SelectAll();
        mOutliner->Invalidate();
    }
    break;
  case nsMsgViewCommandType::selectThread:
    rv = ExpandAndSelectThread();
    break;
  case nsMsgViewCommandType::selectFlagged:
#ifdef DEBUG_seth
    printf("clear current selection, and then select all flagged messages, we may have to expand all first to look for them\n");
#endif
    break;
  case nsMsgViewCommandType::markAllRead:
    if (m_folder)
      rv = m_folder->MarkAllMessagesRead();
    break;
  case nsMsgViewCommandType::toggleThreadWatched:
    rv = ToggleWatched(indices,	numIndices);
    break;
  case nsMsgViewCommandType::expandAll:
    rv = ExpandAll();
    mOutliner->Invalidate();
    break;
  case nsMsgViewCommandType::collapseAll:
    rv = CollapseAll();
    mOutliner->Invalidate();
    break;
  default:
    NS_ASSERTION(PR_FALSE, "invalid command type");
    rv = NS_ERROR_UNEXPECTED;
    break;
  }
  return rv;
}

NS_IMETHODIMP nsMsgDBView::GetCommandStatus(nsMsgViewCommandTypeValue command, PRBool *selectable_p, nsMsgViewCommandCheckStateValue *selected_p)
{
  nsUInt32Array selection;

  GetSelectedIndices(&selection);

  //nsMsgViewIndex *indices = selection.GetData();
  PRInt32 numindices = selection.GetSize();

  nsresult rv = NS_OK;
  switch (command)
  {
  case nsMsgViewCommandType::markMessagesRead:
  case nsMsgViewCommandType::markMessagesUnread:
  case nsMsgViewCommandType::toggleMessageRead:
  case nsMsgViewCommandType::flagMessages:
  case nsMsgViewCommandType::unflagMessages:
  case nsMsgViewCommandType::toggleThreadWatched:
  case nsMsgViewCommandType::deleteMsg:
  case nsMsgViewCommandType::deleteNoTrash:
  case nsMsgViewCommandType::markThreadRead:
  case nsMsgViewCommandType::downloadSelectedForOffline:
    *selectable_p = (numindices > 0);
    break;
  case nsMsgViewCommandType::cmdRequiringMsgBody:
    {
    nsMsgViewIndex *indices = selection.GetData();
    *selectable_p = (numindices > 0) && (!WeAreOffline() || OfflineMsgSelected(indices, numindices));
    }
    break;
  case nsMsgViewCommandType::downloadFlaggedForOffline:
  case nsMsgViewCommandType::markAllRead:
    *selectable_p = PR_TRUE;
    break;
  default:
    NS_ASSERTION(PR_FALSE, "invalid command type");
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}

nsresult 
nsMsgDBView::CopyMessages(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices, PRBool isMove, nsIMsgFolder *destFolder)
{
  nsresult rv = NS_OK;
  NS_ENSURE_ARG_POINTER(destFolder);
  nsCOMPtr<nsISupportsArray> messageArray;
  NS_NewISupportsArray(getter_AddRefs(messageArray));
  for (nsMsgViewIndex index = 0; index < (nsMsgViewIndex) numIndices; index++) {
    nsMsgKey key = m_keys.GetAt(indices[index]);
    nsCOMPtr <nsIMsgDBHdr> msgHdr;
    rv = m_db->GetMsgHdrForKey(key, getter_AddRefs(msgHdr));
    NS_ENSURE_SUCCESS(rv,rv);
    if (msgHdr)
      messageArray->AppendElement(msgHdr);
  }
  m_deletingMsgs = isMove;
  rv = destFolder->CopyMessages(m_folder /* source folder */, messageArray, isMove, window, this /* listener */, PR_FALSE /* isFolder */, PR_TRUE /*allowUndo*/);

  return rv;
}

nsresult
nsMsgDBView::ApplyCommandToIndicesWithFolder(nsMsgViewCommandTypeValue command, nsMsgViewIndex* indices,
                    PRInt32 numIndices, nsIMsgFolder *destFolder)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(destFolder);

  switch (command) {
    case nsMsgViewCommandType::copyMessages:
        NS_ASSERTION(!(m_folder == destFolder), "The source folder and the destination folder are the same");
        if (m_folder != destFolder)
          rv = CopyMessages(mMsgWindow, indices, numIndices, PR_FALSE /* isMove */, destFolder);
        break;
    case nsMsgViewCommandType::moveMessages:
        NS_ASSERTION(!(m_folder == destFolder), "The source folder and the destination folder are the same");
        if (m_folder != destFolder)
          rv = CopyMessages(mMsgWindow, indices, numIndices, PR_TRUE  /* isMove */, destFolder);
        break;
    default:
        NS_ASSERTION(PR_FALSE, "unhandled command");
        rv = NS_ERROR_UNEXPECTED;
        break;
    }
    return rv;
}

nsresult
nsMsgDBView::ApplyCommandToIndices(nsMsgViewCommandTypeValue command, nsMsgViewIndex* indices,
					PRInt32 numIndices)
{
	nsresult rv = NS_OK;
	nsMsgKeyArray imapUids;

    nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(m_folder);
	PRBool thisIsImapFolder = (imapFolder != nsnull);
    if (command == nsMsgViewCommandType::deleteMsg)
		rv = DeleteMessages(mMsgWindow, indices, numIndices, PR_FALSE);
	else if (command == nsMsgViewCommandType::deleteNoTrash)
		rv = DeleteMessages(mMsgWindow, indices, numIndices, PR_TRUE);
	else
	{
		for (int32 i = 0; i < numIndices; i++)
		{
			if (thisIsImapFolder && command != nsMsgViewCommandType::markThreadRead)
				imapUids.Add(GetAt(indices[i]));
			
			switch (command)
			{
			case nsMsgViewCommandType::markMessagesRead:
				rv = SetReadByIndex(indices[i], PR_TRUE);
				break;
			case nsMsgViewCommandType::markMessagesUnread:
				rv = SetReadByIndex(indices[i], PR_FALSE);
				break;
			case nsMsgViewCommandType::toggleMessageRead:
				rv = ToggleReadByIndex(indices[i]);
				break;
			case nsMsgViewCommandType::flagMessages:
				rv = SetFlaggedByIndex(indices[i], PR_TRUE);
				break;
			case nsMsgViewCommandType::unflagMessages:
				rv = SetFlaggedByIndex(indices[i], PR_FALSE);
				break;
			case nsMsgViewCommandType::markThreadRead:
				rv = SetThreadOfMsgReadByIndex(indices[i], imapUids, PR_TRUE);
				break;
			default:
				NS_ASSERTION(PR_FALSE, "unhandled command");
				break;
			}
		}
		
		if (thisIsImapFolder)
		{
			imapMessageFlagsType flags = kNoImapMsgFlag;
			PRBool addFlags = PR_FALSE;
			PRBool isRead = PR_FALSE;

			switch (command)
			{
			case nsMsgViewCommandType::markThreadRead:
			case nsMsgViewCommandType::markMessagesRead:
				flags |= kImapMsgSeenFlag;
				addFlags = PR_TRUE;
				break;
			case nsMsgViewCommandType::markMessagesUnread:
				flags |= kImapMsgSeenFlag;
				addFlags = PR_FALSE;
				break;
			case nsMsgViewCommandType::toggleMessageRead:
				{
					flags |= kImapMsgSeenFlag;
					m_db->IsRead(GetAt(indices[0]), &isRead);
					if (isRead)
						addFlags = PR_TRUE;
					else
            addFlags = PR_FALSE;
				}
				break;
			case nsMsgViewCommandType::flagMessages:
				flags |= kImapMsgFlaggedFlag;
				addFlags = PR_TRUE;
				break;
			case nsMsgViewCommandType::unflagMessages:
				flags |= kImapMsgFlaggedFlag;
        addFlags = PR_FALSE;
				break;
			default:
				break;
			}
			
			if (flags != kNoImapMsgFlag)	// can't get here without thisIsImapThreadPane == TRUE
				imapFolder->StoreImapFlags(flags, addFlags, imapUids.GetArray(), imapUids.GetSize());
			
		}
	}
	return rv;
}
// view modifications methods by index

// This method just removes the specified line from the view. It does
// NOT delete it from the database.
nsresult nsMsgDBView::RemoveByIndex(nsMsgViewIndex index)
{
	if (!IsValidIndex(index))
		return NS_MSG_INVALID_DBVIEW_INDEX;

	m_keys.RemoveAt(index);
	m_flags.RemoveAt(index);
	m_levels.RemoveAt(index);
	NoteChange(index, -1, nsMsgViewNotificationCode::insertOrDelete);
	return NS_OK;
}

nsresult nsMsgDBView::DeleteMessages(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices, PRBool deleteStorage)
{
  nsresult rv = NS_OK;
	nsCOMPtr<nsISupportsArray> messageArray;
	NS_NewISupportsArray(getter_AddRefs(messageArray));
  for (nsMsgViewIndex index = 0; index < (nsMsgViewIndex) numIndices; index++)
  {
    nsMsgKey key = m_keys.GetAt(indices[index]);
    nsCOMPtr <nsIMsgDBHdr> msgHdr;
    rv = m_db->GetMsgHdrForKey(key, getter_AddRefs(msgHdr));
    NS_ENSURE_SUCCESS(rv,rv);
    if (msgHdr)
      messageArray->AppendElement(msgHdr);

  }
  m_deletingMsgs = PR_TRUE;
  m_folder->DeleteMessages(messageArray, window, deleteStorage, PR_FALSE, this, PR_TRUE /*allow Undo*/ );

  return rv;
}

nsresult nsMsgDBView::DownloadForOffline(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsISupportsArray> messageArray;
  NS_NewISupportsArray(getter_AddRefs(messageArray));
  for (nsMsgViewIndex index = 0; index < (nsMsgViewIndex) numIndices; index++)
  {
    nsMsgKey key = m_keys.GetAt(indices[index]);
    nsCOMPtr <nsIMsgDBHdr> msgHdr;
    rv = m_db->GetMsgHdrForKey(key, getter_AddRefs(msgHdr));
    NS_ENSURE_SUCCESS(rv,rv);
    if (msgHdr)
    {
      PRUint32 flags;
      msgHdr->GetFlags(&flags);
      if (!(flags & MSG_FLAG_OFFLINE))
        messageArray->AppendElement(msgHdr);
    }
  }
  m_folder->DownloadMessagesForOffline(messageArray, window);
  return rv;
}

nsresult nsMsgDBView::DownloadFlaggedForOffline(nsIMsgWindow *window)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsISupportsArray> messageArray;
  NS_NewISupportsArray(getter_AddRefs(messageArray));
  nsCOMPtr <nsISimpleEnumerator> enumerator;
  rv = m_db->EnumerateMessages(getter_AddRefs(enumerator));
  if (NS_SUCCEEDED(rv) && enumerator)
  {
    PRBool hasMore;
    
    while (NS_SUCCEEDED(rv = enumerator->HasMoreElements(&hasMore)) && (hasMore == PR_TRUE)) 
    {
      nsCOMPtr <nsIMsgDBHdr> pHeader;
      rv = enumerator->GetNext(getter_AddRefs(pHeader));
      NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
      if (pHeader && NS_SUCCEEDED(rv))
      {
        PRUint32 flags;
        pHeader->GetFlags(&flags);
        if ((flags & MSG_FLAG_MARKED) && !(flags & MSG_FLAG_OFFLINE))
          messageArray->AppendElement(pHeader);
      }
    }
  }
  m_folder->DownloadMessagesForOffline(messageArray, window);
  return rv;
}

// read/unread handling.
nsresult nsMsgDBView::ToggleReadByIndex(nsMsgViewIndex index)
{
  if (!IsValidIndex(index))
    return NS_MSG_INVALID_DBVIEW_INDEX;
  return SetReadByIndex(index, !(m_flags[index] & MSG_FLAG_READ));
}

nsresult nsMsgDBView::SetReadByIndex(nsMsgViewIndex index, PRBool read)
{
  nsresult rv;
  
  if (!IsValidIndex(index))
    return NS_MSG_INVALID_DBVIEW_INDEX;
  if (read) {
    OrExtraFlag(index, MSG_FLAG_READ);
    // MarkRead() will clear this flag in the db
    // and then call OnKeyChange(), but
    // because we are the instigator of the change
    // we'll ignore the change.
    //
    // so we need to clear it in m_flags
    // to keep the db and m_flags in sync
    AndExtraFlag(index, ~MSG_FLAG_NEW);
  }
  else {
    AndExtraFlag(index, ~MSG_FLAG_READ);
  }
  
  nsCOMPtr <nsIMsgDatabase> dbToUse;
  rv = GetDBForViewIndex(index, getter_AddRefs(dbToUse));
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = dbToUse->MarkRead(m_keys[index], read, this);
  NoteChange(index, 1, nsMsgViewNotificationCode::changed);
  if (m_sortType == nsMsgViewSortType::byThread)
  {
    nsMsgViewIndex threadIndex = ThreadIndexOfMsg(m_keys[index], index, nsnull, nsnull);
    if (threadIndex != index)
      NoteChange(threadIndex, 1, nsMsgViewNotificationCode::changed);
  }
  return rv;
}

nsresult nsMsgDBView::SetThreadOfMsgReadByIndex(nsMsgViewIndex index, nsMsgKeyArray &keysMarkedRead, PRBool /*read*/)
{
	nsresult rv;

	if (!IsValidIndex(index))
		return NS_MSG_INVALID_DBVIEW_INDEX;
	rv = MarkThreadOfMsgRead(m_keys[index], index, keysMarkedRead, PR_TRUE);
	return rv;
}

nsresult nsMsgDBView::SetFlaggedByIndex(nsMsgViewIndex index, PRBool mark)
{
	nsresult rv;

	if (!IsValidIndex(index))
		return NS_MSG_INVALID_DBVIEW_INDEX;

  nsCOMPtr <nsIMsgDatabase> dbToUse;
  rv = GetDBForViewIndex(index, getter_AddRefs(dbToUse));
  NS_ENSURE_SUCCESS(rv, rv);

	if (mark)
		OrExtraFlag(index, MSG_FLAG_MARKED);
	else
		AndExtraFlag(index, ~MSG_FLAG_MARKED);

	rv = dbToUse->MarkMarked(m_keys[index], mark, this);
	NoteChange(index, 1, nsMsgViewNotificationCode::changed);
	return rv;
}



// reversing threads involves reversing the threads but leaving the
// expanded messages ordered relative to the thread, so we
// make a copy of each array and copy them over.
nsresult nsMsgDBView::ReverseThreads()
{
    nsUInt32Array *newFlagArray = new nsUInt32Array;
    if (!newFlagArray) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nsMsgKeyArray *newKeyArray = new nsMsgKeyArray;
    if (!newKeyArray) {
        delete newFlagArray;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nsUint8Array *newLevelArray = new nsUint8Array;
    if (!newLevelArray) {
        delete newFlagArray;
        delete newKeyArray;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    PRInt32 sourceIndex, destIndex;
    PRInt32 viewSize = GetSize();

    newKeyArray->SetSize(m_keys.GetSize());
    newFlagArray->SetSize(m_flags.GetSize());
    newLevelArray->SetSize(m_levels.GetSize());

    for (sourceIndex = 0, destIndex = viewSize - 1; sourceIndex < viewSize;) {
        PRInt32 endThread;  // find end of current thread.
        PRBool inExpandedThread = PR_FALSE;
        for (endThread = sourceIndex; endThread < viewSize; endThread++) {
            PRUint32 flags = m_flags.GetAt(endThread);
            if (!inExpandedThread && (flags & (MSG_VIEW_FLAG_ISTHREAD|MSG_VIEW_FLAG_HASCHILDREN)) && !(flags & MSG_FLAG_ELIDED))
                inExpandedThread = PR_TRUE;
            else if (flags & MSG_VIEW_FLAG_ISTHREAD) {
                if (inExpandedThread)
                    endThread--;
                break;
            }
        }

        if (endThread == viewSize)
            endThread--;
        PRInt32 saveEndThread = endThread;
        while (endThread >= sourceIndex)
        {
            newKeyArray->SetAt(destIndex, m_keys.GetAt(endThread));
            newFlagArray->SetAt(destIndex, m_flags.GetAt(endThread));
            newLevelArray->SetAt(destIndex, m_levels.GetAt(endThread));
            endThread--;
            destIndex--;
        }
        sourceIndex = saveEndThread + 1;
    }
    // this copies the contents of both arrays - it would be cheaper to
    // just assign the new data ptrs to the old arrays and "forget" the new
    // arrays' data ptrs, so they won't be freed when the arrays are deleted.
    m_keys.RemoveAll();
    m_flags.RemoveAll();
    m_levels.RemoveAll();
    m_keys.InsertAt(0, newKeyArray);
    m_flags.InsertAt(0, newFlagArray);
    m_levels.InsertAt(0, newLevelArray);

    // if we swizzle data pointers for these arrays, this won't be right.
    delete newFlagArray;
    delete newKeyArray;
    delete newLevelArray;

    return NS_OK;
}

nsresult nsMsgDBView::ReverseSort()
{
    PRUint32 num = GetSize();
	
    nsCOMPtr <nsISupportsArray> folders;
    GetFolders(getter_AddRefs(folders));

    // go up half the array swapping values
    for (PRUint32 i = 0; i < (num / 2); i++) {
        // swap flags
        PRUint32 end = num - i - 1;
        PRUint32 tempFlags = m_flags.GetAt(i);
        m_flags.SetAt(i, m_flags.GetAt(end));
        m_flags.SetAt(end, tempFlags);

        // swap keys
        nsMsgKey tempKey = m_keys.GetAt(i);
        m_keys.SetAt(i, m_keys.GetAt(end));
        m_keys.SetAt(end, tempKey);

        if (folders)
        {
            // swap folders -- 
            // needed when search is done across multiple folders
            nsCOMPtr <nsISupports> tmpSupports 
                = getter_AddRefs(folders->ElementAt(i));
            folders->SetElementAt(i, folders->ElementAt(end));
            folders->SetElementAt(end, tmpSupports);
        }
        // no need to swap elements in m_levels, 
        // since we won't call ReverseSort() if we
        // are in threaded mode, so m_levels are all the same.
    }

    return NS_OK;
}

typedef struct entryInfo {
    nsMsgKey    id;
    PRUint32    bits;
    PRUint32    len;
    //PRUint32    pad;
    nsIMsgFolder* folder;
} EntryInfo;

typedef struct tagIdKey {
    EntryInfo   info;
    PRUint8     key[1];
} IdKey;


typedef struct tagIdPtrKey {
    EntryInfo   info;
    PRUint8     *key;
} IdKeyPtr;

int PR_CALLBACK
FnSortIdKey(const void *pItem1, const void *pItem2, void *privateData)
{
    PRInt32 retVal = 0;
    nsresult rv;

    IdKey** p1 = (IdKey**)pItem1;
    IdKey** p2 = (IdKey**)pItem2;

    nsIMsgDatabase *db = (nsIMsgDatabase *)privateData;

    rv = db->CompareCollationKeys((*p1)->key,(*p1)->info.len,(*p2)->key,(*p2)->info.len,&retVal);
    NS_ASSERTION(NS_SUCCEEDED(rv),"compare failed");

    if (retVal != 0)
        return(retVal);
    if ((*p1)->info.id >= (*p2)->info.id)
        return(1);
    else
        return(-1);
}

int PR_CALLBACK
FnSortIdKeyPtr(const void *pItem1, const void *pItem2, void *privateData)
{
    PRInt32 retVal = 0;
    nsresult rv;

    IdKeyPtr** p1 = (IdKeyPtr**)pItem1;
    IdKeyPtr** p2 = (IdKeyPtr**)pItem2;

    nsIMsgDatabase *db = (nsIMsgDatabase *)privateData;

    rv = db->CompareCollationKeys((*p1)->key,(*p1)->info.len,(*p2)->key,(*p2)->info.len,&retVal);
    NS_ASSERTION(NS_SUCCEEDED(rv),"compare failed");

    if (retVal != 0)
        return(retVal);
    if ((*p1)->info.id >= (*p2)->info.id)
        return(1);
    else
        return(-1);
}


typedef struct tagIdDWord {
    EntryInfo   info;
    PRUint32    dword;
} IdDWord;

int PR_CALLBACK
FnSortIdDWord(const void *pItem1, const void *pItem2, void *privateData)
{
    IdDWord** p1 = (IdDWord**)pItem1;
    IdDWord** p2 = (IdDWord**)pItem2;
    if ((*p1)->dword > (*p2)->dword)
        return(1);
    else if ((*p1)->dword < (*p2)->dword)
        return(-1);
    else if ((*p1)->info.id >= (*p2)->info.id)
        return(1);
    else
        return(-1);
}

typedef struct tagIdPRTime {
    EntryInfo   info;
    PRTime      prtime;
} IdPRTime;

int PR_CALLBACK
FnSortIdPRTime(const void *pItem1, const void *pItem2, void *privateData)
{
    IdPRTime ** p1 = (IdPRTime**)pItem1;
    IdPRTime ** p2 = (IdPRTime**)pItem2;

    if (LL_CMP((*p1)->prtime, >, (*p2)->prtime)) 
        return(1);
    else if (LL_CMP((*p1)->prtime, <, (*p2)->prtime))  
        return(-1);
    else if ((*p1)->info.id >= (*p2)->info.id)
        return(1);
    else
        return(-1);
}

// XXX are these still correct?
//To compensate for memory alignment required for
//systems such as HP-UX these values must be 4 bytes
//aligned.  Don't break this when modify the constants
const int kMaxSubjectKey = 160;
const int kMaxLocationKey = 160;
const int kMaxAuthorKey = 160;
const int kMaxRecipientKey = 80;

nsresult nsMsgDBView::GetFieldTypeAndLenForSort(nsMsgViewSortTypeValue sortType, PRUint16 *pMaxLen, eFieldType *pFieldType)
{
    NS_ENSURE_ARG_POINTER(pMaxLen);
    NS_ENSURE_ARG_POINTER(pFieldType);

    switch (sortType) {
        case nsMsgViewSortType::bySubject:
            *pFieldType = kCollationKey;
            *pMaxLen = kMaxSubjectKey;
            break;
        case nsMsgViewSortType::byLocation:
            *pFieldType = kCollationKey;
            *pMaxLen = kMaxLocationKey;
            break;
        case nsMsgViewSortType::byRecipient:
            *pFieldType = kCollationKey;
            *pMaxLen = kMaxRecipientKey;
            break;
        case nsMsgViewSortType::byAuthor:
            *pFieldType = kCollationKey;
            *pMaxLen = kMaxAuthorKey;
            break;
        case nsMsgViewSortType::byDate:
            *pFieldType = kPRTime;
            *pMaxLen = sizeof(PRTime);
            break;
        case nsMsgViewSortType::byPriority:
        case nsMsgViewSortType::byThread:
        case nsMsgViewSortType::byId:
        case nsMsgViewSortType::bySize:
        case nsMsgViewSortType::byFlagged:
        case nsMsgViewSortType::byUnread:
        case nsMsgViewSortType::byStatus:
            *pFieldType = kU32;
            *pMaxLen = sizeof(PRUint32);
            break;
        default:
            return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
}

nsresult nsMsgDBView::GetPRTimeField(nsIMsgHdr *msgHdr, nsMsgViewSortTypeValue sortType, PRTime *result)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(msgHdr);
  NS_ENSURE_ARG_POINTER(result);

  switch (sortType) {
    case nsMsgViewSortType::byDate:
        rv = msgHdr->GetDate(result);
        break;
    default:
        NS_ASSERTION(0,"should not be here");
        rv = NS_ERROR_UNEXPECTED;
    }

    NS_ENSURE_SUCCESS(rv,rv);
    return NS_OK;
}

#define MSG_STATUS_MASK (MSG_FLAG_REPLIED | MSG_FLAG_FORWARDED)

nsresult nsMsgDBView::GetStatusSortValue(nsIMsgHdr *msgHdr, PRUint32 *result)
{
  NS_ENSURE_ARG_POINTER(msgHdr);
  NS_ENSURE_ARG_POINTER(result);

  PRUint32 messageFlags;
  nsresult rv = msgHdr->GetFlags(&messageFlags);
  NS_ENSURE_SUCCESS(rv,rv);

  if (messageFlags & MSG_FLAG_NEW) {
    // happily, new by definition stands alone
    *result = 0;
    return NS_OK;
  }

  switch (messageFlags & MSG_STATUS_MASK) {
    case MSG_FLAG_REPLIED:
        *result = 2;
        break;
    case MSG_FLAG_FORWARDED|MSG_FLAG_REPLIED:
        *result = 1;
        break;
    case MSG_FLAG_FORWARDED:
        *result = 3;
        break;
    default:
        if (messageFlags & MSG_FLAG_READ) {
            *result = 4;
        }
        else {
            *result = 5;
        }
        break;
    }

    return NS_OK;
}

nsresult nsMsgDBView::GetLongField(nsIMsgHdr *msgHdr, nsMsgViewSortTypeValue sortType, PRUint32 *result)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(msgHdr);
  NS_ENSURE_ARG_POINTER(result);

  PRBool isRead;
  PRUint32 bits;

  switch (sortType) {
    case nsMsgViewSortType::bySize:
        if (mIsNews) {
            rv = msgHdr->GetLineCount(result);
        }
        else {
            rv = msgHdr->GetMessageSize(result);
        }
        break;
    case nsMsgViewSortType::byPriority: 
        nsMsgPriorityValue priority;
        rv = msgHdr->GetPriority(&priority);

        // treat "none" as "normal" when sorting.
        if (priority == nsMsgPriority::none) {
            priority = nsMsgPriority::normal;
        }

        // we want highest priority to have lowest value
        // so ascending sort will have highest priority first.
        *result = nsMsgPriority::highest - priority;
        break;
    case nsMsgViewSortType::byStatus:
        rv = GetStatusSortValue(msgHdr,result);
        break;
    case nsMsgViewSortType::byFlagged:
        bits = 0;
        rv = msgHdr->GetFlags(&bits);
        *result = !(bits & MSG_FLAG_MARKED);  //make flagged come out on top.
        break;
    case nsMsgViewSortType::byUnread:
        rv = msgHdr->GetIsRead(&isRead);
        if (NS_SUCCEEDED(rv)) 
            *result = !isRead;
        break;
    case nsMsgViewSortType::byId:
        // handled by caller, since caller knows the key
    default:
        NS_ASSERTION(0,"should not be here");
        rv = NS_ERROR_UNEXPECTED;
        break;
    }
    
    NS_ENSURE_SUCCESS(rv,rv);
    return NS_OK;
}


nsresult 
nsMsgDBView::GetCollationKey(nsIMsgHdr *msgHdr, nsMsgViewSortTypeValue sortType, PRUint8 **result, PRUint32 *len)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(msgHdr);
  NS_ENSURE_ARG_POINTER(result);

  switch (sortType) {
    case nsMsgViewSortType::bySubject:
        rv = msgHdr->GetSubjectCollationKey(result, len);
        break;
    case nsMsgViewSortType::byLocation:
        rv = GetLocationCollationKey(msgHdr, result, len);
        break;
    case nsMsgViewSortType::byRecipient:
        rv = msgHdr->GetRecipientsCollationKey(result, len);
        break;
    case nsMsgViewSortType::byAuthor:
        rv = msgHdr->GetAuthorCollationKey(result, len);
        break;
    default:
        rv = NS_ERROR_UNEXPECTED;
        break;
    }

    // bailing out with failure will stop the sort and leave us in
    // a bad state.  try to continue on, instead
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get the collation key");
    if (NS_FAILED(rv)) {
        *result = nsnull;
        *len = 0;
    }
    return NS_OK;
}

// As the location collation key is created getting folder from the msgHdr,
// it is defined in this file and not from the db.
nsresult 
nsMsgDBView::GetLocationCollationKey(nsIMsgHdr *msgHdr, PRUint8 **result, PRUint32 *len)
{
    nsresult rv;
    nsCOMPtr <nsIMsgFolder> folder;

    rv = msgHdr->GetFolder(getter_AddRefs(folder));
    NS_ENSURE_SUCCESS(rv,rv);

    if (!folder)
      return NS_ERROR_NULL_POINTER;

    nsCOMPtr <nsIMsgDatabase> dbToUse;
    rv = folder->GetMsgDatabase(nsnull, getter_AddRefs(dbToUse));
    NS_ENSURE_SUCCESS(rv,rv);

    nsXPIDLString locationString; 
    rv = folder->GetPrettiestName(getter_Copies(locationString));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = dbToUse->CreateCollationKey(locationString, result, len);
    NS_ENSURE_SUCCESS(rv,rv);

    return NS_OK;
}

nsresult nsMsgDBView::SaveSortInfo(nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder)
{
  if (m_folder)
  {
    nsCOMPtr <nsIDBFolderInfo> folderInfo;
    nsresult rv = m_folder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(m_db));
    if (NS_SUCCEEDED(rv) && folderInfo)
    {
      // save off sort type and order, view type and flags
      folderInfo->SetSortType(sortType);
      folderInfo->SetSortOrder(sortOrder);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::Sort(nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder)
{
    nsresult rv;

  if (m_sortType == sortType && m_sortValid) 
  {
    if (m_sortOrder == sortOrder) 
    {
            // same as it ever was.  do nothing
            return NS_OK;
        }   
    else 
    {
      SaveSortInfo(sortType, sortOrder);
            if (m_sortType != nsMsgViewSortType::byThread) {
                rv = ReverseSort();
                NS_ENSURE_SUCCESS(rv,rv);
            }
            else {
                rv = ReverseThreads();
                NS_ENSURE_SUCCESS(rv,rv);
            }

            m_sortOrder = sortOrder;
            // we just reversed the sort order...we still need to invalidate the view
            return NS_OK;
        }
    }

    if (sortType == nsMsgViewSortType::byThread) {
        return NS_OK;
    }

    SaveSortInfo(sortType, sortOrder);
    // figure out how much memory we'll need, and the malloc it
    PRUint16 maxLen;
    eFieldType fieldType;

    rv = GetFieldTypeAndLenForSort(sortType, &maxLen, &fieldType);
    NS_ENSURE_SUCCESS(rv,rv);

    nsVoidArray ptrs;
    PRUint32 arraySize = GetSize();

    if (!arraySize) {
       return NS_OK;
    } /* endif */

    nsCOMPtr <nsISupportsArray> folders;
    GetFolders(getter_AddRefs(folders));

    // use IdPRTime, it is the biggest
    IdPRTime** pPtrBase = (IdPRTime**)PR_Malloc(arraySize * sizeof(IdPRTime*));
    NS_ASSERTION(pPtrBase, "out of memory, can't sort");
    if (!pPtrBase) return NS_ERROR_OUT_OF_MEMORY;
    ptrs.AppendElement((void *)pPtrBase); // remember this pointer so we can free it later
    
    // build up the beast, so we can sort it.
    PRUint32 numSoFar = 0;
    // calc max possible size needed for all the rest
    PRUint32 maxSize = (PRUint32)(maxLen + sizeof(EntryInfo) + 1) * (PRUint32)(arraySize - numSoFar);

    PRUint32 maxBlockSize = (PRUint32) 0xf000L;
    PRUint32 allocSize = PR_MIN(maxBlockSize, maxSize);
    char *pTemp = (char *) PR_Malloc(allocSize);
    NS_ASSERTION(pTemp, "out of memory, can't sort");
    if (!pTemp) {   
        FreeAll(&ptrs);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    ptrs.AppendElement(pTemp); // remember this pointer so we can free it later

    char *pBase = pTemp;
    PRBool more = PR_TRUE;

    nsCOMPtr <nsIMsgDBHdr> msgHdr;
    PRUint8 *keyValue = nsnull;
    PRUint32 longValue;
    PRTime timeValue;
    while (more && numSoFar < arraySize) {
      nsMsgKey thisKey = m_keys.GetAt(numSoFar);
      if (sortType != nsMsgViewSortType::byId) {
        rv = GetMsgHdrForViewIndex(numSoFar, getter_AddRefs(msgHdr));
        NS_ASSERTION(NS_SUCCEEDED(rv) && msgHdr, "header not found");
        if (NS_FAILED(rv) || !msgHdr) {
          FreeAll(&ptrs);
          return NS_ERROR_UNEXPECTED;
        }
      }
      else {
        msgHdr = nsnull;
      }

      // could be a problem here if the ones that appear here are different than the ones already in the array
      void *pField = nsnull;
      PRUint32 actualFieldLen = 0;
      if (fieldType == kCollationKey) {
        rv = GetCollationKey(msgHdr, sortType, &keyValue, &actualFieldLen);
        NS_ENSURE_SUCCESS(rv,rv);

        pField = (void *) keyValue;
      }
      else if (fieldType == kPRTime) {
        rv = GetPRTimeField(msgHdr, sortType, &timeValue);
        NS_ENSURE_SUCCESS(rv,rv);

        pField = (void *) &timeValue;
        actualFieldLen = maxLen;
      }
      else {
        if (sortType == nsMsgViewSortType::byId) {
            longValue = thisKey;
        }
        else {
            rv = GetLongField(msgHdr, sortType, &longValue);
            NS_ENSURE_SUCCESS(rv,rv);
        }
        pField = (void *)&longValue;
        actualFieldLen = maxLen;
      }

      // check to see if this entry fits into the block we have allocated so far
      // pTemp - pBase = the space we have used so far
      // sizeof(EntryInfo) + fieldLen = space we need for this entry
      // allocSize = size of the current block
      if ((PRUint32)(pTemp - pBase) + (PRUint32)sizeof(EntryInfo) + (PRUint32)actualFieldLen >= allocSize) {
        maxSize = (PRUint32)(maxLen + sizeof(EntryInfo) + 1) * (PRUint32)(arraySize - numSoFar);
        maxBlockSize = (PRUint32) 0xf000L;
        allocSize = PR_MIN(maxBlockSize, maxSize);
        pTemp = (char *) PR_Malloc(allocSize);
        NS_ASSERTION(pTemp, "out of memory, can't sort");
        if (!pTemp) {
          FreeAll(&ptrs);
          return NS_ERROR_OUT_OF_MEMORY;
        }
        pBase = pTemp;
        ptrs.AppendElement(pTemp); // remember this pointer so we can free it later
      }

      // make sure there aren't more IDs than we allocated space for
      NS_ASSERTION(numSoFar < arraySize, "out of memory");
      if (numSoFar >= arraySize) {
        FreeAll(&ptrs);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      // now store this entry away in the allocated memory
      pPtrBase[numSoFar] = (IdPRTime*)pTemp;
      EntryInfo *info = (EntryInfo*)pTemp;
      info->id = thisKey;
      info->bits = m_flags.GetAt(numSoFar);
      info->len = actualFieldLen;
      //info->pad = 0;

    if (folders)
    {
        nsCOMPtr <nsISupports> tmpSupports 
            = getter_AddRefs(folders->ElementAt(numSoFar));
        nsCOMPtr<nsIMsgFolder> curFolder;
        if(tmpSupports) {
            curFolder = do_QueryInterface(tmpSupports);
            info->folder = curFolder;
        }
    }

      pTemp += sizeof(EntryInfo);

      PRInt32 bytesLeft = allocSize - (PRInt32)(pTemp - pBase);
      PRInt32 bytesToCopy = PR_MIN(bytesLeft, (PRInt32)actualFieldLen);
      if (pField && bytesToCopy > 0) {
        nsCRT::memcpy((void *)pTemp, pField, bytesToCopy);
        if (bytesToCopy < (PRInt32)actualFieldLen) {
          NS_ASSERTION(0, "wow, big block");
          info->len = bytesToCopy;
        }
      }
      else {
        *pTemp = 0;
      }
      //In order to align memory for systems that require it, such as HP-UX
      //calculate the correct value to pad the bytesToCopy value
      PRInt32 bytesToPad = sizeof(PRInt32) - (bytesToCopy & 0x00000003);

      //if bytesToPad is not 4 then alignment is needed so add the padding
      //otherwise memory is already aligned - no need to add padding
      if (bytesToPad != sizeof(PRInt32)) {
        //Add the necessary padding to bytesToCopy
        bytesToCopy += bytesToPad; 
      }

      pTemp += bytesToCopy;
      ++numSoFar;
      PR_FREEIF(keyValue);
    }

    // do the sort
    switch (fieldType) {
        case kCollationKey:
          {

            nsCOMPtr <nsIMsgDatabase> dbToUse = m_db;

            if (!dbToUse) // probably search view
              GetDBForViewIndex(0, getter_AddRefs(dbToUse));
            if (dbToUse)
              NS_QuickSort(pPtrBase, numSoFar, sizeof(IdKey*), FnSortIdKey, dbToUse);
          }
            break;
        case kU32:
            NS_QuickSort(pPtrBase, numSoFar, sizeof(IdDWord*), FnSortIdDWord, nsnull);
            break;
        case kPRTime:
            NS_QuickSort(pPtrBase, numSoFar, sizeof(IdPRTime*), FnSortIdPRTime, nsnull);
            break;
        default:
            NS_ASSERTION(0, "not supposed to get here");
            break;
    }

    // now put the IDs into the array in proper order
    for (PRUint32 i = 0; i < numSoFar; i++) {
        m_keys.SetAt(i, pPtrBase[i]->info.id);
        m_flags.SetAt(i, pPtrBase[i]->info.bits);

        if (folders)
        {
            nsCOMPtr <nsISupports> tmpSupports 
                = do_QueryInterface(pPtrBase[i]->info.folder);
            folders->SetElementAt(i, tmpSupports);
        }
    }

    m_sortType = sortType;
    m_sortOrder = sortOrder;

    if (sortOrder == nsMsgViewSortOrder::descending) {
        rv = ReverseSort();
        NS_ASSERTION(NS_SUCCEEDED(rv),"failed to reverse sort");
    }

    // free all the memory we allocated
    FreeAll(&ptrs);

    m_sortValid = PR_TRUE;
    //m_db->SetSortInfo(sortType, sortOrder);

    return NS_OK;
}

void nsMsgDBView::FreeAll(nsVoidArray *ptrs)
{
    PRInt32 i;
    PRInt32 count = (PRInt32) ptrs->Count();
    if (count == 0) return;

    for (i=(count - 1);i>=0;i--) {
        void *ptr = (void *) ptrs->ElementAt(i);
        PR_FREEIF(ptr);
        ptrs->RemoveElementAt(i);
    }
}

nsMsgViewIndex nsMsgDBView::GetIndexOfFirstDisplayedKeyInThread(nsIMsgThread *threadHdr)
{
	nsMsgViewIndex	retIndex = nsMsgViewIndex_None;
	PRUint32	childIndex = 0;
	// We could speed up the unreadOnly view by starting our search with the first
	// unread message in the thread. Sometimes, that will be wrong, however, so
	// let's skip it until we're sure it's neccessary.
//	(m_viewFlags & nsMsgViewFlagsType::kUnreadOnly) 
//		? threadHdr->GetFirstUnreadKey(m_db) : threadHdr->GetChildAt(0);
  PRUint32 numThreadChildren;
  threadHdr->GetNumChildren(&numThreadChildren);
	while (retIndex == nsMsgViewIndex_None && childIndex < numThreadChildren)
	{
		nsMsgKey childKey;
    threadHdr->GetChildKeyAt(childIndex++, &childKey);
		retIndex = FindViewIndex(childKey);
	}
	return retIndex;
}

// caller must referTo hdr if they want to hold it or change it!
nsresult nsMsgDBView::GetFirstMessageHdrToDisplayInThread(nsIMsgThread *threadHdr, nsIMsgDBHdr **result)
{
  nsresult rv;

	if (m_viewFlags & nsMsgViewFlagsType::kUnreadOnly)
		rv = threadHdr->GetFirstUnreadChild(result);
	else
		rv = threadHdr->GetChildHdrAt(0, result);
	return rv;
}

// Find the view index of the thread containing the passed msgKey, if
// the thread is in the view. MsgIndex is passed in as a shortcut if
// it turns out the msgKey is the first message in the thread,
// then we can avoid looking for the msgKey.
nsMsgViewIndex nsMsgDBView::ThreadIndexOfMsg(nsMsgKey msgKey, 
											  nsMsgViewIndex msgIndex /* = nsMsgViewIndex_None */,
											  PRInt32 *pThreadCount /* = NULL */,
											  PRUint32 *pFlags /* = NULL */)
{
	if (m_sortType != nsMsgViewSortType::byThread)
		return nsMsgViewIndex_None;
	nsCOMPtr <nsIMsgThread> threadHdr;
  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  nsresult rv = m_db->GetMsgHdrForKey(msgKey, getter_AddRefs(msgHdr));
  NS_ENSURE_SUCCESS(rv, nsMsgViewIndex_None);
  rv = m_db->GetThreadContainingMsgHdr(msgHdr, getter_AddRefs(threadHdr));
  NS_ENSURE_SUCCESS(rv, nsMsgViewIndex_None);

	nsMsgViewIndex retIndex = nsMsgViewIndex_None;

	if (threadHdr != nsnull)
	{
		if (msgIndex == nsMsgViewIndex_None)
			msgIndex = FindViewIndex(msgKey);

		if (msgIndex == nsMsgViewIndex_None)	// key is not in view, need to find by thread
		{
			msgIndex = GetIndexOfFirstDisplayedKeyInThread(threadHdr);
			//nsMsgKey		threadKey = (msgIndex == nsMsgViewIndex_None) ? nsMsgKey_None : GetAt(msgIndex);
			if (pFlags)
				threadHdr->GetFlags(pFlags);
		}
		nsMsgViewIndex startOfThread = msgIndex;
		while ((PRInt32) startOfThread >= 0 && m_levels[startOfThread] != 0)
			startOfThread--;
		retIndex = startOfThread;
		if (pThreadCount)
		{
			PRInt32 numChildren = 0;
			nsMsgViewIndex threadIndex = startOfThread;
			do
			{
				threadIndex++;
				numChildren++;
			}
			while ((int32) threadIndex < m_levels.GetSize() && m_levels[threadIndex] != 0);
			*pThreadCount = numChildren;
		}
	}
	return retIndex;
}

nsMsgKey nsMsgDBView::GetKeyOfFirstMsgInThread(nsMsgKey key)
{
	nsCOMPtr <nsIMsgThread> pThread;
	nsCOMPtr <nsIMsgDBHdr> msgHdr;
    nsresult rv = m_db->GetMsgHdrForKey(key, getter_AddRefs(msgHdr));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = m_db->GetThreadContainingMsgHdr(msgHdr, getter_AddRefs(pThread));
    NS_ENSURE_SUCCESS(rv, rv);
	nsMsgKey	firstKeyInThread = nsMsgKey_None;

	NS_ASSERTION(pThread, "error getting msg from thread");
	if (!pThread)
	{
		return firstKeyInThread;
	}
	// ### dmb UnreadOnly - this is wrong. But didn't seem to matter in 4.x
	pThread->GetChildKeyAt(0, &firstKeyInThread);
	return firstKeyInThread;
}

NS_IMETHODIMP nsMsgDBView::GetKeyAt(nsMsgViewIndex index, nsMsgKey *result)
{
  NS_ENSURE_ARG(result);
  *result = GetAt(index);
  return NS_OK;
}

nsMsgKey nsMsgDBView::GetAt(nsMsgViewIndex index) 
{
	if (index >= m_keys.GetSize() || index == nsMsgViewIndex_None)
		return nsMsgKey_None;
	else
		return(m_keys.GetAt(index));
}

nsMsgViewIndex	nsMsgDBView::FindKey(nsMsgKey key, PRBool expand)
{
	nsMsgViewIndex retIndex = nsMsgViewIndex_None;
	retIndex = (nsMsgViewIndex) (m_keys.FindIndex(key));
	if (key != nsMsgKey_None && retIndex == nsMsgViewIndex_None && expand && m_db)
	{
		nsMsgKey threadKey = GetKeyOfFirstMsgInThread(key);
		if (threadKey != nsMsgKey_None)
		{
			nsMsgViewIndex threadIndex = FindKey(threadKey, PR_FALSE);
			if (threadIndex != nsMsgViewIndex_None)
			{
				PRUint32 flags = m_flags[threadIndex];
				if ((flags & MSG_FLAG_ELIDED) && NS_SUCCEEDED(ExpandByIndex(threadIndex, nsnull)))
					retIndex = FindKey(key, PR_FALSE);
			}
		}
	}
	return retIndex;
}

nsresult nsMsgDBView::GetThreadCount(nsMsgKey messageKey, PRUint32 *pThreadCount)
{
	nsresult rv = NS_MSG_MESSAGE_NOT_FOUND;
	nsCOMPtr <nsIMsgDBHdr> msgHdr;
    rv = m_db->GetMsgHdrForKey(messageKey, getter_AddRefs(msgHdr));
    NS_ENSURE_SUCCESS(rv, rv);
	nsCOMPtr <nsIMsgThread> pThread;
    rv = m_db->GetThreadContainingMsgHdr(msgHdr, getter_AddRefs(pThread));
	if (NS_SUCCEEDED(rv) && pThread != nsnull)
    rv = pThread->GetNumChildren(pThreadCount);
	return rv;
}

// This counts the number of messages in an expanded thread, given the
// index of the first message in the thread.
PRInt32 nsMsgDBView::CountExpandedThread(nsMsgViewIndex index)
{
	PRInt32 numInThread = 0;
	nsMsgViewIndex startOfThread = index;
	while ((PRInt32) startOfThread >= 0 && m_levels[startOfThread] != 0)
		startOfThread--;
	nsMsgViewIndex threadIndex = startOfThread;
	do
	{
		threadIndex++;
		numInThread++;
	}
	while ((PRInt32) threadIndex < m_levels.GetSize() && m_levels[threadIndex] != 0);

	return numInThread;
}

// returns the number of lines that would be added (> 0) or removed (< 0) 
// if we were to try to expand/collapse the passed index.
nsresult nsMsgDBView::ExpansionDelta(nsMsgViewIndex index, PRInt32 *expansionDelta)
{
	PRUint32 numChildren;
	nsresult	rv;

	*expansionDelta = 0;
	if ( index > ((nsMsgViewIndex) m_keys.GetSize()))
		return NS_MSG_MESSAGE_NOT_FOUND;
	char	flags = m_flags[index];

	if (m_sortType != nsMsgViewSortType::byThread)
		return NS_OK;

	// The client can pass in the key of any message
	// in a thread and get the expansion delta for the thread.

	if (!(m_viewFlags & nsMsgViewFlagsType::kUnreadOnly))
	{
		rv = GetThreadCount(m_keys[index], &numChildren);
		NS_ENSURE_SUCCESS(rv, rv);
	}
	else
	{
		numChildren = CountExpandedThread(index);
	}

	if (flags & MSG_FLAG_ELIDED)
		*expansionDelta = numChildren - 1;
	else
		*expansionDelta = - (PRInt32) (numChildren - 1);

	return NS_OK;
}

nsresult nsMsgDBView::ToggleExpansion(nsMsgViewIndex index, PRUint32 *numChanged)
{
  NS_ENSURE_ARG(numChanged);
  *numChanged = 0;
	nsMsgViewIndex threadIndex = ThreadIndexOfMsg(GetAt(index), index);
	if (threadIndex == nsMsgViewIndex_None)
	{
		NS_ASSERTION(PR_FALSE, "couldn't find thread");
		return NS_MSG_MESSAGE_NOT_FOUND;
	}
	PRInt32	flags = m_flags[threadIndex];

	// if not a thread, or doesn't have children, no expand/collapse
	// If we add sub-thread expand collapse, this will need to be relaxed
	if (!(flags & MSG_VIEW_FLAG_ISTHREAD) || !(flags && MSG_VIEW_FLAG_HASCHILDREN))
		return NS_MSG_MESSAGE_NOT_FOUND;
	if (flags & MSG_FLAG_ELIDED)
		return ExpandByIndex(threadIndex, numChanged);
	else
		return CollapseByIndex(threadIndex, numChanged);

}

nsresult nsMsgDBView::ExpandAndSelectThread()
{
    nsresult rv;

    NS_ASSERTION(mOutlinerSelection, "no outliner selection");
    if (!mOutlinerSelection) return NS_ERROR_UNEXPECTED;

    PRInt32 index;
    rv = mOutlinerSelection->GetCurrentIndex(&index);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = ExpandAndSelectThreadByIndex(index);
    NS_ENSURE_SUCCESS(rv,rv);
    return NS_OK;
}

nsresult nsMsgDBView::ExpandAndSelectThreadByIndex(nsMsgViewIndex index)
{
  nsresult rv;

  nsMsgViewIndex threadIndex;
  PRBool inThreadedMode = (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay);
  
  if (inThreadedMode) {
    threadIndex = ThreadIndexOfMsg(GetAt(index), index);
    if (threadIndex == nsMsgViewIndex_None) {
      NS_ASSERTION(PR_FALSE, "couldn't find thread");
      return NS_MSG_MESSAGE_NOT_FOUND;
    }
  }
  else {
    threadIndex = index;
  }

  PRInt32 flags = m_flags[threadIndex];
  PRInt32 count = 0;

  if (inThreadedMode && (flags & MSG_VIEW_FLAG_ISTHREAD) && (flags && MSG_VIEW_FLAG_HASCHILDREN)) {
    // if closed, expand this thread.
    if (flags & MSG_FLAG_ELIDED) {
      PRUint32 numExpanded;
      rv = ExpandByIndex(threadIndex, &numExpanded);
      NS_ENSURE_SUCCESS(rv,rv);
    }

    // get the number of messages in the expanded thread
    // so we know how many to select
    count = CountExpandedThread(threadIndex); 
  }
  else {
    count = 1;
  }
  NS_ASSERTION(count > 0, "bad count");

  // update the selection

  NS_ASSERTION(mOutlinerSelection, "no outliner selection");
  if (!mOutlinerSelection) return NS_ERROR_UNEXPECTED;

  // clear the existing selection.
  mOutlinerSelection->ClearSelection(); 

  // is this correct when we are selecting multiple items?
  mOutlinerSelection->SetCurrentIndex(threadIndex); 

  // the count should be 1 or greater. if there was only one message in the thread, we just select it.
  // if more, we select all of them.
  mOutlinerSelection->RangedSelect(threadIndex, threadIndex + count - 1, PR_TRUE /* augment */);

  if (count == 1) {
    // if we ended up selecting on message, this will cause us to load it.
    SelectionChanged();
  }
  else {
    //XXX todo, should multiple selection clear the thread pane?  see bug #xxxxx
    //do we want to do something like this to clear the message pane
    //when the user selects a thread?
    //NoteChange(?, ?, nsMsgViewNotificationCode::clearMessagePane);
  }
  return NS_OK;
}

nsresult nsMsgDBView::ExpandAll()
{
	for (PRInt32 i = GetSize() - 1; i >= 0; i--) 
	{
		PRUint32 numExpanded;
		PRUint32 flags = m_flags[i];
		if (flags & MSG_FLAG_ELIDED)
			ExpandByIndex(i, &numExpanded);
	}
	return NS_OK;
}

nsresult nsMsgDBView::ExpandByIndex(nsMsgViewIndex index, PRUint32 *pNumExpanded)
{
	PRUint32			flags = m_flags[index];
	nsMsgKey		firstIdInThread;
    //nsMsgKey        startMsg = nsMsgKey_None;
	nsresult		rv = NS_OK;
	PRUint32			numExpanded = 0;

	NS_ASSERTION(flags & MSG_FLAG_ELIDED, "can't expand an already expanded thread");
	flags &= ~MSG_FLAG_ELIDED;

	if ((PRUint32) index > m_keys.GetSize())
		return NS_MSG_MESSAGE_NOT_FOUND;

	firstIdInThread = m_keys[index];
	nsCOMPtr <nsIMsgDBHdr> msgHdr;
  nsCOMPtr <nsIMsgThread> pThread;
    m_db->GetMsgHdrForKey(firstIdInThread, getter_AddRefs(msgHdr));
	if (msgHdr == nsnull)
	{
		NS_ASSERTION(PR_FALSE, "couldn't find message to expand");
		return NS_MSG_MESSAGE_NOT_FOUND;
	}
  rv = m_db->GetThreadContainingMsgHdr(msgHdr, getter_AddRefs(pThread));
	m_flags[index] = flags;
  NoteChange(index, 1, nsMsgViewNotificationCode::changed);
	if (m_viewFlags & nsMsgViewFlagsType::kUnreadOnly)
	{
		if (flags & MSG_FLAG_READ)
			m_levels.Add(0);	// keep top level hdr in thread, even though read.
		rv = ListUnreadIdsInThread(pThread,  index, &numExpanded);
	}
	else
		rv = ListIdsInThread(pThread,  index, &numExpanded);

	NoteStartChange(index + 1, numExpanded, nsMsgViewNotificationCode::insertOrDelete);

  NoteEndChange(index + 1, numExpanded, nsMsgViewNotificationCode::insertOrDelete);
	if (pNumExpanded != nsnull)
		*pNumExpanded = numExpanded;
	return rv;
}

nsresult nsMsgDBView::CollapseAll()
{
    for (PRInt32 i = 0; i < GetSize(); i++)
    {
        PRUint32 numExpanded;
        PRUint32 flags = m_flags[i];
        if (!(flags & MSG_FLAG_ELIDED) && (flags & MSG_VIEW_FLAG_HASCHILDREN))
            CollapseByIndex(i, &numExpanded);
    }
    return NS_OK;
}

nsresult nsMsgDBView::CollapseByIndex(nsMsgViewIndex index, PRUint32 *pNumCollapsed)
{
	nsMsgKey		firstIdInThread;
	nsresult	rv;
	PRInt32	flags = m_flags[index];
	PRInt32	threadCount = 0;

	if (flags & MSG_FLAG_ELIDED || m_sortType != nsMsgViewSortType::byThread || !(flags & MSG_VIEW_FLAG_HASCHILDREN))
		return NS_OK;
	flags  |= MSG_FLAG_ELIDED;

	if (index > m_keys.GetSize())
		return NS_MSG_MESSAGE_NOT_FOUND;

	firstIdInThread = m_keys[index];
	nsCOMPtr <nsIMsgDBHdr> msgHdr;
  rv = m_db->GetMsgHdrForKey(firstIdInThread, getter_AddRefs(msgHdr));
	if (!NS_SUCCEEDED(rv) || msgHdr == nsnull)
	{
		NS_ASSERTION(PR_FALSE, "error collapsing thread");
		return NS_MSG_MESSAGE_NOT_FOUND;
	}

	m_flags[index] = flags;
	NoteChange(index, 1, nsMsgViewNotificationCode::changed);

	rv = ExpansionDelta(index, &threadCount);
	if (NS_SUCCEEDED(rv))
	{
		PRInt32 numRemoved = threadCount; // don't count first header in thread
    NoteStartChange(index + 1, -numRemoved, nsMsgViewNotificationCode::insertOrDelete);
		// start at first id after thread.
		for (int i = 1; i <= threadCount && index + 1 < m_keys.GetSize(); i++)
		{
			m_keys.RemoveAt(index + 1);
			m_flags.RemoveAt(index + 1);
			m_levels.RemoveAt(index + 1);
		}
		if (pNumCollapsed != nsnull)
			*pNumCollapsed = numRemoved;	
		NoteEndChange(index + 1, -numRemoved, nsMsgViewNotificationCode::insertOrDelete);
	}
	return rv;
}

nsresult nsMsgDBView::OnNewHeader(nsMsgKey newKey, nsMsgKey aParentKey, PRBool /*ensureListed*/)
{
	nsresult rv = NS_MSG_MESSAGE_NOT_FOUND;;
	// views can override this behaviour, which is to append to view.
	// This is the mail behaviour, but threaded views will want
	// to insert in order...
	nsCOMPtr <nsIMsgDBHdr> msgHdr;
    NS_ASSERTION(m_db, "m_db is null");
    if (m_db)
      rv = m_db->GetMsgHdrForKey(newKey, getter_AddRefs(msgHdr));
	if (NS_SUCCEEDED(rv) && msgHdr != nsnull)
	{
		rv = AddHdr(msgHdr);
	}
	return rv;
}

nsresult nsMsgDBView::GetThreadContainingIndex(nsMsgViewIndex index, nsIMsgThread **resultThread)
{
  nsCOMPtr <nsIMsgDBHdr> msgHdr;

  NS_ENSURE_TRUE(m_db, NS_ERROR_NULL_POINTER);

  nsresult rv = m_db->GetMsgHdrForKey(m_keys[index], getter_AddRefs(msgHdr));
  NS_ENSURE_SUCCESS(rv, rv);
  return m_db->GetThreadContainingMsgHdr(msgHdr, resultThread);
}

nsMsgViewIndex nsMsgDBView::GetIndexForThread(nsIMsgDBHdr *hdr)
{
  nsMsgViewIndex retIndex = nsMsgViewIndex_None;
  nsMsgViewIndex prevInsertIndex = nsMsgViewIndex_None;
  nsMsgKey insertKey;
  hdr->GetMessageKey(&insertKey);
  
  if (m_sortOrder == nsMsgViewSortOrder::ascending)
  {
    // loop backwards looking for top level message with id > id of header we're inserting 
    // and put new header before found header, or at end.
    for (PRInt32 i = GetSize() - 1; i >= 0; i--) 
    {
      if (m_levels[i] == 0)
      {
        if (insertKey < m_keys.GetAt(i))
          prevInsertIndex = i;
        else if (insertKey >= m_keys.GetAt(i))
        {
          retIndex = (prevInsertIndex == nsMsgViewIndex_None) ? nsMsgViewIndex_None : i + 1;
          if (prevInsertIndex == nsMsgViewIndex_None)
          {
            retIndex = nsMsgViewIndex_None;
          }
          else
          {
            for (retIndex = i + 1; retIndex < (nsMsgViewIndex)GetSize(); retIndex++)
            {
              if (m_levels[retIndex] == 0)
                break;
            }
          }
          break;
        }
        
      }
    }
  }
  else
  {
    // loop forwards looking for top level message with id < id of header we're inserting and put 
    // new header before found header, or at beginning.
    for (PRInt32 i = 0; i < GetSize(); i++) 
    {
      if (!m_levels[i])
      {
        if (insertKey > m_keys.GetAt(i))
        {
          retIndex = i;
          break;
        }
      }
    }
  }
  return retIndex;
}


nsMsgViewIndex nsMsgDBView::GetInsertIndex(nsIMsgDBHdr *msgHdr)
{
  PRBool done = PR_FALSE;
  PRBool withinOne = PR_FALSE;
  nsMsgViewIndex retIndex = nsMsgViewIndex_None;
  nsMsgViewIndex tryIndex = GetSize() / 2;
  nsMsgViewIndex newTryIndex;
  nsMsgViewIndex lowIndex = 0;
  nsMsgViewIndex highIndex = GetSize() - 1;
  IdDWord	dWordEntryInfo1, dWordEntryInfo2;
  IdKeyPtr	keyInfo1, keyInfo2;
  IdPRTime timeInfo1, timeInfo2;
  void *comparisonContext = nsnull;
  
  nsresult rv;
  
  if (GetSize() == 0)
    return 0;
  
  PRUint16	maxLen;
  eFieldType fieldType;
  rv = GetFieldTypeAndLenForSort(m_sortType, &maxLen, &fieldType);
  const void *pValue1, *pValue2;
  
  if ((m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay) != 0)
  {
    retIndex = GetIndexForThread(msgHdr);
    return retIndex;
  }
  
  int (* PR_CALLBACK comparisonFun) (const void *pItem1, const void *pItem2, void *privateData)=nsnull;
  int retStatus = 0;
  switch (fieldType)
  {
    case kCollationKey:
      rv = GetCollationKey(msgHdr, m_sortType, &(keyInfo1.key), &(keyInfo1.info.len));
      NS_ASSERTION(NS_SUCCEEDED(rv),"failed to create collation key");
      msgHdr->GetMessageKey(&keyInfo1.info.id);
      comparisonFun = FnSortIdKeyPtr;
      comparisonContext = m_db.get();
      pValue1 = (void *) &keyInfo1;
      break;
    case kU32:
      if (m_sortType == nsMsgViewSortType::byId) {
        msgHdr->GetMessageKey(&dWordEntryInfo1.dword);
      }
      else {
        GetLongField(msgHdr, m_sortType, &dWordEntryInfo1.dword);
      }
      msgHdr->GetMessageKey(&dWordEntryInfo1.info.id);
      pValue1 = (void *) &dWordEntryInfo1;
      comparisonFun = FnSortIdDWord;
      break;
    case kPRTime:
      rv = GetPRTimeField(msgHdr, m_sortType, &timeInfo1.prtime);
      msgHdr->GetMessageKey(&timeInfo1.info.id);
      NS_ENSURE_SUCCESS(rv,rv);
      comparisonFun = FnSortIdPRTime;
      pValue1 = (void *) &timeInfo1;
      break;
    default:
      done = PR_TRUE;
  }
  while (!done)
  {
    if (highIndex == lowIndex)
      break;
    nsMsgKey	messageKey = GetAt(tryIndex);
    nsCOMPtr <nsIMsgDBHdr> tryHdr;
    rv = m_db->GetMsgHdrForKey(messageKey, getter_AddRefs(tryHdr));
    if (!tryHdr)
      break;
    if (fieldType == kCollationKey)
    {
      rv = GetCollationKey(tryHdr, m_sortType, &(keyInfo2.key), &(keyInfo2.info.len));
      NS_ASSERTION(NS_SUCCEEDED(rv),"failed to create collation key");
      keyInfo2.info.id = messageKey;
      pValue2 = &keyInfo2;
    }
    else if (fieldType == kU32)
    {
      if (m_sortType == nsMsgViewSortType::byId) {
        dWordEntryInfo2.dword = messageKey;
      }
      else {
        GetLongField(tryHdr, m_sortType, &dWordEntryInfo2.dword);
      }
      dWordEntryInfo2.info.id = messageKey;
      pValue2 = &dWordEntryInfo2;
    }
    else if (fieldType == kPRTime)
    {
      GetPRTimeField(tryHdr, m_sortType, &timeInfo2.prtime);
      timeInfo2.info.id = messageKey;
      pValue2 = &timeInfo2;
    }
    retStatus = (*comparisonFun)(&pValue1, &pValue2, comparisonContext);
    if (retStatus == 0)
      break;
    if (m_sortOrder == nsMsgViewSortOrder::descending)	//switch retStatus based on sort order
      retStatus = (retStatus > 0) ? -1 : 1;
    
    if (retStatus < 0)
    {
      newTryIndex = tryIndex  - (tryIndex - lowIndex) / 2;
      if (newTryIndex == tryIndex)
      {
        if (!withinOne && newTryIndex > lowIndex)
        {
          newTryIndex--;
          withinOne = PR_TRUE;
        }
      }
      highIndex = tryIndex;
    }
    else
    {
      newTryIndex = tryIndex + (highIndex - tryIndex) / 2;
      if (newTryIndex == tryIndex)
      {
        if (!withinOne && newTryIndex < highIndex)
        {
          withinOne = PR_TRUE;
          newTryIndex++;
        }
        lowIndex = tryIndex;
      }
    }
    if (tryIndex == newTryIndex)
      break;
    else
      tryIndex = newTryIndex;
  }
  if (retStatus >= 0)
    retIndex = tryIndex + 1;
  else if (retStatus < 0)
    retIndex = tryIndex;
  
  return retIndex;
}

nsresult	nsMsgDBView::AddHdr(nsIMsgDBHdr *msgHdr)
{
  PRUint32	flags = 0;
#ifdef DEBUG_bienvenu
  NS_ASSERTION((int) m_keys.GetSize() == m_flags.GetSize() && (int) m_keys.GetSize() == m_levels.GetSize(), "view arrays out of sync!");
#endif
  msgHdr->GetFlags(&flags);
  if (flags & MSG_FLAG_IGNORED && !GetShowingIgnored())
    return NS_OK;

  nsMsgKey msgKey, threadId;
  nsMsgKey threadParent;
  msgHdr->GetMessageKey(&msgKey);
  msgHdr->GetThreadId(&threadId);
  msgHdr->GetThreadParent(&threadParent);

  // ### this isn't quite right, is it? Should be checking that our thread parent key is none?
  if (threadParent == nsMsgKey_None) 
    flags |= MSG_VIEW_FLAG_ISTHREAD;
  nsMsgViewIndex insertIndex = GetInsertIndex(msgHdr);
  if (insertIndex == nsMsgViewIndex_None)
  {
	// if unreadonly, level is 0 because we must be the only msg in the thread.
    PRInt32 levelToAdd = 0;
#if 0 
    if (!(m_viewFlags & nsMsgViewFlagsType::kUnreadOnly)) 
    {
        levelToAdd = FindLevelInThread(msgHdr, insertIndex);
    }
#endif

    if (m_sortOrder == nsMsgViewSortOrder::ascending)
	{
	  m_keys.Add(msgKey);
	  m_flags.Add(flags);
      m_levels.Add(levelToAdd);
      NoteChange(m_keys.GetSize() - 1, 1, nsMsgViewNotificationCode::insertOrDelete);
	}
	else
	{
      m_keys.InsertAt(0, msgKey);
      m_flags.InsertAt(0, flags);
      m_levels.InsertAt(0, levelToAdd);
      NoteChange(0, 1, nsMsgViewNotificationCode::insertOrDelete);
	}
	m_sortValid = PR_FALSE;
  }
  else
  {
    m_keys.InsertAt(insertIndex, msgKey);
    m_flags.InsertAt(insertIndex, flags);
    PRInt32 level = 0; 
#if 0 
    if (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay)
    {
        level = FindLevelInThread(msgHdr, insertIndex);
    }
#endif
    m_levels.InsertAt(insertIndex, level);
    NoteChange(insertIndex, 1, nsMsgViewNotificationCode::insertOrDelete);
  }
  OnHeaderAddedOrDeleted();
  return NS_OK;
}

PRBool nsMsgDBView::WantsThisThread(nsIMsgThread * /*threadHdr*/)
{
  return PR_TRUE; // default is to want all threads.
}

PRInt32 nsMsgDBView::FindLevelInThread(nsIMsgDBHdr *msgHdr, nsMsgViewIndex startOfThreadViewIndex)
{
  nsMsgKey threadParent;
  msgHdr->GetThreadParent(&threadParent);
  nsMsgViewIndex parentIndex = m_keys.FindIndex(threadParent, startOfThreadViewIndex);
  if (parentIndex != nsMsgViewIndex_None)
    return m_levels[parentIndex] + 1;
  else
  {
#ifdef DEBUG_bienvenu1
    NS_ASSERTION(PR_FALSE, "couldn't find parent of msg");
#endif
    nsMsgKey msgKey;
    msgHdr->GetMessageKey(&msgKey);
    // check if this is a newly promoted thread root
    if (threadParent == nsMsgKey_None || threadParent == msgKey)
      return 0;
    else
      return 1; // well, return level 1.
  }
}

nsresult nsMsgDBView::ListIdsInThreadOrder(nsIMsgThread *threadHdr, nsMsgKey parentKey, PRInt32 level, nsMsgViewIndex *viewIndex, PRUint32 *pNumListed)
{
  nsresult rv = NS_OK;
  nsCOMPtr <nsISimpleEnumerator> msgEnumerator;
  threadHdr->EnumerateMessages(parentKey, getter_AddRefs(msgEnumerator));
  PRUint32 numChildren;
  (void) threadHdr->GetNumChildren(&numChildren);

  // skip the first one.
  PRBool hasMore;
  nsCOMPtr <nsISupports> supports;
  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  while (NS_SUCCEEDED(rv) && NS_SUCCEEDED(rv = msgEnumerator->HasMoreElements(&hasMore)) && hasMore)
  {
    rv = msgEnumerator->GetNext(getter_AddRefs(supports));
    if (NS_SUCCEEDED(rv) && supports)
    {
      msgHdr = do_QueryInterface(supports);
      nsMsgKey msgKey;
      PRUint32 msgFlags, newFlags;
      msgHdr->GetMessageKey(&msgKey);
      msgHdr->GetFlags(&msgFlags);
      AdjustReadFlag(msgHdr, &msgFlags);
      m_keys.InsertAt(*viewIndex, msgKey);
      // ### TODO - how about hasChildren flag?
      m_flags.InsertAt(*viewIndex, msgFlags & ~MSG_VIEW_FLAGS);
      // ### TODO this is going to be tricky - might use enumerators
      m_levels.InsertAt(*viewIndex, level); 
      // turn off thread or elided bit if they got turned on (maybe from new only view?)
      msgHdr->AndFlags(~(MSG_VIEW_FLAG_ISTHREAD | MSG_FLAG_ELIDED), &newFlags);
      (*pNumListed)++;
      (*viewIndex)++;
      if (*pNumListed > numChildren)
      {
        NS_ASSERTION(PR_FALSE, "thread corrupt in db");
        // if we've listed more messages than are in the thread, then the db
        // is corrupt, and we should invalidate it.
        // we'll use this rv to indicate there's something wrong with the db
        // though for now it probably won't get paid attention to.
        m_db->SetSummaryValid(PR_FALSE);
        rv = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
        break;
      }
      rv = ListIdsInThreadOrder(threadHdr, msgKey, level + 1, viewIndex, pNumListed);
    }
  }
  return rv; // we don't want to return the rv from the enumerator when it reaches the end, do we?
}

nsresult	nsMsgDBView::ListIdsInThread(nsIMsgThread *threadHdr, nsMsgViewIndex startOfThreadViewIndex, PRUint32 *pNumListed)
{
  NS_ENSURE_ARG(threadHdr);
  // these children ids should be in thread order.
  PRUint32 i;
  nsMsgViewIndex viewIndex = startOfThreadViewIndex + 1;
  *pNumListed = 0;

  if (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay)
  {
    nsMsgKey parentKey = m_keys[startOfThreadViewIndex];

    return ListIdsInThreadOrder(threadHdr, parentKey, 1, &viewIndex, pNumListed);
  }
  // if we're not threaded, just list em out in db order

  PRUint32 numChildren;
  threadHdr->GetNumChildren(&numChildren);
  for (i = 1; i < numChildren; i++)
  {
    nsCOMPtr <nsIMsgDBHdr> msgHdr;
    threadHdr->GetChildHdrAt(i, getter_AddRefs(msgHdr));
    if (msgHdr != nsnull)
    {
      nsMsgKey msgKey;
      PRUint32 msgFlags, newFlags;
      msgHdr->GetMessageKey(&msgKey);
      msgHdr->GetFlags(&msgFlags);
      AdjustReadFlag(msgHdr, &msgFlags);
      m_keys.InsertAt(viewIndex, msgKey);
      // ### TODO - how about hasChildren flag?
      m_flags.InsertAt(viewIndex, msgFlags & ~MSG_VIEW_FLAGS);
      // ### TODO this is going to be tricky - might use enumerators
      PRInt32 level = FindLevelInThread(msgHdr, startOfThreadViewIndex);
      m_levels.InsertAt(viewIndex, level); 
      // turn off thread or elided bit if they got turned on (maybe from new only view?)
      if (i > 0)	
        msgHdr->AndFlags(~(MSG_VIEW_FLAG_ISTHREAD | MSG_FLAG_ELIDED), &newFlags);
      (*pNumListed)++;
      viewIndex++;
    }
  }
  return NS_OK;
}

PRInt32 nsMsgDBView::GetLevelInUnreadView(nsIMsgDBHdr *msgHdr, nsMsgViewIndex startOfThread, nsMsgViewIndex viewIndex)
{
  PRBool done = PR_FALSE;
  PRInt32 levelToAdd = 1;
  nsCOMPtr <nsIMsgDBHdr> curMsgHdr = msgHdr;

  // look through the ancestors of the passed in msgHdr in turn, looking for them in the view, up to the start of
  // the thread. If we find an ancestor, then our level is one greater than the level of the ancestor.
  while (!done)
  {
    nsMsgKey parentKey;
    curMsgHdr->GetThreadParent(&parentKey);
    if (parentKey != nsMsgKey_None)
    {
      nsMsgViewIndex parentIndexInView = nsMsgViewIndex_None;
        // scan up to find view index of ancestor, if any
      for (nsMsgViewIndex indexToTry = viewIndex - 1; indexToTry >= startOfThread; indexToTry--)
      {
        if (m_keys[indexToTry] == parentKey)
        {
          parentIndexInView = indexToTry;
          break;
        }
        if (indexToTry == 0)
          break;
      }
      if (parentIndexInView != nsMsgViewIndex_None)
      {
        levelToAdd = m_levels[parentIndexInView] + 1;
        break;
      }
    }
    else
      break;
    m_db->GetMsgHdrForKey(parentKey, getter_AddRefs(curMsgHdr));
    if (curMsgHdr == nsnull)
      break;
  }
  return levelToAdd;
}

nsresult	nsMsgDBView::ListUnreadIdsInThread(nsIMsgThread *threadHdr, nsMsgViewIndex startOfThreadViewIndex, PRUint32 *pNumListed)
{
  NS_ENSURE_ARG(threadHdr);
	// these children ids should be in thread order.
  nsMsgViewIndex viewIndex = startOfThreadViewIndex + 1;
	*pNumListed = 0;
  nsUint8Array levelStack;
  nsMsgKey topLevelMsgKey = m_keys[startOfThreadViewIndex];

  PRUint32 numChildren;
  threadHdr->GetNumChildren(&numChildren);
	PRUint32 i;
	for (i = 0; i < numChildren; i++)
	{
		nsCOMPtr <nsIMsgDBHdr> msgHdr;
    threadHdr->GetChildHdrAt(i, getter_AddRefs(msgHdr));
		if (msgHdr != nsnull)
		{
      nsMsgKey msgKey;
      PRUint32 msgFlags;
      msgHdr->GetMessageKey(&msgKey);
      msgHdr->GetFlags(&msgFlags);
      PRBool isRead = AdjustReadFlag(msgHdr, &msgFlags);
      // determining the level is going to be tricky, since we're not storing the
      // level in the db anymore. It will mean looking up the view for each of the
      // ancestors of the current msg until we find one in the view. I guess we could
      // check each ancestor to see if it's unread before looking for it in the view.
#if 0
			// if the current header's level is <= to the top of the level stack,
			// pop off the top of the stack.
			while (levelStack.GetSize() > 1 && 
						msgHdr->GetLevel()  <= levelStack.GetAt(levelStack.GetSize() - 1))
			{
				levelStack.RemoveAt(levelStack.GetSize() - 1);
			}
#endif
			if (!isRead)
			{
				PRUint8 levelToAdd;
				// just make sure flag is right in db.
				m_db->MarkHdrRead(msgHdr, PR_FALSE, nsnull);
        if (msgKey != topLevelMsgKey)
        {
				  m_keys.InsertAt(viewIndex, msgKey);
          m_flags.InsertAt(viewIndex, msgFlags);
  //					pLevels[i] = msgHdr->GetLevel();
          levelToAdd = GetLevelInUnreadView(msgHdr, startOfThreadViewIndex, viewIndex);
          m_levels.InsertAt(viewIndex, levelToAdd);
#ifdef DEBUG_bienvenu
//					XP_Trace("added at level %d\n", levelToAdd);
#endif
				  levelStack.Add(levelToAdd);
          viewIndex++;
				  (*pNumListed)++;
        }
			}
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::OnKeyChange(nsMsgKey aKeyChanged, PRUint32 aOldFlags, 
                                       PRUint32 aNewFlags, nsIDBChangeListener *aInstigator)
{
  // if we're not the instigator, update flags if this key is in our view
  if (aInstigator != this)
  {
    nsMsgViewIndex index = FindViewIndex(aKeyChanged);
    if (index != nsMsgViewIndex_None)
    {
      PRUint32 viewOnlyFlags = m_flags[index] & (MSG_VIEW_FLAGS | MSG_FLAG_ELIDED);

      // ### what about saving the old view only flags, like IsThread and HasChildren?
      // I think we'll want to save those away.
      m_flags[index] = aNewFlags | viewOnlyFlags;
      // tell the view the extra flag changed, so it can
      // update the previous view, if any.
      OnExtraFlagChanged(index, aNewFlags);
      NoteChange(index, 1, nsMsgViewNotificationCode::changed);
      PRUint32 deltaFlags = (aOldFlags ^ aNewFlags);
      if (deltaFlags & (MSG_FLAG_READ | MSG_FLAG_NEW))
      {
        nsMsgViewIndex threadIndex = ThreadIndexOfMsg(aKeyChanged);
        // may need to fix thread counts
        if (threadIndex != nsMsgViewIndex_None)
          NoteChange(threadIndex, 1, nsMsgViewNotificationCode::changed);
      }
    }
  }
  // don't need to propagate notifications, right?
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::OnKeyDeleted(nsMsgKey aKeyChanged, nsMsgKey aParentKey, PRInt32 aFlags, 
                            nsIDBChangeListener *aInstigator)
{
  nsMsgViewIndex deletedIndex = m_keys.FindIndex(aKeyChanged);
  if (deletedIndex != nsMsgViewIndex_None)
    RemoveByIndex(deletedIndex);

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::OnKeyAdded(nsMsgKey aKeyChanged, nsMsgKey aParentKey, PRInt32 aFlags, 
                          nsIDBChangeListener *aInstigator)
{
  return OnNewHeader(aKeyChanged, aParentKey, PR_FALSE); 
  // probably also want to pass that parent key in, since we went to the trouble
  // of figuring out what it is.
}
                          
NS_IMETHODIMP nsMsgDBView::OnParentChanged (nsMsgKey aKeyChanged, nsMsgKey oldParent, nsMsgKey newParent, nsIDBChangeListener *aInstigator)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::OnAnnouncerGoingAway(nsIDBChangeAnnouncer *instigator)
{
  if (m_db)
  {
    m_db->RemoveListener(this);
    m_db = nsnull;
  }

  ClearHdrCache();

  // this is important, because the outliner will ask us for our
  // row count, which get determine from the number of keys.
  m_keys.RemoveAll();
  // be consistent
  m_flags.RemoveAll();
  m_levels.RemoveAll();

  // clear the existing selection.
  if (mOutlinerSelection) {
    mOutlinerSelection->ClearSelection(); 
  }

  // this will force the outliner to ask for the cell values
  // since we don't have a db and we don't have any keys, 
  // the thread pane goes blank
  if (mOutliner) mOutliner->Invalidate();

  return NS_OK;
}

    
NS_IMETHODIMP nsMsgDBView::OnReadChanged(nsIDBChangeListener *instigator)
{
  return NS_OK;
}

void nsMsgDBView::ClearHdrCache()
{
  m_cachedHdr = nsnull;
  m_cachedMsgKey = nsMsgKey_None;
}

void	nsMsgDBView::EnableChangeUpdates()
{
}
void	nsMsgDBView::DisableChangeUpdates()
{
}
void	nsMsgDBView::NoteChange(nsMsgViewIndex firstLineChanged, PRInt32 numChanged, 
							 nsMsgViewNotificationCodeValue changeType)
{
  if (mOutliner)
  {
    switch (changeType)
    {
    case nsMsgViewNotificationCode::changed:
      mOutliner->InvalidateRange(firstLineChanged, firstLineChanged + numChanged - 1);
      break;
    case nsMsgViewNotificationCode::insertOrDelete:
      if (numChanged < 0)
        mRemovingRow = PR_TRUE;
      mOutliner->RowCountChanged(firstLineChanged, numChanged);
      mRemovingRow = PR_FALSE;
    case nsMsgViewNotificationCode::all:
      ClearHdrCache();
      break;
    }
  }
}

void	nsMsgDBView::NoteStartChange(nsMsgViewIndex firstlineChanged, PRInt32 numChanged, 
							 nsMsgViewNotificationCodeValue changeType)
{
}
void	nsMsgDBView::NoteEndChange(nsMsgViewIndex firstlineChanged, PRInt32 numChanged, 
							 nsMsgViewNotificationCodeValue changeType)
{
  // send the notification now.
  NoteChange(firstlineChanged, numChanged, changeType);
}

NS_IMETHODIMP nsMsgDBView::GetSortOrder(nsMsgViewSortOrderValue *aSortOrder)
{
    NS_ENSURE_ARG_POINTER(aSortOrder);
    *aSortOrder = m_sortOrder;
    return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::GetSortType(nsMsgViewSortTypeValue *aSortType)
{
    NS_ENSURE_ARG_POINTER(aSortType);
    *aSortType = m_sortType;
    return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::GetViewType(nsMsgViewTypeValue *aViewType)
{
    NS_ASSERTION(0,"you should be overriding this\n");
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsMsgDBView::GetViewFlags(nsMsgViewFlagsTypeValue *aViewFlags)
{
    NS_ENSURE_ARG_POINTER(aViewFlags);
    *aViewFlags = m_viewFlags;
    return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::SetViewFlags(nsMsgViewFlagsTypeValue aViewFlags)
{
    m_viewFlags = aViewFlags;
    return NS_OK;
}

nsresult nsMsgDBView::MarkThreadOfMsgRead(nsMsgKey msgId, nsMsgViewIndex msgIndex, nsMsgKeyArray &idsMarkedRead, PRBool bRead)
{
    nsCOMPtr <nsIMsgThread> threadHdr;
    nsresult rv = GetThreadContainingIndex(msgIndex, getter_AddRefs(threadHdr));
    NS_ENSURE_SUCCESS(rv, rv);

    nsMsgViewIndex threadIndex;

    NS_ASSERTION(threadHdr, "threadHdr is null");
    if (!threadHdr) {
        return NS_MSG_MESSAGE_NOT_FOUND;
    }
    nsCOMPtr <nsIMsgDBHdr> firstHdr;
    threadHdr->GetChildAt(0, getter_AddRefs(firstHdr));
    nsMsgKey firstHdrId;
    firstHdr->GetMessageKey(&firstHdrId);
    if (msgId != firstHdrId)
        threadIndex = GetIndexOfFirstDisplayedKeyInThread(threadHdr);
    else
        threadIndex = msgIndex;
    rv = MarkThreadRead(threadHdr, threadIndex, idsMarkedRead, bRead);
    return rv;
}

nsresult nsMsgDBView::MarkThreadRead(nsIMsgThread *threadHdr, nsMsgViewIndex threadIndex, nsMsgKeyArray &idsMarkedRead, PRBool bRead)
{
    PRBool threadElided = PR_TRUE;
    if (threadIndex != nsMsgViewIndex_None)
        threadElided = (m_flags.GetAt(threadIndex) & MSG_FLAG_ELIDED);

    PRUint32 numChildren;
    threadHdr->GetNumChildren(&numChildren);
    for (PRInt32 childIndex = 0; childIndex < (PRInt32) numChildren ; childIndex++) {
        nsCOMPtr <nsIMsgDBHdr> msgHdr;
        threadHdr->GetChildHdrAt(childIndex, getter_AddRefs(msgHdr));
        NS_ASSERTION(msgHdr, "msgHdr is null");
        if (!msgHdr) {
            continue;
        }

        PRBool isRead;

        nsMsgKey hdrMsgId;
        msgHdr->GetMessageKey(&hdrMsgId);
        m_db->IsRead(hdrMsgId, &isRead);

        if (isRead != bRead) {
	    // MarkHdrRead will change the unread count on the thread
            m_db->MarkHdrRead(msgHdr, bRead, nsnull);
            // insert at the front.  should we insert at the end?
            idsMarkedRead.InsertAt(0, hdrMsgId);
        }
    }

    return NS_OK;
}

PRBool nsMsgDBView::AdjustReadFlag(nsIMsgDBHdr *msgHdr, PRUint32 *msgFlags)
{
  PRBool isRead = PR_FALSE;
  nsMsgKey msgKey;
  msgHdr->GetMessageKey(&msgKey);
  m_db->IsRead(msgKey, &isRead);
    // just make sure flag is right in db.
#ifdef DEBUG_bienvenu
  NS_ASSERTION(isRead == (*msgFlags & MSG_FLAG_READ != 0), "msgFlags out of sync");
#endif
  if (isRead)
    *msgFlags |= MSG_FLAG_READ;
  else
    *msgFlags &= ~MSG_FLAG_READ;
  m_db->MarkHdrRead(msgHdr, isRead, nsnull);
  return isRead;
}

// Starting from startIndex, performs the passed in navigation, including
// any marking read needed, and returns the resultId and resultIndex of the
// destination of the navigation.  If no message is found in the view,
// it returns a resultId of nsMsgKey_None and an resultIndex of nsMsgViewIndex_None.
NS_IMETHODIMP nsMsgDBView::ViewNavigate(nsMsgNavigationTypeValue motion, nsMsgKey *pResultKey, nsMsgViewIndex *pResultIndex, nsMsgViewIndex *pThreadIndex, PRBool wrap)
{
    NS_ENSURE_ARG_POINTER(pResultKey);
    NS_ENSURE_ARG_POINTER(pResultIndex);
    NS_ENSURE_ARG_POINTER(pThreadIndex);

    PRInt32 currentIndex; 
    nsMsgViewIndex startIndex;

    if (!mOutlinerSelection) // we must be in stand alone message mode
    {
      currentIndex = FindViewIndex(m_currentlyDisplayedMsgKey);
    }
    else
    {         
      nsresult rv = mOutlinerSelection->GetCurrentIndex(&currentIndex);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    startIndex = currentIndex;
    return nsMsgDBView::NavigateFromPos(motion, startIndex, pResultKey, pResultIndex, pThreadIndex, wrap);
}

nsresult nsMsgDBView::NavigateFromPos(nsMsgNavigationTypeValue motion, nsMsgViewIndex startIndex, nsMsgKey *pResultKey, nsMsgViewIndex *pResultIndex, nsMsgViewIndex *pThreadIndex, PRBool wrap)
{
    nsresult rv = NS_OK;
    nsMsgKey resultThreadKey;
    nsMsgViewIndex curIndex;
    nsMsgViewIndex lastIndex = (GetSize() > 0) ? (nsMsgViewIndex) GetSize() - 1 : nsMsgViewIndex_None;
    nsMsgViewIndex threadIndex = nsMsgViewIndex_None;

    // if there aren't any messages in the view, bail out.
    if (GetSize() <= 0) {
      *pResultIndex = nsMsgViewIndex_None;
      *pResultKey = nsMsgKey_None;
      return NS_OK;
    }

    switch (motion) {
        case nsMsgNavigationType::firstMessage:
            *pResultIndex = 0;
            *pResultKey = m_keys.GetAt(0);
            break;
        case nsMsgNavigationType::nextMessage:
            // return same index and id on next on last message
            *pResultIndex = PR_MIN(startIndex + 1, lastIndex);
            *pResultKey = m_keys.GetAt(*pResultIndex);
            break;
        case nsMsgNavigationType::previousMessage:
            *pResultIndex = (startIndex != nsMsgViewIndex_None) ? startIndex - 1 : 0;
            *pResultKey = m_keys.GetAt(*pResultIndex);
            break;
        case nsMsgNavigationType::lastMessage:
            *pResultIndex = lastIndex;
            *pResultKey = m_keys.GetAt(*pResultIndex);
            break;
        case nsMsgNavigationType::firstFlagged:
            rv = FindFirstFlagged(pResultIndex);
            if (IsValidIndex(*pResultIndex))
                *pResultKey = m_keys.GetAt(*pResultIndex);
            break;
        case nsMsgNavigationType::nextFlagged:
            rv = FindNextFlagged(startIndex + 1, pResultIndex);
            if (IsValidIndex(*pResultIndex))
                *pResultKey = m_keys.GetAt(*pResultIndex);
            break;
        case nsMsgNavigationType::previousFlagged:
            rv = FindPrevFlagged(startIndex, pResultIndex);
            if (IsValidIndex(*pResultIndex))
                *pResultKey = m_keys.GetAt(*pResultIndex);
            break;
        case nsMsgNavigationType::firstNew:
            rv = FindFirstNew(pResultIndex);
            if (IsValidIndex(*pResultIndex))
                *pResultKey = m_keys.GetAt(*pResultIndex);
            break;
        case nsMsgNavigationType::firstUnreadMessage:
            startIndex = nsMsgViewIndex_None;        // note fall thru - is this motion ever used?
        case nsMsgNavigationType::nextUnreadMessage:
            for (curIndex = (startIndex == nsMsgViewIndex_None) ? 0 : startIndex; curIndex <= lastIndex && lastIndex != nsMsgViewIndex_None; curIndex++) {
                PRUint32 flags = m_flags.GetAt(curIndex);

                // don't return start index since navigate should move
                if (!(flags & MSG_FLAG_READ) && (curIndex != startIndex)) {
                    *pResultIndex = curIndex;
                    *pResultKey = m_keys.GetAt(*pResultIndex);
                    break;
                }
                // check for collapsed thread with new children
                if (m_sortType == nsMsgViewSortType::byThread && flags & MSG_VIEW_FLAG_ISTHREAD && flags & MSG_FLAG_ELIDED) {
                    nsCOMPtr <nsIMsgThread> threadHdr;
                    GetThreadContainingIndex(curIndex, getter_AddRefs(threadHdr));
                    NS_ENSURE_SUCCESS(rv, rv);

                    NS_ASSERTION(threadHdr, "threadHdr is null");
                    if (!threadHdr) {
                        continue;
                    }
                    PRUint32 numUnreadChildren;
                    threadHdr->GetNumUnreadChildren(&numUnreadChildren);
                    if (numUnreadChildren > 0) {
                        PRUint32 numExpanded;
                        ExpandByIndex(curIndex, &numExpanded);
                        lastIndex += numExpanded;
                        if (pThreadIndex)
                            *pThreadIndex = curIndex;
                    }
                }
            }
            if (curIndex > lastIndex) {
                // wrap around by starting at index 0.
                if (wrap) {
                    nsMsgKey startKey = GetAt(startIndex);

                    rv = NavigateFromPos(nsMsgNavigationType::nextUnreadMessage, nsMsgViewIndex_None, pResultKey, pResultIndex, pThreadIndex, PR_FALSE);

                    if (*pResultKey == startKey) {   
                        // wrapped around and found start message!
                        *pResultIndex = nsMsgViewIndex_None;
                        *pResultKey = nsMsgKey_None;
                    }
                }
                else {
                    *pResultIndex = nsMsgViewIndex_None;
                    *pResultKey = nsMsgKey_None;
                }
            }
            break;
        case nsMsgNavigationType::previousUnreadMessage:
            if (startIndex == nsMsgViewIndex_None) {
              break;
            }
            rv = FindPrevUnread(m_keys.GetAt(startIndex), pResultKey,
                                &resultThreadKey);
            if (NS_SUCCEEDED(rv)) {
                *pResultIndex = FindViewIndex(*pResultKey);
                if (*pResultKey != resultThreadKey && m_sortType == nsMsgViewSortType::byThread) {
                    threadIndex  = ThreadIndexOfMsg(*pResultKey, nsMsgViewIndex_None);
                    if (*pResultIndex == nsMsgViewIndex_None) {
                        nsCOMPtr <nsIMsgThread> threadHdr;
                        nsCOMPtr <nsIMsgDBHdr> msgHdr;
                        rv = m_db->GetMsgHdrForKey(*pResultKey, getter_AddRefs(msgHdr));
                        NS_ENSURE_SUCCESS(rv, rv);
                        rv = m_db->GetThreadContainingMsgHdr(msgHdr, getter_AddRefs(threadHdr));
                        NS_ENSURE_SUCCESS(rv, rv);

                        NS_ASSERTION(threadHdr, "threadHdr is null");
                        if (threadHdr) {
                            break;
                        }
                        PRUint32 numUnreadChildren;
                        threadHdr->GetNumUnreadChildren(&numUnreadChildren);
                        if (numUnreadChildren > 0) {
                            PRUint32 numExpanded;
                            ExpandByIndex(threadIndex, &numExpanded);
                        }
                        *pResultIndex = FindViewIndex(*pResultKey);
                    }
                }
                if (pThreadIndex)
                    *pThreadIndex = threadIndex;
            }
            break;
        case nsMsgNavigationType::lastUnreadMessage:
            break;
        case nsMsgNavigationType::nextUnreadThread:
            {
                nsMsgKeyArray idsMarkedRead;

                if (startIndex == nsMsgViewIndex_None) {
                    NS_ASSERTION(0,"startIndex == nsMsgViewIndex_None");
                    break;
                }
                rv = MarkThreadOfMsgRead(m_keys.GetAt(startIndex), startIndex, idsMarkedRead, PR_TRUE);
                if (NS_SUCCEEDED(rv)) 
                    return NavigateFromPos(nsMsgNavigationType::nextUnreadMessage, startIndex, pResultKey, pResultIndex, pThreadIndex, PR_TRUE);
                break;
            }
        case nsMsgNavigationType::toggleThreadKilled:
            {
                PRBool resultKilled;

                if (startIndex == nsMsgViewIndex_None) {
                    NS_ASSERTION(0,"startIndex == nsMsgViewIndex_None");
                    break;
                }
                threadIndex = ThreadIndexOfMsg(GetAt(startIndex), startIndex);
                ToggleIgnored(&startIndex, 1, &resultKilled);
                if (resultKilled) {
                    if (threadIndex != nsMsgViewIndex_None)
                        CollapseByIndex(threadIndex, nsnull);
                    return NavigateFromPos(nsMsgNavigationType::nextUnreadThread, threadIndex, pResultKey, pResultIndex, pThreadIndex, PR_TRUE);
                }
                else {
                    *pResultIndex = startIndex;
                    *pResultKey = m_keys.GetAt(*pResultIndex);
                    return NS_OK;
                }
            }
        case nsMsgNavigationType::laterMessage:
            if (startIndex == nsMsgViewIndex_None) {
                NS_ASSERTION(0, "unexpected");
                break;
            }
            m_db->MarkLater(m_keys.GetAt(startIndex), LL_ZERO);
            return NavigateFromPos(nsMsgNavigationType::nextUnreadMessage, startIndex, pResultKey, pResultIndex, pThreadIndex, PR_TRUE);
        default:
            NS_ASSERTION(0, "unsupported motion");
            break;
    }
    return NS_OK;
}

NS_IMETHODIMP nsMsgDBView::NavigateStatus(nsMsgNavigationTypeValue motion, PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    PRBool enable = PR_FALSE;
    nsresult rv = NS_ERROR_FAILURE;
    nsMsgKey resultKey = nsMsgKey_None;
    PRInt32 index;
    nsMsgViewIndex resultIndex = nsMsgViewIndex_None;
    rv = mOutlinerSelection->GetCurrentIndex(&index);

    // warning - we no longer validate index up front because fe passes in -1 for no
    // selection, so if you use index, be sure to validate it before using it
    // as an array index.
    switch (motion) {
        case nsMsgNavigationType::firstMessage:
        case nsMsgNavigationType::lastMessage:
            if (GetSize() > 0)
                enable = PR_TRUE;
            break;
        case nsMsgNavigationType::nextMessage:
            if (IsValidIndex(index) && index < GetSize() - 1)
                enable = PR_TRUE;
            break;
        case nsMsgNavigationType::previousMessage:
            if (IsValidIndex(index) && index != 0 && GetSize() > 1)
                enable = PR_TRUE;
            break;
        case nsMsgNavigationType::firstFlagged:
            rv = FindFirstFlagged(&resultIndex);
            enable = (NS_SUCCEEDED(rv) && resultIndex != nsMsgViewIndex_None);
            break;
        case nsMsgNavigationType::nextFlagged:
            rv = FindNextFlagged(index + 1, &resultIndex);
            enable = (NS_SUCCEEDED(rv) && resultIndex != nsMsgViewIndex_None);
            break;
        case nsMsgNavigationType::previousFlagged:
            if (IsValidIndex(index) && index != 0)
                rv = FindPrevFlagged(index, &resultIndex);
            enable = (NS_SUCCEEDED(rv) && resultIndex != nsMsgViewIndex_None);
            break;
        case nsMsgNavigationType::firstNew:
            rv = FindFirstNew(&resultIndex);
            enable = (NS_SUCCEEDED(rv) && resultIndex != nsMsgViewIndex_None);
            break;
        case nsMsgNavigationType::laterMessage:
            enable = GetSize() > 0;
            break;
        case nsMsgNavigationType::readMore:
            enable = PR_TRUE;  // for now, always true.
            break;
        case nsMsgNavigationType::nextFolder:
        case nsMsgNavigationType::nextUnreadThread:
        case nsMsgNavigationType::nextUnreadMessage:
        case nsMsgNavigationType::toggleThreadKilled:
            enable = PR_TRUE;  // always enabled
            break;
        case nsMsgNavigationType::previousUnreadMessage:
            if (IsValidIndex(index)) {
                nsMsgKey threadId;
                rv = FindPrevUnread(m_keys.GetAt(index), &resultKey, &threadId);
                enable = (resultKey != nsMsgKey_None);
            }
            break;
        default:
            NS_ASSERTION(0,"unexpected");
            break;
    }

    *_retval = enable;
    return NS_OK;
}

// Note that these routines do NOT expand collapsed threads! This mimics the old behaviour,
// but it's also because we don't remember whether a thread contains a flagged message the
// same way we remember if a thread contains new messages. It would be painful to dive down
// into each collapsed thread to update navigate status.
// We could cache this info, but it would still be expensive the first time this status needs
// to get updated.
nsresult nsMsgDBView::FindNextFlagged(nsMsgViewIndex startIndex, nsMsgViewIndex *pResultIndex)
{
    nsMsgViewIndex lastIndex = (nsMsgViewIndex) GetSize() - 1;
    nsMsgViewIndex curIndex;

    *pResultIndex = nsMsgViewIndex_None;

    if (GetSize() > 0) {
        for (curIndex = startIndex; curIndex <= lastIndex; curIndex++) {
            PRUint32 flags = m_flags.GetAt(curIndex);
            if (flags & MSG_FLAG_MARKED) {
                *pResultIndex = curIndex;
                break;
            }
        }
    }

    return NS_OK;
}

nsresult nsMsgDBView::FindFirstNew(nsMsgViewIndex *pResultIndex)
{
    if (m_db) {
        nsMsgKey firstNewKey;
        m_db->GetFirstNew(&firstNewKey);
        if (pResultIndex)
            *pResultIndex = FindKey(firstNewKey, PR_TRUE);
    }
    return NS_OK;
}

// Generic routine to find next unread id. It doesn't do an expand of a
// thread with new messages, so it can't return a view index.
nsresult nsMsgDBView::FindNextUnread(nsMsgKey startId, nsMsgKey *pResultKey,
                                     nsMsgKey *resultThreadId)
{
    nsMsgViewIndex startIndex = FindViewIndex(startId);
    nsMsgViewIndex curIndex = startIndex;
    nsMsgViewIndex lastIndex = (nsMsgViewIndex) GetSize() - 1;
    nsresult rv = NS_OK;

    if (startIndex == nsMsgViewIndex_None)
        return NS_MSG_MESSAGE_NOT_FOUND;

    *pResultKey = nsMsgKey_None;
    if (resultThreadId)
        *resultThreadId = nsMsgKey_None;

    for (; curIndex <= lastIndex && (*pResultKey == nsMsgKey_None); curIndex++) {
        char    flags = m_flags.GetAt(curIndex);

        if (!(flags & MSG_FLAG_READ) && (curIndex != startIndex)) {
            *pResultKey = m_keys.GetAt(curIndex);
            break;
        }
        // check for collapsed thread with unread children
        if (m_sortType == nsMsgViewSortType::byThread && flags & MSG_VIEW_FLAG_ISTHREAD && flags & MSG_FLAG_ELIDED) {
            nsCOMPtr<nsIMsgThread> thread;
            //nsMsgKey threadId = m_keys.GetAt(curIndex);
            rv = GetThreadFromMsgIndex(curIndex, getter_AddRefs(thread));
            if (NS_SUCCEEDED(rv) && thread) {
              nsCOMPtr <nsIMsgDBHdr> unreadChild;
              rv = thread->GetFirstUnreadChild(getter_AddRefs(unreadChild));
              if (NS_SUCCEEDED(rv) && unreadChild)
                unreadChild->GetMessageKey(pResultKey);
            }
            //rv = m_db->GetUnreadKeyInThread(threadId, pResultKey, resultThreadId);
            if (NS_SUCCEEDED(rv) && (*pResultKey != nsMsgKey_None))
                break;
        }
    }
    // found unread message but we don't know the thread
    if (*pResultKey != nsMsgKey_None && resultThreadId && *resultThreadId == nsMsgKey_None) {
        NS_ASSERTION(0,"fix this");
        //*resultThreadId = m_db->GetThreadIdForMsgId(*pResultKey);
    }
    return rv;
}


nsresult nsMsgDBView::FindPrevUnread(nsMsgKey startKey, nsMsgKey *pResultKey,
                                     nsMsgKey *resultThreadId)
{
    nsMsgViewIndex startIndex = FindViewIndex(startKey);
    nsMsgViewIndex curIndex = startIndex;
    nsresult rv = NS_MSG_MESSAGE_NOT_FOUND;

    if (startIndex == nsMsgViewIndex_None)
        return NS_MSG_MESSAGE_NOT_FOUND;

    *pResultKey = nsMsgKey_None;
    if (resultThreadId)
        *resultThreadId = nsMsgKey_None;

    for (; (int) curIndex >= 0 && (*pResultKey == nsMsgKey_None); curIndex--) {
        PRUint32 flags = m_flags.GetAt(curIndex);

        if (curIndex != startIndex && flags & MSG_VIEW_FLAG_ISTHREAD && flags & MSG_FLAG_ELIDED) {
            NS_ASSERTION(0,"fix this");
            //nsMsgKey threadId = m_keys.GetAt(curIndex);
            //rv = m_db->GetUnreadKeyInThread(threadId, pResultKey, resultThreadId);
            if (NS_SUCCEEDED(rv) && (*pResultKey != nsMsgKey_None))
                break;
        }
        if (!(flags & MSG_FLAG_READ) && (curIndex != startIndex)) {
            *pResultKey = m_keys.GetAt(curIndex);
            rv = NS_OK;
            break;
        }
    }
    // found unread message but we don't know the thread
    if (*pResultKey != nsMsgKey_None && resultThreadId && *resultThreadId == nsMsgKey_None) {
        NS_ASSERTION(0,"fix this");
        //*resultThreadId = m_db->GetThreadIdForMsgId(*pResultKey);
    }
    return rv;
}

nsresult nsMsgDBView::FindFirstFlagged(nsMsgViewIndex *pResultIndex)
{
    return FindNextFlagged(0, pResultIndex);
}

nsresult nsMsgDBView::FindPrevFlagged(nsMsgViewIndex startIndex, nsMsgViewIndex *pResultIndex)
{
    nsMsgViewIndex curIndex;

    *pResultIndex = nsMsgViewIndex_None;

    if (GetSize() > 0 && IsValidIndex(startIndex)) {
        curIndex = startIndex;
        do {
            if (curIndex != 0)
                curIndex--;

            PRUint32 flags = m_flags.GetAt(curIndex);
            if (flags & MSG_FLAG_MARKED) {
                *pResultIndex = curIndex;
                break;
            }
        }
        while (curIndex != 0);
    }
    return NS_OK;
}

PRBool nsMsgDBView::IsValidIndex(nsMsgViewIndex index)
{
    return ((index >=0) && (index < (nsMsgViewIndex) m_keys.GetSize()));
}

nsresult nsMsgDBView::OrExtraFlag(nsMsgViewIndex index, PRUint32 orflag)
{
	PRUint32	flag;
	if (!IsValidIndex(index))
		return NS_MSG_INVALID_DBVIEW_INDEX;
	flag = m_flags[index];
	flag |= orflag;
	m_flags[index] = flag;
	OnExtraFlagChanged(index, flag);
	return NS_OK;
}

nsresult nsMsgDBView::AndExtraFlag(nsMsgViewIndex index, PRUint32 andflag)
{
	PRUint32	flag;
	if (!IsValidIndex(index))
		return NS_MSG_INVALID_DBVIEW_INDEX;
	flag = m_flags[index];
	flag &= andflag;
	m_flags[index] = flag;
	OnExtraFlagChanged(index, flag);
	return NS_OK;
}

nsresult nsMsgDBView::SetExtraFlag(nsMsgViewIndex index, PRUint32 extraflag)
{
	if (!IsValidIndex(index))
		return NS_MSG_INVALID_DBVIEW_INDEX;
	m_flags[index] = extraflag;
	OnExtraFlagChanged(index, extraflag);
	return NS_OK;
}


nsresult nsMsgDBView::ToggleIgnored(nsMsgViewIndex * indices, PRInt32 numIndices, PRBool *resultToggleState)
{
  nsresult rv = NS_OK;
  nsCOMPtr <nsIMsgThread> thread;

  if (numIndices == 1)
  {
    nsMsgViewIndex    threadIndex = GetThreadFromMsgIndex(*indices, getter_AddRefs(thread));
    if (thread)
    {
      rv = ToggleThreadIgnored(thread, threadIndex);
      if (resultToggleState)
      {
        PRUint32 threadFlags;
        thread->GetFlags(&threadFlags);
        *resultToggleState = (threadFlags & MSG_FLAG_IGNORED) ? PR_TRUE : PR_FALSE;
      }
    }
  }
  else
  {
    if (numIndices > 1)
      NS_QuickSort (indices, numIndices, sizeof(nsMsgViewIndex), CompareViewIndices, nsnull);
    for (int curIndex = numIndices - 1; curIndex >= 0; curIndex--)
    {
      // here we need to build up the unique threads, and mark them ignored.
      nsMsgViewIndex threadIndex;
      threadIndex = GetThreadFromMsgIndex(*indices, getter_AddRefs(thread));
    }
  }
  return rv;
}

nsMsgViewIndex	nsMsgDBView::GetThreadFromMsgIndex(nsMsgViewIndex index, 
													 nsIMsgThread **threadHdr)
{
	nsMsgKey			msgKey = GetAt(index);
	nsMsgViewIndex		threadIndex;

	NS_ENSURE_ARG(threadHdr);
  nsresult rv = GetThreadContainingIndex(index, threadHdr);
  NS_ENSURE_SUCCESS(rv,nsMsgViewIndex_None);

	if (*threadHdr == nsnull)
		return nsMsgViewIndex_None;

  nsMsgKey threadKey;
  (*threadHdr)->GetThreadKey(&threadKey);
	if (msgKey !=threadKey)
		threadIndex = GetIndexOfFirstDisplayedKeyInThread(*threadHdr);
	else
		threadIndex = index;
	return threadIndex;
}

// ignore handling.
nsresult nsMsgDBView::ToggleThreadIgnored(nsIMsgThread *thread, nsMsgViewIndex threadIndex)

{
	nsresult		rv;
	if (!IsValidIndex(threadIndex))
		return NS_MSG_INVALID_DBVIEW_INDEX;
  PRUint32 threadFlags;
  thread->GetFlags(&threadFlags);
	rv = SetThreadIgnored(thread, threadIndex, !((threadFlags & MSG_FLAG_IGNORED) != 0));
	return rv;
}

nsresult nsMsgDBView::ToggleThreadWatched(nsIMsgThread *thread, nsMsgViewIndex index)
{
	nsresult		rv;
	if (!IsValidIndex(index))
		return NS_MSG_INVALID_DBVIEW_INDEX;
  PRUint32 threadFlags;
  thread->GetFlags(&threadFlags);
	rv = SetThreadWatched(thread, index, !((threadFlags & MSG_FLAG_WATCHED) != 0));
	return rv;
}

nsresult nsMsgDBView::ToggleWatched( nsMsgViewIndex* indices,	PRInt32 numIndices)
{
	nsresult rv;
	nsCOMPtr <nsIMsgThread> thread;

	if (numIndices == 1)
	{
		nsMsgViewIndex	threadIndex = GetThreadFromMsgIndex(*indices, getter_AddRefs(thread));
		if (threadIndex != nsMsgViewIndex_None)
		{
			rv = ToggleThreadWatched(thread, threadIndex);
		}
	}
	else
	{
		if (numIndices > 1)
			NS_QuickSort (indices, numIndices, sizeof(nsMsgViewIndex), CompareViewIndices, nsnull);
		for (int curIndex = numIndices - 1; curIndex >= 0; curIndex--)
		{
		  nsMsgViewIndex	threadIndex;    
          threadIndex = GetThreadFromMsgIndex(*indices, getter_AddRefs(thread));
      // here we need to build up the unique threads, and mark them watched.
		}
	}
	return NS_OK;
}

nsresult nsMsgDBView::SetThreadIgnored(nsIMsgThread *thread, nsMsgViewIndex threadIndex, PRBool ignored)
{
	nsresult rv;

	if (!IsValidIndex(threadIndex))
		return NS_MSG_INVALID_DBVIEW_INDEX;
	rv = m_db->MarkThreadIgnored(thread, m_keys[threadIndex], ignored, this);
	NoteChange(threadIndex, 1, nsMsgViewNotificationCode::changed);
	if (ignored)
	{
		nsMsgKeyArray	idsMarkedRead;

		MarkThreadRead(thread, threadIndex, idsMarkedRead, PR_TRUE);
	}
	return rv;
}
nsresult nsMsgDBView::SetThreadWatched(nsIMsgThread *thread, nsMsgViewIndex index, PRBool watched)
{
	nsresult rv;

	if (!IsValidIndex(index))
		return NS_MSG_INVALID_DBVIEW_INDEX;
	rv = m_db->MarkThreadWatched(thread, m_keys[index], watched, this);
	NoteChange(index, 1, nsMsgViewNotificationCode::changed);
	return rv;
}

NS_IMETHODIMP nsMsgDBView::GetMsgFolder(nsIMsgFolder **aMsgFolder)
{
  NS_ENSURE_ARG_POINTER(aMsgFolder);
  *aMsgFolder = m_folder;
  NS_IF_ADDREF(*aMsgFolder);
  return NS_OK;
}

NS_IMETHODIMP 
nsMsgDBView::GetNumSelected(PRUint32 *numSelected)
{ 
  NS_ENSURE_ARG_POINTER(numSelected);
    
  if (!mOutlinerSelection) 
  {
    *numSelected = 0;
    return NS_OK;
  }
  
  // We call this a lot from the front end JS, so make it fast.
  return mOutlinerSelection->GetCount((PRInt32*)numSelected);
}

NS_IMETHODIMP 
nsMsgDBView::GetMsgToSelectAfterDelete(nsMsgViewIndex *msgToSelectAfterDelete)
{
  NS_ENSURE_ARG_POINTER(msgToSelectAfterDelete);
  *msgToSelectAfterDelete = nsMsgViewIndex_None;
  if (!mOutlinerSelection) 
  {
    // if we don't have an outliner selection then we must be in stand alone mode.
    // return the index of the current message key as the first selected index.
    *msgToSelectAfterDelete = FindViewIndex(m_currentlyDisplayedMsgKey);
    return NS_OK;
  }
   
  PRInt32 selectionCount;
  PRInt32 startRange;
  PRInt32 endRange;
  nsresult rv = mOutlinerSelection->GetRangeCount(&selectionCount);
  for (PRInt32 i = 0; i < selectionCount; i++) 
  {
    rv = mOutlinerSelection->GetRangeAt(i, &startRange, &endRange);
    *msgToSelectAfterDelete = PR_MIN(*msgToSelectAfterDelete, startRange);
  }
  nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(m_folder);
  PRBool thisIsImapFolder = (imapFolder != nsnull);
  if (thisIsImapFolder) //need to update the imap-delete model, can change more than once in a session.
    GetImapDeleteModel(nsnull);
  if (mDeleteModel == nsMsgImapDeleteModels::IMAPDelete)
    if (selectionCount > 1 || (endRange-startRange) > 0)  //multiple selection either using Ctrl or Shift keys
      *msgToSelectAfterDelete = nsMsgViewIndex_None;
    else
      *msgToSelectAfterDelete += 1;

  return NS_OK;
}


NS_IMETHODIMP 
nsMsgDBView::GetCurrentlyDisplayedMessage (nsMsgViewIndex *currentlyDisplayedMessage)
{
  NS_ENSURE_ARG_POINTER(currentlyDisplayedMessage);
  *currentlyDisplayedMessage = FindViewIndex(m_currentlyDisplayedMsgKey);
  return NS_OK;
}

// if nothing selected, return an NS_ERROR
NS_IMETHODIMP
nsMsgDBView::GetHdrForFirstSelectedMessage(nsIMsgDBHdr **hdr)
{
  NS_ENSURE_ARG_POINTER(hdr);

  nsresult rv;
  nsMsgKey key;
  rv = GetKeyForFirstSelectedMessage(&key);
  // don't assert, it is legal for nothing to be selected
  if (NS_FAILED(rv)) return rv;

  rv = m_db->GetMsgHdrForKey(key, hdr);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

// if nothing selected, return an NS_ERROR
NS_IMETHODIMP 
nsMsgDBView::GetURIForFirstSelectedMessage(char **uri)
{
  NS_ENSURE_ARG_POINTER(uri);

#ifdef DEBUG_mscott
  printf("inside GetURIForFirstSelectedMessage\n");
#endif
  nsresult rv;
  nsMsgKey key;
  rv = GetKeyForFirstSelectedMessage(&key);
  // don't assert, it is legal for nothing to be selected
  if (NS_FAILED(rv)) return rv;
 
  rv = GenerateURIForMsgKey(key, m_folder, uri);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

// nsIMsgCopyServiceListener methods
NS_IMETHODIMP
nsMsgDBView::OnStartCopy()
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBView::OnProgress(PRUint32 aProgress, PRUint32 aProgressMax)
{
  return NS_OK;
}

// believe it or not, these next two are msgcopyservice listener methods!
NS_IMETHODIMP
nsMsgDBView::SetMessageKey(PRUint32 aMessageKey)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBView::GetMessageId(nsCString* aMessageId)
{
  return NS_OK;
}
  
NS_IMETHODIMP
nsMsgDBView::OnStopCopy(nsresult aStatus)
{
  m_deletingMsgs = PR_FALSE;
  m_removedIndices.RemoveAll();
  return NS_OK;
}
// end nsIMsgCopyServiceListener methods

PRBool nsMsgDBView::OfflineMsgSelected(nsMsgViewIndex * indices, PRInt32 numIndices)
{
  nsCOMPtr <nsIMsgLocalMailFolder> localFolder = do_QueryInterface(m_folder);
  if (localFolder)
    return PR_TRUE;

  nsresult rv = NS_OK;
  for (nsMsgViewIndex index = 0; index < (nsMsgViewIndex) numIndices; index++)
  {
    PRUint32 flags = m_flags.GetAt(indices[index]);
    if ((flags & MSG_FLAG_OFFLINE))
      return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult
nsMsgDBView::GetKeyForFirstSelectedMessage(nsMsgKey *key)
{
  NS_ENSURE_ARG_POINTER(key);
  // if we don't have an outliner selection we must be in stand alone mode....
  if (!mOutlinerSelection) 
  {
    *key = m_currentlyDisplayedMsgKey;
    return NS_OK;
  }

  PRInt32 currentIndex;
  nsresult rv = mOutlinerSelection->GetCurrentIndex(&currentIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  // check that the first index is valid, it may not be if nothing is selected
  if (currentIndex >= 0 && currentIndex < GetSize()) {
    *key = m_keys.GetAt(currentIndex);
  }
  else {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

nsresult nsMsgDBView::GetFolders(nsISupportsArray **aFolders)
{
    NS_ENSURE_ARG_POINTER(aFolders);
    *aFolders = nsnull;
	
    return NS_OK;
}

nsresult nsMsgDBView::AdjustRowCount(PRInt32 rowCountBeforeSort, PRInt32 rowCountAfterSort)
{
  PRInt32 rowChange = rowCountBeforeSort - rowCountAfterSort;

  if (rowChange) {
    // this is not safe to use when you have a selection
    // RowCountChanged() will call AdjustSelection()
    if (mOutliner) mOutliner->RowCountChanged(0, rowChange);
  }
  return NS_OK;
}

nsresult nsMsgDBView::GetImapDeleteModel(nsIMsgFolder *folder)
{
   nsresult rv = NS_OK;
   nsCOMPtr <nsIMsgIncomingServer> server;
   if (folder) //for the search view 
     folder->GetServer(getter_AddRefs(server));
   else if (m_folder)
     m_folder->GetServer(getter_AddRefs(server));
   nsCOMPtr<nsIImapIncomingServer> imapServer = do_QueryInterface(server, &rv);
   if (NS_SUCCEEDED(rv) && imapServer )
     imapServer->GetDeleteModel(&mDeleteModel);       
   return rv;
}


//
// CanDropOn
//
// Can't drop on the thread pane.
//
NS_IMETHODIMP nsMsgDBView::CanDropOn(PRInt32 index, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  return NS_OK;
}

//
// CanDropBeforeAfter
//
// Can't drop on the thread pane.
//
NS_IMETHODIMP nsMsgDBView::CanDropBeforeAfter(PRInt32 index, PRBool before, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  return NS_OK;
}


//
// Drop
//
// Can't drop on the thread pane.
//
NS_IMETHODIMP nsMsgDBView::Drop(PRInt32 row, PRInt32 orient)
{
  return NS_OK;
}


//
// IsSorted
//
// ...
//
NS_IMETHODIMP nsMsgDBView::IsSorted(PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}


