/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsEditingSession_h__
#define nsEditingSession_h__


#ifndef nsWeakReference_h__
#include "nsWeakReference.h"
#endif

#include "nsITimer.h"
#include "nsAutoPtr.h"

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
class nsIDocShell;
class nsIEditorDocShell;
class nsIChannel;
class nsIEditor;
class nsIControllers;

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

  nsIDocShell *   GetDocShellFromWindow(nsIDOMWindow *aWindow);
  nsresult        GetEditorDocShellFromWindow(nsIDOMWindow *aWindow, 
                                              nsIEditorDocShell** outDocShell);
  
  nsresult        SetupEditorCommandController(const char *aControllerClassName,
                                               nsIDOMWindow *aWindow,
                                               nsISupports *aContext,
                                               PRUint32 *aControllerId);

  nsresult        SetContextOnControllerById(nsIControllers* aControllers, 
                                            nsISupports* aContext,
                                            PRUint32 aID);

  nsresult        PrepareForEditing(nsIDOMWindow *aWindow);

  static void     TimerCallback(nsITimer *aTimer, void *aClosure);
  nsCOMPtr<nsITimer>  mLoadBlankDocTimer;
  
  // progress load stuff
  nsresult        StartDocumentLoad(nsIWebProgress *aWebProgress,
                                    bool isToBeMadeEditable);
  nsresult        EndDocumentLoad(nsIWebProgress *aWebProgress, 
                                  nsIChannel* aChannel, nsresult aStatus,
                                  bool isToBeMadeEditable);
  nsresult        StartPageLoad(nsIChannel *aChannel);
  nsresult        EndPageLoad(nsIWebProgress *aWebProgress, 
                              nsIChannel* aChannel, nsresult aStatus);
  
  bool            IsProgressForTargetDocument(nsIWebProgress *aWebProgress);

  void            RemoveEditorControllers(nsIDOMWindow *aWindow);
  void            RemoveWebProgressListener(nsIDOMWindow *aWindow);
  void            RestoreAnimationMode(nsIDOMWindow *aWindow);
  void            RemoveListenersAndControllers(nsIDOMWindow *aWindow,
                                                nsIEditor *aEditor);

protected:

  bool            mDoneSetup;    // have we prepared for editing yet?

  // Used to prevent double creation of editor because nsIWebProgressListener
  //  receives a STATE_STOP notification before the STATE_START 
  //  for our document, so we wait for the STATE_START, then STATE_STOP 
  //  before creating an editor
  bool            mCanCreateEditor; 

  bool            mInteractive;
  bool            mMakeWholeDocumentEditable;

  bool            mDisabledJSAndPlugins;

  // True if scripts were enabled before the editor turned scripts
  // off, otherwise false.
  bool            mScriptsEnabled;

  // True if plugins were enabled before the editor turned plugins
  // off, otherwise false.
  bool            mPluginsEnabled;

  bool            mProgressListenerRegistered;

  // The image animation mode before it was turned off.
  PRUint16        mImageAnimationMode;

  // THE REMAINING MEMBER VARIABLES WILL BECOME A SET WHEN WE EDIT
  // MORE THAN ONE EDITOR PER EDITING SESSION
  nsRefPtr<nsComposerCommandsUpdater> mStateMaintainer;
  
  // Save the editor type so we can create the editor after loading uri
  nsCString       mEditorType; 
  PRUint32        mEditorFlags;
  PRUint32        mEditorStatus;
  PRUint32        mBaseCommandControllerId;
  PRUint32        mDocStateControllerId;
  PRUint32        mHTMLCommandControllerId;

  // Make sure the docshell we use is safe
  nsWeakPtr       mDocShell;

  // See if we can reuse an existing editor
  nsWeakPtr       mExistingEditor;
};



#endif // nsEditingSession_h__
