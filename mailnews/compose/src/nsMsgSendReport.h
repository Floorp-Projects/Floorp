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
 *   Jean-Francois Ducarroz <ducarroz@netscape.com>
 */

#ifndef __nsMsgSendReport_h__
#define __nsMsgSendReport_h__

#include "nsIMsgSendReport.h"
#include "nsString.h"
#include "nsCOMPtr.h"

class nsMsgProcessReport : public nsIMsgProcessReport
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGPROCESSREPORT

  nsMsgProcessReport();
  virtual ~nsMsgProcessReport();

private:
  PRBool    mProceeded;
  nsresult  mError;
  nsString  mMessage;
};


class nsMsgSendReport : public nsIMsgSendReport
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGSENDREPORT

  nsMsgSendReport();
  virtual ~nsMsgSendReport();

private:
  #define SEND_LAST_PROCESS  process_FCC
  nsCOMPtr<nsIMsgProcessReport> mProcessReport[SEND_LAST_PROCESS + 1];
  PRInt32 mDeliveryMode;
  PRInt32 mCurrentProcess;
  PRBool mAlreadyDisplayReport;
};

#endif
