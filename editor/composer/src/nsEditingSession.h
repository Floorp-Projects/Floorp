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
 *      Simon Fraser <sfraser@netscape.com>
 *
 */

#ifndef nsEditingSession_h__
#define nsEditingSession_h__


#ifndef nsWeakReference_h__
#include "nsWeakReference.h"
#endif

#include "nsIEditor.h"

#ifndef __gen_nsIDocShell_h__
#include "nsIDocShell.h"
#endif

#ifndef __gen_nsIEditingShell_h__
#include "nsIEditingShell.h"
#endif

#ifndef __gen_nsIEditingSession_h__
#include "nsIEditingSession.h"
#endif


#define NS_EDITINGSESSION_CID                            \
{ 0xbc26ff01, 0xf2bd, 0x11d4, { 0xa7, 0x3c, 0xe5, 0xa4, 0xb5, 0xa8, 0xbd, 0xfc } }


class nsEditingSession : public nsIEditingSession,
                         public nsSupportsWeakReference
{
public:

                  nsEditingSession();
  virtual         ~nsEditingSession();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIEditingSession
  NS_DECL_NSIEDITINGSESSION


protected:

  nsresult        GetDocShellFromWindow(nsIDOMWindow *inWindow, nsIDocShell** outDocShell);  
  nsresult        SetupFrameControllers(nsIDOMWindow *inWindow);
  
  nsresult        SetEditorOnControllers(nsIDOMWindow *inWindow, nsIEditor* inEditor);

protected:

  nsWeakPtr       mEditingShell;      // weak ptr back to our editing (web) shell. It owns us.
  
};



#endif // nsEditingSession_h__
