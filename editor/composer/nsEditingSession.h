/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEditingSession_h__
#define nsEditingSession_h__


#ifndef nsWeakReference_h__
#include "nsWeakReference.h"            // for nsSupportsWeakReference, etc
#endif

#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsISupportsImpl.h"            // for NS_DECL_ISUPPORTS
#include "nsIWeakReferenceUtils.h"      // for nsWeakPtr
#include "nsWeakReference.h"            // for nsSupportsWeakReference, etc
#include "nscore.h"                     // for nsresult

#ifndef __gen_nsIWebProgressListener_h__
#include "nsIWebProgressListener.h"
#endif

#ifndef __gen_nsIEditingSession_h__
#include "nsIEditingSession.h"          // for NS_DECL_NSIEDITINGSESSION, etc
#endif

#include "nsString.h"                   // for nsCString

class nsIDOMWindow;
class nsISupports;
class nsITimer;

#define NS_EDITINGSESSION_CID                            \
{ 0xbc26ff01, 0xf2bd, 0x11d4, { 0xa7, 0x3c, 0xe5, 0xa4, 0xb5, 0xa8, 0xbd, 0xfc } }


class nsComposerCommandsUpdater;
class nsIChannel;
class nsIControllers;
class nsIDocShell;
class nsIEditor;
class nsIWebProgress;

class nsEditingSession : public nsIEditingSession,
                         public nsIWebProgressListener,
                         public nsSupportsWeakReference
{
public:

  nsEditingSession();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER
  
  // nsIEditingSession
  NS_DECL_NSIEDITINGSESSION

protected:
  virtual         ~nsEditingSession();

  nsIDocShell *   GetDocShellFromWindow(nsIDOMWindow *aWindow);
  
  nsresult        SetupEditorCommandController(const char *aControllerClassName,
                                               nsIDOMWindow *aWindow,
                                               nsISupports *aContext,
                                               uint32_t *aControllerId);

  nsresult        SetContextOnControllerById(nsIControllers* aControllers, 
                                            nsISupports* aContext,
                                            uint32_t aID);

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
  uint16_t        mImageAnimationMode;

  // THE REMAINING MEMBER VARIABLES WILL BECOME A SET WHEN WE EDIT
  // MORE THAN ONE EDITOR PER EDITING SESSION
  nsRefPtr<nsComposerCommandsUpdater> mStateMaintainer;
  
  // Save the editor type so we can create the editor after loading uri
  nsCString       mEditorType; 
  uint32_t        mEditorFlags;
  uint32_t        mEditorStatus;
  uint32_t        mBaseCommandControllerId;
  uint32_t        mDocStateControllerId;
  uint32_t        mHTMLCommandControllerId;

  // Make sure the docshell we use is safe
  nsWeakPtr       mDocShell;

  // See if we can reuse an existing editor
  nsWeakPtr       mExistingEditor;
};



#endif // nsEditingSession_h__
