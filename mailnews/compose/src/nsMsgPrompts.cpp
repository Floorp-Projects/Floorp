/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsMsgCopy.h"
#include "nsIPref.h"
#include "nsMsgCompPrefs.h"
#include "nsMsgPrompts.h"
#include "nsINetSupportDialogService.h"
#include "nsMsgComposeStringBundle.h"

static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);

nsresult
nsMsgDisplayMessageByID(PRInt32 msgID)
{
  nsresult rv;

  char *msg = ComposeBEGetStringByID(msgID);
  if (!msg)
    return NS_ERROR_FAILURE;

  NS_WITH_SERVICE(nsINetSupportDialogService, dialog, kNetSupportDialogCID, &rv);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsAutoString alertText(msg);
  if (dialog) 
  {
    rv = dialog->Alert(alertText);
  }

  PR_FREEIF(msg);
  return NS_OK;
}

nsresult
nsMsgDisplayMessageByString(char *msg)
{
  nsresult rv;

  if ((!msg) || (!*msg))
    return NS_ERROR_FAILURE;

  NS_WITH_SERVICE(nsINetSupportDialogService, dialog, kNetSupportDialogCID, &rv);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsAutoString alertText(msg);
  if (dialog) 
  {
#ifdef BUG_7770_IS_FIXED
    rv = dialog->Alert(alertText);
#else
#ifdef DEBUG
    printf("%s",msg);
#endif
#endif    
  }

  return NS_OK;
}

nsresult
nsMsgAskBooleanQuestionByID(PRInt32 msgID, PRBool *answer)
{
  nsresult rv;
  PRInt32 result;

  char *msg = ComposeBEGetStringByID(msgID);
  if (!msg)
    return NS_ERROR_FAILURE;

  NS_WITH_SERVICE(nsINetSupportDialogService, dialog, kNetSupportDialogCID, &rv);  
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  
  nsAutoString confirmText(msg);
  if (dialog) 
  {
    rv = dialog->Confirm(confirmText, &result);
    if (result == 1) 
    {
      *answer = PR_TRUE;
    }
    else 
    {
      *answer = PR_FALSE;
    }
  }

  PR_FREEIF(msg);
  return NS_OK;
}     

nsresult
nsMsgAskBooleanQuestionByID(char *msg, PRBool *answer)
{
  nsresult rv;
  PRInt32 result;

  if ((!msg) || (!*msg))
    return NS_ERROR_FAILURE;

  NS_WITH_SERVICE(nsINetSupportDialogService, dialog, kNetSupportDialogCID, &rv);  
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  
  nsAutoString confirmText(msg);
  if (dialog) 
  {
    rv = dialog->Confirm(confirmText, &result);
    if (result == 1) 
    {
      *answer = PR_TRUE;
    }
    else 
    {
      *answer = PR_FALSE;
    }
  }

  PR_FREEIF(msg);
  return NS_OK;
}     
