/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Ed Burns <edburns@acm.org>
 */

/*
 * nsActions.h
 */
 
#ifndef PromptActionEvents_h___
#define PromptActionEvents_h___

#include "nsActions.h"
#include "ns_util.h"

struct NativeBrowserControl;

extern jobject gPromptProperties; // declared, defined in CBrowserContainer.cpp

class wsPromptUsernameAndPasswordEvent : public nsActionEvent {
public:
    wsPromptUsernameAndPasswordEvent (NativeBrowserControl *yourInitContext,
                   jobject yourPromptGlobalRef,
                   wsStringStruct *inStrings,
                   PRUint32 savePassword, 
                   PRUnichar **outUser, 
                   PRUnichar **outPwd, 
                   PRBool *_retval);
    void    *       handleEvent    (void);
    
protected:
    NativeBrowserControl *mInitContext;
    jobject mPromptGlobalRef;
    wsStringStruct *mInStrings;
    PRUint32 mSavePassword;
    PRUnichar **mOutUser;
    PRUnichar **mOutPwd;
    PRBool *mRetVal;
};

class wsPromptUniversalDialogEvent : public nsActionEvent {
public:
    wsPromptUniversalDialogEvent (NativeBrowserControl *yourInitContext,
                                  jobject yourPromptGlobalRef,
                                  wsStringStruct *inStrings,
                                  PRUnichar **fieldOne, 
                                  PRUnichar **fieldTwo, 
                                  PRBool *checkboxState,
                                  PRInt32 numButtons,
                                  PRInt32 numFields,
                                  PRInt32 fieldIsPasswd,
                                  PRInt32 *buttonPressed);
    void    *       handleEvent    (void);
    
protected:
    NativeBrowserControl *mInitContext;
    jobject mPromptGlobalRef;
    wsStringStruct *mInStrings;
    PRUnichar **mFieldOne;
    PRUnichar **mFieldTwo;
    PRBool *mCheckboxState;
    PRInt32 mNumButtons;
    PRInt32 mNumFields;
    PRInt32 mFieldIsPasswd;
    PRInt32 *mButtonPressed;
};


#endif /* PromptActionEvents_h___ */

      
// EOF
