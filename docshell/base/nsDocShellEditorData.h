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


#include "nsIEditor.h"


// a non-XPCOM class that is used to store per-docshell editor-related
// data.

class nsDocShellEditorData
{
public:

              nsDocShellEditorData(nsIDocShell* inOwningDocShell);
              ~nsDocShellEditorData();
              

  // set a flag to say this frame should be editable when the next url loads
  nsresult    MakeEditable(PRBool inWaitForUriLoad);
  
  PRBool      GetEditable();
  
              // actually create the editor for this docShell
  nsresult    CreateEditor();
  
              // get the editing session. The editing session always lives on the content
              // root docShell; this call may crawl up the frame tree to find it.
  nsresult    GetEditingSession(nsIEditingSession **outEditingSession);
  
              // get the editor for this docShell. May return null but NS_OK
  nsresult    GetEditor(nsIEditor **outEditor);
  
              // set the editor on this docShell
  nsresult    SetEditor(nsIEditor *inEditor);

  // Tear down the editor on this docshell, if any.
  void        TearDownEditor();

protected:              

  nsresult    EnsureEditingSession();

protected:

  nsIDocShell*                mDocShell;        // the doc shell that owns us. Weak ref, since it always outlives us.  
  
  nsCOMPtr<nsIEditingSession> mEditingSession;  // only present for the content root docShell. Session is owned here

  PRBool                      mMakeEditable;    // indicates whether to make an editor after a url load
  nsCOMPtr<nsIEditor>         mEditor;          // if this frame is editable, store editor here. Editor is owned here.

};


#endif // nsDocShellEditorData_h__
