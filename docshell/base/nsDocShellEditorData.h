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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
 */
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

protected:              

  nsresult    GetOrCreateEditingSession(nsIEditingSession **outEditingSession);

protected:

  nsIDocShell*                mDocShell;        // the doc shell that owns us. Weak ref, since it always outlives us.  
  
  nsCOMPtr<nsIEditingSession> mEditingSession;  // only present for the content root docShell. Session is owned here

  PRBool                      mMakeEditable;    // indicates whether to make an editor after a url load
  nsCOMPtr<nsIEditor>         mEditor;          // if this frame is editable, store editor here. Editor is owned here.

};


#endif // nsDocShellEditorData_h__
