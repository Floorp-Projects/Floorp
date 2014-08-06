/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDocShellEditorData_h__
#define nsDocShellEditorData_h__

#ifndef nsCOMPtr_h___
#include "nsCOMPtr.h"
#endif

#include "nsIHTMLDocument.h"

class nsIDocShell;
class nsIEditingSession;
class nsIEditor;

class nsDocShellEditorData
{
public:

  explicit nsDocShellEditorData(nsIDocShell* inOwningDocShell);
  ~nsDocShellEditorData();

  nsresult MakeEditable(bool inWaitForUriLoad);
  bool GetEditable();
  nsresult CreateEditor();
  nsresult GetEditingSession(nsIEditingSession **outEditingSession);
  nsresult GetEditor(nsIEditor **outEditor);
  nsresult SetEditor(nsIEditor *inEditor);
  void TearDownEditor();
  nsresult DetachFromWindow();
  nsresult ReattachToWindow(nsIDocShell *aDocShell);
  bool WaitingForLoad() const { return mMakeEditable; }

protected:

  nsresult EnsureEditingSession();

  // The doc shell that owns us. Weak ref, since it always outlives us.  
  nsIDocShell* mDocShell;

  // Only present for the content root docShell. Session is owned here.
  nsCOMPtr<nsIEditingSession> mEditingSession;

  // Indicates whether to make an editor after a url load.
  bool mMakeEditable;
  
  // If this frame is editable, store editor here. Editor is owned here.
  nsCOMPtr<nsIEditor> mEditor;

  // Denotes if the editor is detached from its window. The editor is detached
  // while it's stored in the session history bfcache.
  bool mIsDetached;

  // Backup for mMakeEditable while the editor is detached.
  bool mDetachedMakeEditable;

  // Backup for the corresponding nsIHTMLDocument's  editing state while
  // the editor is detached.
  nsIHTMLDocument::EditingState mDetachedEditingState;

};


#endif // nsDocShellEditorData_h__
