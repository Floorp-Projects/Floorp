/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Travis Bogard <travis@netscape.com>
 */

#ifndef nsHTMLDocShell_h__
#define nsHTMLDocShell_h__

#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsIDOMDocument.h"

#include "nsCHTMLDocShell.h"
#include "nsDocShellBase.h"

class nsHTMLDocShell : public nsDocShellBase, public nsIHTMLDocShell
{
public:
   NS_DECL_ISUPPORTS

   NS_IMETHOD CanHandleContentType(const PRUnichar* contentType, PRBool* canHandle);


   NS_DECL_NSIHTMLDOCSHELL

   //nsIDocShellEdit Overrides
   NS_IMETHOD SelectAll();

   static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void** ppv);

protected:
   nsHTMLDocShell();
   virtual ~nsHTMLDocShell();

   nsresult GetPrimaryFrameFor(nsIContent* content, nsIFrame** frame);

protected:
   PRBool   mAllowPlugins;
   PRInt32  mMarginWidth;
   PRInt32  mMarginHeight;
   PRBool   mIsFrame;
};

#endif /* nsHTMLDocShell_h__ */
