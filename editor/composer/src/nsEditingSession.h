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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Simon Fraser <sfraser@netscape.com>
 *
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

#ifndef nsEditingSession_h__
#define nsEditingSession_h__


#ifndef nsWeakReference_h__
#include "nsWeakReference.h"
#endif

#include "nsIEditor.h"

#ifndef __gen_nsIDocShell_h__
#include "nsIDocShell.h"
#endif

#ifndef __gen_nsIWebProgressListener_h__
#include "nsIWebProgressListener.h"
#endif

#ifndef __gen_nsIEditingSession_h__
#include "nsIEditingSession.h"
#endif



#define NS_EDITINGSESSION_CID                            \
{ 0xbc26ff01, 0xf2bd, 0x11d4, { 0xa7, 0x3c, 0xe5, 0xa4, 0xb5, 0xa8, 0xbd, 0xfc } }


class nsIWebProgress;
class nsIEditorDocShell;

class nsComposerCommandsUpdater;

class nsEditingSession : public nsIEditingSession,
                         public nsIWebProgressListener,
                         public nsSupportsWeakReference
{
public:

                  nsEditingSession();
  virtual         ~nsEditingSession();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER
  
  // nsIEditingSession
  NS_DECL_NSIEDITINGSESSION


protected:

  nsresult        GetDocShellFromWindow(nsIDOMWindow *inWindow, nsIDocShell** outDocShell);  
  nsresult        GetEditorDocShellFromWindow(nsIDOMWindow *inWindow, nsIEditorDocShell** outDocShell);
  nsresult        SetupFrameControllers(nsIDOMWindow *inWindow);
  
  nsresult        SetEditorOnControllers(nsIDOMWindow *inWindow, nsIEditor* inEditor);

  nsresult        PrepareForEditing();
  
  // progress load stuff
  nsresult        StartDocumentLoad(nsIWebProgress *aWebProgress);
  nsresult        EndDocumentLoad(nsIWebProgress *aWebProgress, nsIChannel* aChannel, nsresult aStatus);
  nsresult        StartPageLoad(nsIWebProgress *aWebProgress);
  nsresult        EndPageLoad(nsIWebProgress *aWebProgress, nsIChannel* aChannel, nsresult aStatus);
  
  PRBool          NotifyingCurrentDocument(nsIWebProgress *aWebProgress);

protected:

  nsWeakPtr       mEditingShell;      // weak ptr back to our editing (web) shell. It owns us.
  PRBool          mDoneSetup;         // have we prepared for editing yet?

  nsComposerCommandsUpdater    *mStateMaintainer;      // we hold the owning ref to this.
  
};



#endif // nsEditingSession_h__
