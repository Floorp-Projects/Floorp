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

#ifndef _MsgCompose_H_
#define _MsgCompose_H_

#include "msgCore.h"
#include "prprf.h" /* should be defined into msgCore.h? */
#include "net.h" /* should be defined into msgCore.h? */
#include "MailNewsTypes.h"
#include "intl_csi.h"
#include "msgcom.h"

#include "MsgCompGlue.h"
#include "nsMsgHeaderMasks.h"
#include "nsMsgFolderFlags.h"
#include "nsMsgCompFields.h"
#include "nsIMsgCompose.h"
#include "nsMsgSend.h"

/* The MSG_REPLY_TYPE shares the same space as MSG_CommandType, to avoid
   possible weird errors, but is restricted to the `composition' commands
   (MSG_ReplyToSender through MSG_ForwardMessage.)
 */
typedef MSG_CommandType MSG_REPLY_TYPE;


struct MSG_AttachedFile;
typedef struct PrintSetup_ PrintSetup;
typedef struct _XPDialogState XPDialogState;

HJ08142
class MSG_NewsHost;
class MSG_HTMLRecipients;


#endif /* _MsgCompose_H_ */
