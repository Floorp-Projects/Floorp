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
#include "nsITimer.h"

#ifndef __gen_nsIControllers_h__
#include "nsIControllers.h"
#endif

#ifndef __gen_nsIDocShell_h__
#include "nsIDocShell.h"
#endif

#ifndef __gen_nsIWebProgressListener_h__
#include "nsIWebProgressListener.h"
#endif

#ifndef __gen_nsIEditingSession_h__
#include "nsIEditingSession.h"
#endif

#include "nsString.h"

#define NS_EDITINGSESSION_CID                            \
{ 0xbc26ff01, 0xf2bd, 0x11d4, { 0xa7, 0x3c, 0xe5, 0xa4, 0xb5, 0xa8, 0xbd, 0xfc } }


class nsIWebProgress;
class nsIEditorDocShell;
class nsIChannel;

#ifndef FULL_EDITOR_HTML_SUPPORT
class nsEditorParserObserver;
#endif


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

  nsresult        GetDocShellFromWindow(nsIDOMWindow *aWindow, 
                                        nsIDocShell** outDocShell);
  nsresult        GetEditorDocShellFromWindow(nsIDOMWindow *aWindow, 
                                              nsIEditorDocShell** outDocShell);
  
  nsresult        SetupEditorCommandController(const char *aControllerClassName,
                                               nsIDOMWindow *aWindow,
                                               nsISupports *aRefCon,
                                               PRUint32 *aControllerId);
  nsresult        SetEditorOnControllers(nsIDOMWindow *aWindow, 
                                         nsIEditor* aEditor);
  nsresult        SetRefConOnControllerById(nsIControllers* aControllers, 
                                            nsISupports* aRefCon,
                                            PRUint32 aID);

  nsresult        PrepareForEditing();

  static void     TimerCallback(nsITimer *aTimer, void *aClosure);
  nsCOMPtr<nsITimer>  mLoadBlankDocTimer;
  
  // progress load stuff
  nsresult        StartDocumentLoad(nsIWebProgress *aWebProgress);
  nsresult        EndDocumentLoad(nsIWebProgress *aWebProgress, 
                                  nsIChannel* aChannel, nsresult aStatus);
  nsresult        StartPageLoad(nsIChannel *aChannel);
  nsresult        EndPageLoad(nsIWebProgress *aWebProgress, 
                              nsIChannel* aChannel, nsresult aStatus);
  
  PRBool          NotifyingCurrentDocument(nsIWebProgress *aWebProgress);

protected:

  nsWeakPtr       mEditingShell;// weak ptr to the editing (web) shell that owns us
  PRBool          mDoneSetup;    // have we prepared for editing yet?

  // Used to prevent double creation of editor because nsIWebProgressListener
  //  receives a STATE_STOP notification before the STATE_START 
  //  for our document, so we wait for the STATE_START, then STATE_STOP 
  //  before creating an editor
  PRBool          mCanCreateEditor; 

  // THE REMAINING MEMBER VARIABLES WILL BECOME A SET WHEN WE EDIT
  // MORE THAN ONE EDITOR PER EDITING SESSION
  nsCOMPtr<nsISupports> mStateMaintainer;  // we hold the owning ref to this
  
  // Save the editor type so we can create the editor after loading uri
  nsCString       mEditorType; 
  PRUint32        mEditorFlags;
  PRUint32        mEditorStatus;
  PRUint32        mBaseCommandControllerId;
  PRUint32        mDocStateControllerId;
  PRUint32        mHTMLCommandControllerId;
};



#endif // nsEditingSession_h__
