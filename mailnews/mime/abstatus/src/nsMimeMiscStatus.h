/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef __nsMimeMiscStatus_h__
#define __nsMimeMiscStatus_h__

#include "nsIMimeMiscStatus.h"

#define NS_MIME_MISC_STATUS_CID \
  { 0x842cc263, 0x5255, 0x11d3,   \
  { 0x82, 0xb8, 0x44, 0x45, 0x53, 0x54, 0x0, 0x9 } };

class nsMimeMiscStatus: public nsIMimeMiscStatus {
public: 
  nsMimeMiscStatus ();
  virtual       ~nsMimeMiscStatus (void);

  // nsISupports interface
  NS_DECL_ISUPPORTS
     
  NS_IMETHOD GetGlobalXULandJS(char **_retval);

  NS_IMETHOD GetIndividualXUL(const char *aHeader, const char *aName, 
                              const char *aEmail, char **_retval);

  NS_IMETHOD GetMiscStatus(const char *aName, const char *aEmail, PRInt32 *_retval);

  NS_IMETHOD GetImageURL(PRInt32 aStatus, char **_retval);
  
private:
 
};

/* this function will be used by the factory to generate an class access object....*/
extern nsresult NS_NewMimeMiscStatus(const nsIID& iid, void **result);

#endif /* __nsMimeMiscStatus_h__ */
