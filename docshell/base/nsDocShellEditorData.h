/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
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
#ifndef nsDocShellEditorData_h__
#define nsDocShellEditorData_h__

#ifndef nsCOMPtr_h___
#include "nsCOMPtr.h"
#endif

#ifndef __gen_nsIDocShell_h__
#include "nsIDocShell.h"
#endif

#ifndef __gen_nsIEditingSession_h__
#include "nsIEditingSession.h"
#endif


#include "nsIHTMLDocument.h"
#include "nsIEditor.h"

class nsIDOMWindow;

class nsDocShellEditorData
{
public:

  nsDocShellEditorData(nsIDocShell* inOwningDocShell);
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
