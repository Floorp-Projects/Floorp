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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __GeckoUtils_h__
#define __GeckoUtils_h__

#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIJSContextStack.h"

class nsIDOMWindow;
class nsIDOMNode;
class nsIDOMElement;
class nsIDocShell;
class nsIURI;
class nsIEditor;


class GeckoUtils
{
  public:

    static void GatherTextUnder(nsIDOMNode* aNode, nsString& aResult);
    static void GetEnclosingLinkElementAndHref(nsIDOMNode* aNode, nsIDOMElement** aLinkContent, nsString& aHref);

    // Returns whether or not the given protocol ('http', 'file', 'mailto', etc.)
    // is handled internally. Returns PR_TRUE in error cases.
    static PRBool isProtocolInternal(const char* aProtocol);

    /* Ouputs the docshell |aDocShell|'s URI as a nsACString. */
    static void GetURIForDocShell(nsIDocShell* aDocShell, nsACString& aURI);
  
    // Finds the anchor node for the selection in the given editor
    static void GetAnchorNodeFromSelection(nsIEditor* inEditor, nsIDOMNode** outAnchorNode, PRInt32* outAnchorOffset);
    
    /* Given a URI, and a docshell node, will traverse the tree looking for the docshell with the
       given URI.  This is used for example when unblocking popups, because the popup "windows" are docshells
       found somewhere in a document's docshell tree.  NOTE: Addrefs the found docshell! 
    */
    static void FindDocShellForURI(nsIURI *aURI, nsIDocShell *aRoot, nsIDocShell **outMatch);
    
    /* Finds the preferred size (ie the minimum size where scrollbars are not needed) of the content window. */
    static void GetIntrisicSize(nsIDOMWindow* aWindow, PRInt32* outWidth, PRInt32* outHeight);
};

/* Stack-based utility that will push a null JSContext onto the JS stack during the
   length of its lifetime.

   For example, this is needed when some unprivileged JS code executes from a webpage. 
   If we try to call into Gecko then, the current JSContext will be the webpage, and so 
   Gecko might deny *us* the right to do something. For this reason we push a null JSContext, 
   to make sure that whatever we want to do will be allowed. 
*/
class StNullJSContextScope {
public:
  StNullJSContextScope(nsresult *rv) {
    mStack = do_GetService("@mozilla.org/js/xpc/ContextStack;1", rv);
    if (NS_SUCCEEDED(*rv) && mStack)
      *rv = mStack->Push(nsnull);
  }
  
  ~StNullJSContextScope() {
    if (mStack) {
      JSContext *ctx;
      mStack->Pop(&ctx);
      NS_ASSERTION(!ctx, "Popped JSContext not null!");
    }
  }
  
private:
  nsCOMPtr<nsIJSContextStack> mStack;
};
    
#endif
