/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIImapFlagAndUidState_h___
#define nsIImapFlagAndUidState_h___

// perhaps this should be an interface, but mainly I just want to avoid linking
// it into the test app - it's a really silly little class.
class nsIImapFlagAndUidState 
{
public:
    NS_IMETHOD				GetNumberOfMessages(PRInt32 *result) = 0;
    NS_IMETHOD				GetUidOfMessage(PRInt32 zeroBasedIndex, PRUint32 *result) = 0;
    NS_IMETHOD				GetMessageFlags(PRInt32 zeroBasedIndex, imapMessageFlagsType *resul) = 0;
};




#endif
