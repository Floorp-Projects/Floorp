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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "msgCore.h"  // for pre-compiled headers

#include "nsImapCore.h"
#include "nsImapFlagAndUidState.h"
#include "prcmon.h"
#include "nspr.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsImapFlagAndUidState, nsIImapFlagAndUidState)

NS_IMETHODIMP nsImapFlagAndUidState::GetNumberOfMessages(PRInt32 *result)
{
  if (!result)
    return NS_ERROR_NULL_POINTER;
  *result = fNumberOfMessagesAdded;
  return NS_OK;
}

NS_IMETHODIMP nsImapFlagAndUidState::GetUidOfMessage(PRInt32 zeroBasedIndex, PRUint32 *result)
{
  if (!result)
    return NS_ERROR_NULL_POINTER;
  
  PR_CEnterMonitor(this);
  if (zeroBasedIndex < fNumberOfMessagesAdded)
    *result = fUids[zeroBasedIndex];
  else
    *result = 0xFFFFFFFF;	// so that value is non-zero and we don't ask for bad msgs
  PR_CExitMonitor(this);
  return NS_OK;
}



NS_IMETHODIMP	nsImapFlagAndUidState::GetMessageFlags(PRInt32 zeroBasedIndex, PRUint16 *result)
{
	if (!result)
		return NS_ERROR_NULL_POINTER;
	imapMessageFlagsType returnFlags = kNoImapMsgFlag;
	if (zeroBasedIndex < fNumberOfMessagesAdded)
		returnFlags = fFlags[zeroBasedIndex];

	*result = returnFlags;
	return NS_OK;
}

NS_IMETHODIMP nsImapFlagAndUidState::GetNumberOfRecentMessages(PRInt32 *result)
{
  if (!result)
    return NS_ERROR_NULL_POINTER;
  
  PR_CEnterMonitor(this);
  PRUint32 counter = 0;
  PRInt32 numUnseenMessages = 0;
  
  for (counter = 0; counter < (PRUint32) fNumberOfMessagesAdded; counter++)
  {
    if (fFlags[counter] & kImapMsgRecentFlag)
      numUnseenMessages++;
  }
  PR_CExitMonitor(this);
  
  *result = numUnseenMessages;
  
  return NS_OK;
}

/* amount to expand for imap entry flags when we need more */

nsImapFlagAndUidState::nsImapFlagAndUidState(PRInt32 numberOfMessages, PRUint16 flags)
{
  fNumberOfMessagesAdded = 0;
  fNumberOfMessageSlotsAllocated = numberOfMessages;
  if (!fNumberOfMessageSlotsAllocated)
	  fNumberOfMessageSlotsAllocated = kImapFlagAndUidStateSize;
  fFlags = (imapMessageFlagsType*) PR_Malloc(sizeof(imapMessageFlagsType) * fNumberOfMessageSlotsAllocated); // new imapMessageFlagsType[fNumberOfMessageSlotsAllocated];
  
  fUids.SetSize(fNumberOfMessageSlotsAllocated);
  nsCRT::memset(fFlags, 0, sizeof(imapMessageFlagsType) * fNumberOfMessageSlotsAllocated);
  fSupportedUserFlags = flags;
  fNumberDeleted = 0;
  NS_INIT_REFCNT();
}

nsImapFlagAndUidState::nsImapFlagAndUidState(const nsImapFlagAndUidState& state, 
										   uint16 flags)
{
  fNumberOfMessagesAdded = state.fNumberOfMessagesAdded;
  
  fNumberOfMessageSlotsAllocated = state.fNumberOfMessageSlotsAllocated;
  fFlags = (imapMessageFlagsType*) PR_Malloc(sizeof(imapMessageFlagsType) * fNumberOfMessageSlotsAllocated); // new imapMessageFlagsType[fNumberOfMessageSlotsAllocated];
  fUids.CopyArray((nsMsgKeyArray *) &state.fUids);

  memcpy(fFlags, state.fFlags, sizeof(imapMessageFlagsType) * fNumberOfMessageSlotsAllocated);
  fSupportedUserFlags = flags;
  fNumberDeleted = 0;
  NS_INIT_REFCNT();
}

nsImapFlagAndUidState::~nsImapFlagAndUidState()
{
  PR_FREEIF(fFlags);
}
	
NS_IMETHODIMP
nsImapFlagAndUidState::SetSupportedUserFlags(uint16 flags)
{
  fSupportedUserFlags |= flags;
  return NS_OK;
}


// we need to reset our flags, (re-read all) but chances are the memory allocation needed will be
// very close to what we were already using

NS_IMETHODIMP nsImapFlagAndUidState::Reset(PRUint32 howManyLeft)
{
    PR_CEnterMonitor(this);
	if (!howManyLeft)
		fNumberOfMessagesAdded = fNumberDeleted = 0;		// used space is still here
    PR_CExitMonitor(this);
	return NS_OK;
}


// Remove (expunge) a message from our array, since now it is gone for good

NS_IMETHODIMP nsImapFlagAndUidState::ExpungeByIndex(PRUint32 msgIndex)
{
  // protect ourselves in case the server gave us an index key of -1.....
  if ((PRInt32) msgIndex < 0)
    return NS_ERROR_INVALID_ARG;

  PRUint32 counter = 0;
  
  if ((PRUint32) fNumberOfMessagesAdded < msgIndex)
    return NS_ERROR_INVALID_ARG;
  
  PR_CEnterMonitor(this);
  msgIndex--;  // msgIndex is 1-relative
  fNumberOfMessagesAdded--;
  if (fFlags[msgIndex] & kImapMsgDeletedFlag)	// see if we already had counted this one as deleted
    fNumberDeleted--;
  for (counter = msgIndex; counter < (PRUint32) fNumberOfMessagesAdded; counter++)
  {
    fUids.SetAt(counter, fUids[counter + 1]);                               
     fFlags[counter] = fFlags[counter + 1];                                  
  }

  PR_CExitMonitor(this);
  return NS_OK;
}


// adds to sorted list.  protects against duplicates and going past fNumberOfMessageSlotsAllocated  
NS_IMETHODIMP nsImapFlagAndUidState::AddUidFlagPair(PRUint32 uid, imapMessageFlagsType flags)
{
  if (uid == nsMsgKey_None) // ignore uid of -1
    return NS_OK;
  PR_CEnterMonitor(this);
  // make sure there is room for this pair
  if (fNumberOfMessagesAdded >= fNumberOfMessageSlotsAllocated)
  {
    fNumberOfMessageSlotsAllocated += kImapFlagAndUidStateSize;
    fUids.SetSize(fNumberOfMessageSlotsAllocated);
    fFlags = (imapMessageFlagsType*) PR_REALLOC(fFlags, sizeof(imapMessageFlagsType) * fNumberOfMessageSlotsAllocated); // new imapMessageFlagsType[fNumberOfMessageSlotsAllocated];
  }
  
  // optimize the common case of placing on the end
  if (!fNumberOfMessagesAdded || (uid > (PRUint32) fUids[fNumberOfMessagesAdded - 1]))
  {	
    fUids.SetAt(fNumberOfMessagesAdded, uid);
    fFlags[fNumberOfMessagesAdded] = flags;
    fNumberOfMessagesAdded++;
    if (flags & kImapMsgDeletedFlag)
      fNumberDeleted++;
    PR_CExitMonitor(this);
    return NS_OK;
  }
  
  // search for the slot for this uid-flag pair
  
  PRInt32 insertionIndex = -1;
  PRBool foundIt = PR_FALSE;
  
  GetMessageFlagsFromUID(uid, &foundIt, &insertionIndex);
  // Hmmm, is the server sending back unordered fetch responses?
  if (((PRUint32) fUids[insertionIndex]) != uid)
  {
    // shift the uids and flags to the right
    for (PRInt32 shiftIndex = fNumberOfMessagesAdded; shiftIndex > insertionIndex; shiftIndex--)
    {
      fUids.SetAt(shiftIndex, fUids[shiftIndex - 1]);
      fFlags[shiftIndex] = fFlags[shiftIndex - 1];
    }
    fFlags[insertionIndex] = flags;
    fUids.SetAt(insertionIndex, uid);
    fNumberOfMessagesAdded++;
    if (fFlags[insertionIndex] & kImapMsgDeletedFlag)
      fNumberDeleted++;
  }
  else 
  {
    if ((fFlags[insertionIndex] & kImapMsgDeletedFlag) && !(flags & kImapMsgDeletedFlag))
      fNumberDeleted--;
    else
      if (!(fFlags[insertionIndex] & kImapMsgDeletedFlag) && (flags & kImapMsgDeletedFlag))
        fNumberDeleted++;
      fFlags[insertionIndex] = flags;
  }
  PR_CExitMonitor(this);
  return NS_OK;
}

	
PRInt32 nsImapFlagAndUidState::GetNumberOfDeletedMessages()
{
  return fNumberDeleted;
}
	
// since the uids are sorted, start from the back (rb)

PRUint32  nsImapFlagAndUidState::GetHighestNonDeletedUID()
{
	PRUint32 msgIndex = fNumberOfMessagesAdded;
	do {
		if (msgIndex <= 0)
			return(0);
		msgIndex--;
		if (fUids[msgIndex] && !(fFlags[msgIndex] & kImapMsgDeletedFlag))
			return fUids[msgIndex];
	} while (msgIndex > 0);
	return 0;
}


// Has the user read the last message here ? Used when we first open the inbox to see if there
// really is new mail there.

PRBool nsImapFlagAndUidState::IsLastMessageUnseen()
{
	PRUint32 msgIndex = fNumberOfMessagesAdded;

	if (msgIndex <= 0)
		return PR_FALSE;
	msgIndex--;
	// if last message is deleted, it was probably filtered the last time around
	if (fUids[msgIndex] && (fFlags[msgIndex] & (kImapMsgSeenFlag | kImapMsgDeletedFlag)))
		return PR_FALSE;
	return PR_TRUE; 
}



// find a message flag given a key with non-recursive binary search, since some folders
// may have thousand of messages, once we find the key set its index, or the index of
// where the key should be inserted

imapMessageFlagsType nsImapFlagAndUidState::GetMessageFlagsFromUID(PRUint32 uid, PRBool *foundIt, PRInt32 *ndx)
{
  PR_CEnterMonitor(this);
  
  PRInt32 msgIndex = 0;
  PRInt32 hi = fNumberOfMessagesAdded - 1;
  PRInt32 lo = 0;
  
  *foundIt = PR_FALSE;
  *ndx = -1;
  while (lo <= hi)
  {
    msgIndex = (lo + hi) / 2;
    if (fUids[msgIndex] == (PRUint32) uid)
    {
      PRInt32 returnFlags = fFlags[msgIndex];
      
      *foundIt = PR_TRUE;
      *ndx = msgIndex;
      PR_CExitMonitor(this);
      return returnFlags;
    }
    if (fUids[msgIndex] > (PRUint32) uid)
      hi = msgIndex -1;
    else if (fUids[msgIndex] < (PRUint32) uid)
      lo = msgIndex + 1;
  }
  msgIndex = lo;
  // leave msgIndex pointing to the first slot with a value > uid
  // first, move it before any ids that are > (shouldn't happen).
  while ((msgIndex > 0) && (fUids[msgIndex - 1] > (PRUint32) uid))
    msgIndex--;
  
  // next, move msgIndex up to the first slot > than uid.
  while ((PRUint32) uid < fUids[msgIndex])
    msgIndex++;
  
  if (msgIndex < 0)
    msgIndex = 0;
  *ndx = msgIndex;
  PR_CExitMonitor(this);
  return 0;
}

