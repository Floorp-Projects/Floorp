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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsMsgOfflineImapOperation.h"

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsMsgOfflineImapOperation, nsIMsgOfflineImapOperation)

// property names for offine imap operation fields.
#define PROP_OPERATION "op"
#define PROP_OPERATION_FLAGS "opFlags"
#define PROP_NEW_FLAGS "newFlags"
#define PROP_MESSAGE_KEY "msgKey"
#define PROP_SRC_MESSAGE_KEY "srcMsgKey"
#define PROP_NEW_FLAGS "newFlags"
#define PROP_SRC_FOLDER_URI "srcFolderURI"
#define PROP_MOVE_DEST_FOLDER_URI "moveDest"
#define PROP_NUM_COPY_DESTS "numCopyDests"
#define PROP_COPY_DESTS "copyDests" // how to delimit these? Or should we do the "dest1","dest2" etc trick? But then we'd need to shuffle
                                    // them around since we delete off the front first.

nsMsgOfflineImapOperation::nsMsgOfflineImapOperation(nsMsgDatabase *db, nsIMdbRow *row)
{
  NS_ASSERTION(db, "can't have null db");
  NS_ASSERTION(row, "can't have null row");
  NS_INIT_ISUPPORTS();
  m_operation = 0;
  m_operationFlags = 0;
  m_messageKey = nsMsgKey_None;
  m_sourceMessageKey = nsMsgKey_None;
  m_mdb = db; 
  NS_ADDREF(m_mdb);
  m_mdbRow = row;
  m_newFlags = 0;

}

nsMsgOfflineImapOperation::~nsMsgOfflineImapOperation()
{
  NS_IF_RELEASE(m_mdb);
}

/* attribute nsOfflineImapOperationType operation; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetOperation(nsOfflineImapOperationType *aOperation)
{
  NS_ENSURE_ARG(aOperation);
  nsresult rv = m_mdb->GetUint32Property(m_mdbRow, PROP_OPERATION, (PRUint32 *) aOperation, 0);
  m_operation = *aOperation;
  return rv;
}

NS_IMETHODIMP nsMsgOfflineImapOperation::SetOperation(nsOfflineImapOperationType aOperation)
{
  m_operation |= aOperation;
  return m_mdb->SetUint32Property(m_mdbRow, PROP_OPERATION, aOperation);
}

/* void clearOperation (in nsOfflineImapOperationType operation); */
NS_IMETHODIMP nsMsgOfflineImapOperation::ClearOperation(nsOfflineImapOperationType operation)
{
  m_operation &= ~operation;
  switch (operation)
  {
  case kMsgMoved:
  case kAppendTemplate:
  case kAppendDraft:
    m_moveDestination.Adopt(nsCRT::strdup(""));
    break;
  case kMsgCopy:
    m_copyDestinations.RemoveCStringAt(0);
    break;
  }
  return m_mdb->SetUint32Property(m_mdbRow, PROP_OPERATION, m_operation);
}

/* attribute nsMsgKey messageKey; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetMessageKey(nsMsgKey *aMessageKey)
{
  NS_ENSURE_ARG(aMessageKey);
  nsresult rv = m_mdb->GetUint32Property(m_mdbRow, PROP_MESSAGE_KEY, &m_messageKey, 0);
  *aMessageKey = m_messageKey;
  return rv;
}

NS_IMETHODIMP nsMsgOfflineImapOperation::SetMessageKey(nsMsgKey aMessageKey)
{
  m_messageKey = aMessageKey;
  return m_mdb->SetUint32Property(m_mdbRow, PROP_MESSAGE_KEY, m_messageKey);
}


/* attribute imapMessageFlagsType flagOperation; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetFlagOperation(imapMessageFlagsType *aFlagOperation)
{
  NS_ENSURE_ARG(aFlagOperation);
  nsresult rv = m_mdb->GetUint32Property(m_mdbRow, PROP_OPERATION_FLAGS, &m_operationFlags, 0);
  *aFlagOperation = m_operationFlags;
  return NS_OK;
}
NS_IMETHODIMP nsMsgOfflineImapOperation::SetFlagOperation(imapMessageFlagsType aFlagOperation)
{
  SetOperation(kFlagsChanged);
  nsresult rv = SetNewFlags(aFlagOperation);
  NS_ENSURE_SUCCESS(rv, rv);
  m_operationFlags |= aFlagOperation;
  return m_mdb->SetUint32Property(m_mdbRow, PROP_OPERATION_FLAGS, m_operationFlags);
}

/* attribute imapMessageFlagsType flagOperation; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetNewFlags(imapMessageFlagsType *aNewFlags)
{
  NS_ENSURE_ARG(aNewFlags);
  nsresult rv = m_mdb->GetUint32Property(m_mdbRow, PROP_NEW_FLAGS, (PRUint32 *) &m_newFlags, 0);
  *aNewFlags = m_newFlags;
  return NS_OK;
}
NS_IMETHODIMP nsMsgOfflineImapOperation::SetNewFlags(imapMessageFlagsType aNewFlags)
{
  m_newFlags = aNewFlags;
  return m_mdb->SetUint32Property(m_mdbRow, PROP_NEW_FLAGS, m_newFlags);
}


/* attribute string destinationFolderURI; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetDestinationFolderURI(char * *aDestinationFolderURI)
{
  NS_ENSURE_ARG(aDestinationFolderURI);
  nsresult rv = m_mdb->GetProperty(m_mdbRow, PROP_MOVE_DEST_FOLDER_URI, getter_Copies(m_moveDestination));
  *aDestinationFolderURI = nsCRT::strdup(m_moveDestination);
  return (*aDestinationFolderURI) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgOfflineImapOperation::SetDestinationFolderURI(const char * aDestinationFolderURI)
{
  m_moveDestination.Adopt(aDestinationFolderURI ? nsCRT::strdup(aDestinationFolderURI) : 0);
  return m_mdb->SetProperty(m_mdbRow, PROP_MOVE_DEST_FOLDER_URI, aDestinationFolderURI);
}

/* attribute string sourceFolderURI; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetSourceFolderURI(char * *aSourceFolderURI)
{
  NS_ENSURE_ARG(aSourceFolderURI);
  nsresult rv = m_mdb->GetProperty(m_mdbRow, PROP_SRC_FOLDER_URI, getter_Copies(m_sourceFolder));
  *aSourceFolderURI = nsCRT::strdup(m_sourceFolder);
  return NS_OK;
}

NS_IMETHODIMP nsMsgOfflineImapOperation::SetSourceFolderURI(const char * aSourceFolderURI)
{
  m_sourceFolder.Adopt(aSourceFolderURI ? nsCRT::strdup(aSourceFolderURI) : 0);
  SetOperation(kMoveResult);

  return m_mdb->SetProperty(m_mdbRow, PROP_SRC_FOLDER_URI, aSourceFolderURI);
}

NS_IMETHODIMP nsMsgOfflineImapOperation::AddMessageCopyOperation(const char *destinationBox)
{
  SetOperation(kMsgCopy);
  nsCAutoString newDest(destinationBox);
  nsresult rv = GetCopiesFromDB();
  NS_ENSURE_SUCCESS(rv, rv);
  m_copyDestinations.AppendCString(newDest);
  return SetCopiesToDB();
}

// we write out the folders as one string, separated by 0x1.
#define FOLDER_SEP_CHAR '\001'

nsresult nsMsgOfflineImapOperation::GetCopiesFromDB()
{
  nsXPIDLCString copyDests;
  m_copyDestinations.Clear();
  nsresult rv = m_mdb->GetProperty(m_mdbRow, PROP_COPY_DESTS, getter_Copies(copyDests));
  nsCAutoString copyDestsCString((const char *) copyDests);
  // use 0x1 as the delimiter between folder names since it's not a legal character
  if (NS_SUCCEEDED(rv) && copyDestsCString.Length() > 0)
  {
    PRInt32 curCopyDestStart = 0;
    PRInt32 nextCopyDestPos = 0;

    while (nextCopyDestPos != -1)
    {
      nsCString curDest;
      nextCopyDestPos = copyDestsCString.FindChar(FOLDER_SEP_CHAR, PR_FALSE, curCopyDestStart);
      if (nextCopyDestPos > 0)
        copyDestsCString.Mid(curDest, curCopyDestStart, nextCopyDestPos - curCopyDestStart);
      else
        copyDestsCString.Mid(curDest, curCopyDestStart, copyDestsCString.Length() - curCopyDestStart);
      curCopyDestStart = nextCopyDestPos + 1;
      m_copyDestinations.AppendCString(curDest);
    }
  }
  return rv;
}

nsresult nsMsgOfflineImapOperation::SetCopiesToDB()
{
  nsCAutoString copyDests;

  // use 0x1 as the delimiter between folders
  for (PRInt32 i = 0; i < m_copyDestinations.Count(); i++)
  {
    if (i > 0)
      copyDests.Append(FOLDER_SEP_CHAR);
    nsCString *curDest = m_copyDestinations.CStringAt(i);
    copyDests.Append(curDest->get());
  }
  return m_mdb->SetProperty(m_mdbRow, PROP_COPY_DESTS, copyDests.get());
}

/* attribute long numberOfCopies; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetNumberOfCopies(PRInt32 *aNumberOfCopies)
{
  NS_ENSURE_ARG(aNumberOfCopies);
  nsresult rv = GetCopiesFromDB();
  NS_ENSURE_SUCCESS(rv, rv);
  *aNumberOfCopies = m_copyDestinations.Count();
  return NS_OK;
}

/* string getCopyDestination (in long copyIndex); */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetCopyDestination(PRInt32 copyIndex, char **retval)
{
  NS_ENSURE_ARG(retval);
  nsresult rv = GetCopiesFromDB();
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString *copyDest = m_copyDestinations.CStringAt(copyIndex);
  if (copyDest)
  {
    *retval = copyDest->ToNewCString();
    return (*retval) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }
  else
    return NS_ERROR_NULL_POINTER;
}

