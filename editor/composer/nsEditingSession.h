/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEditingSession_h__
#define nsEditingSession_h__

#include "nsCOMPtr.h"               // for nsCOMPtr
#include "nsISupportsImpl.h"        // for NS_DECL_ISUPPORTS
#include "nsIWeakReferenceUtils.h"  // for nsWeakPtr
#include "nsWeakReference.h"        // for nsSupportsWeakReference, etc
#include "nscore.h"                 // for nsresult

#ifndef __gen_nsIWebProgressListener_h__
#  include "nsIWebProgressListener.h"
#endif

#ifndef __gen_nsIEditingSession_h__
#  include "nsIEditingSession.h"  // for NS_DECL_NSIEDITINGSESSION, etc
#endif

#include "nsString.h"  // for nsCString

class mozIDOMWindowProxy;
class nsBaseCommandController;
class nsIDOMWindow;
class nsISupports;
class nsITimer;
class nsIChannel;
class nsIControllers;
class nsIDocShell;
class nsIWebProgress;
class nsIPIDOMWindowOuter;
class nsIPIDOMWindowInner;

namespace mozilla {
class ComposerCommandsUpdater;
class HTMLEditor;
}  // namespace mozilla

class nsEditingSession final : public nsIEditingSession,
                               public nsIWebProgressListener,
                               public nsSupportsWeakReference {
 public:
  nsEditingSession();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER

  // nsIEditingSession
  NS_DECL_NSIEDITINGSESSION

  /**
   * Removes all the editor's controllers/listeners etc and makes the window
   * uneditable.
   */
  nsresult DetachFromWindow(nsPIDOMWindowOuter* aWindow);

  /**
   * Undos DetachFromWindow(), reattaches this editing session/editor
   * to the window.
   */
  nsresult ReattachToWindow(nsPIDOMWindowOuter* aWindow);

 protected:
  virtual ~nsEditingSession();

  typedef already_AddRefed<nsBaseCommandController> (*ControllerCreatorFn)();

  nsresult SetupEditorCommandController(
      ControllerCreatorFn aControllerCreatorFn, mozIDOMWindowProxy* aWindow,
      nsISupports* aContext, uint32_t* aControllerId);

  nsresult SetContextOnControllerById(nsIControllers* aControllers,
                                      nsISupports* aContext, uint32_t aID);

  /**
   *  Set the editor on the controller(s) for this window
   */
  nsresult SetEditorOnControllers(nsPIDOMWindowOuter& aWindow,
                                  mozilla::HTMLEditor* aEditor);

  /**
   *  Setup editor and related support objects
   */
  MOZ_CAN_RUN_SCRIPT nsresult SetupEditorOnWindow(nsPIDOMWindowOuter& aWindow);

  nsresult PrepareForEditing(nsPIDOMWindowOuter* aWindow);

  static void TimerCallback(nsITimer* aTimer, void* aClosure);
  nsCOMPtr<nsITimer> mLoadBlankDocTimer;

  // progress load stuff
  nsresult StartDocumentLoad(nsIWebProgress* aWebProgress,
                             bool isToBeMadeEditable);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult EndDocumentLoad(nsIWebProgress* aWebProgress, nsIChannel* aChannel,
                           nsresult aStatus, bool isToBeMadeEditable);
  nsresult StartPageLoad(nsIChannel* aChannel);
  nsresult EndPageLoad(nsIWebProgress* aWebProgress, nsIChannel* aChannel,
                       nsresult aStatus);

  bool IsProgressForTargetDocument(nsIWebProgress* aWebProgress);

  void RemoveEditorControllers(nsPIDOMWindowOuter* aWindow);
  void RemoveWebProgressListener(nsPIDOMWindowOuter* aWindow);
  void RestoreAnimationMode(nsPIDOMWindowOuter* aWindow);
  void RemoveListenersAndControllers(nsPIDOMWindowOuter* aWindow,
                                     mozilla::HTMLEditor* aHTMLEditor);

  /**
   * Disable scripts and plugins in aDocShell.
   */
  nsresult DisableJSAndPlugins(nsPIDOMWindowInner* aWindow);

  /**
   * Restore JS and plugins (enable/disable them) according to the state they
   * were before the last call to disableJSAndPlugins.
   */
  nsresult RestoreJSAndPlugins(nsPIDOMWindowInner* aWindow);

 protected:
  bool mDoneSetup;  // have we prepared for editing yet?

  // Used to prevent double creation of editor because nsIWebProgressListener
  //  receives a STATE_STOP notification before the STATE_START
  //  for our document, so we wait for the STATE_START, then STATE_STOP
  //  before creating an editor
  bool mCanCreateEditor;

  bool mInteractive;
  bool mMakeWholeDocumentEditable;

  bool mDisabledJSAndPlugins;

  // True if scripts were enabled before the editor turned scripts
  // off, otherwise false.
  bool mScriptsEnabled;

  // True if plugins were enabled before the editor turned plugins
  // off, otherwise false.
  bool mPluginsEnabled;

  bool mProgressListenerRegistered;

  // The image animation mode before it was turned off.
  uint16_t mImageAnimationMode;

  // THE REMAINING MEMBER VARIABLES WILL BECOME A SET WHEN WE EDIT
  // MORE THAN ONE EDITOR PER EDITING SESSION
  RefPtr<mozilla::ComposerCommandsUpdater> mComposerCommandsUpdater;

  // Save the editor type so we can create the editor after loading uri
  nsCString mEditorType;
  uint32_t mEditorFlags;
  uint32_t mEditorStatus;
  uint32_t mBaseCommandControllerId;
  uint32_t mDocStateControllerId;
  uint32_t mHTMLCommandControllerId;

  // Make sure the docshell we use is safe
  nsWeakPtr mDocShell;

  // See if we can reuse an existing editor
  nsWeakPtr mExistingEditor;
};

#endif  // nsEditingSession_h__
