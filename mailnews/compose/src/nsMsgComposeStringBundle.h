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

#ifndef _nsMsgComposeStringBundle_H_
#define _nsMsgComposeStringBundle_H_

#include "nscore.h"

//
// The defines needed for error conditions. The corresponding strings
// are defined in composebe_en.properties
// 
typedef enum {
  NS_MSG_UNABLE_TO_OPEN_FILE = 12500,
  NS_MSG_UNABLE_TO_OPEN_TMP_FILE,
  NS_MSG_UNABLE_TO_SAVE_TEMPLATE,
  NS_MSG_UNABLE_TO_SAVE_DRAFT,
  NS_MSG_LOAD_ATTACHMNTS,
  NS_MSG_LOAD_ATTACHMNT,
  NS_MSG_COULDNT_OPEN_FCC_FILE,
  NS_MSG_CANT_POST_TO_MULTIPLE_NEWS_HOSTS,
  NS_MSG_ASSEMB_DONE_MSG,
  NS_MSG_ASSEMBLING_MSG,
  NS_MSG_NO_SENDER,
  NS_MSG_NO_RECIPIENTS,
  NS_MSG_ERROR_WRITING_FILE
} nsMsgComposeErrorIDs;

NS_BEGIN_EXTERN_C

char     *ComposeBEGetStringByID(PRInt32 stringID);

NS_END_EXTERN_C

#endif /* _nsMsgComposeStringBundle_H_ */
