/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Alec Flett <alecf@netscape.com>
 */


#ifndef __nsSmtpDataSource_h
#define __nsSmtpDataSource_h

#include "nscore.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsMsgRDFUtils.h"

class nsSmtpDataSource : public nsIRDFDataSource
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRDFDATASOURCE

  nsSmtpDataSource();
  virtual ~nsSmtpDataSource();
  
private:

  nsresult GetSmtpServerTargets(nsISupportsArray **aResultArray);
  
  static nsresult initGlobalObjects();

  static nsrefcnt gRefCount;
  static nsCOMPtr<nsIRDFResource> kNC_Child;
  static nsCOMPtr<nsIRDFResource> kNC_Name;
  static nsCOMPtr<nsIRDFResource> kNC_Key;
  static nsCOMPtr<nsIRDFResource> kNC_SmtpServers;
  static nsCOMPtr<nsIRDFResource> kNC_IsSessionDefaultServer;
  static nsCOMPtr<nsIRDFResource> kNC_IsDefaultServer;

  static nsCOMPtr<nsIRDFLiteral> kTrueLiteral;
  
  static nsCOMPtr<nsISupportsArray> mServerArcsOut;
  static nsCOMPtr<nsISupportsArray> mServerRootArcsOut;
  nsCOMPtr <nsISupportsArray> mObservers;
  
};

#endif
