/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef nsIEditorStyleSheets_h__
#define nsIEditorStyleSheets_h__


#include "nsISupports.h"

class nsString;
class nsICSSStyleSheet;

#define NS_IEDITORSTYLESHEETS_IID                  \
{ /* 4805e682-49b9-11d3-9ce4-ed60bd6cb5bc} */      \
0x4805e682, 0x49b9, 0x11d3,                        \
{ 0x9c, 0xe4, 0xed, 0x60, 0xbd, 0x6c, 0xb5, 0xbc } }


class nsIEditorStyleSheets : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEDITORSTYLESHEETS_IID; return iid; }

  /** load and apply the style sheet, specified by aURL, to
    * the editor's document. This can involve asynchronous
    * network I/O
    * @param aURL         The style sheet to be loaded and applied.
    * @param aStyleSheet  Optional: if not null, return the style sheet created from aURL
    */
  NS_IMETHOD ApplyStyleSheet(const nsAReadableString& aURL, nsICSSStyleSheet **aStyleSheet)=0;

  /** load and apply an Override style sheet, specified by aURL, to
    * the editor's document. 
    * IMPORTANT: This is assumed to be synchronous:
    *            URL is a local file with no @import used
    * This action is not undoable.
    * It is not intended for use by "user", only editor developers
    *   to change display behavior for editing (like showing special cursors)
    *   that will not be affected by loading other "document" style sheets
    *   loaded using ApplyStyleSheet.
    *
    * @param aURL         The style sheet to be loaded and applied.
    * @param aStyleSheet  Optional: if not null, return the style sheet created from aURL
    */
  NS_IMETHOD ApplyOverrideStyleSheet(const nsAReadableString& aURL, nsICSSStyleSheet **aStyleSheet)=0;

  /** Add the given Style Sheet to the editor's document
    * This is always synchronous
    * @param aSheet  The style sheet to be  applied.
    */
  NS_IMETHOD AddStyleSheet(nsICSSStyleSheet* aSheet)=0;

  /** Remove the given Style Sheet from the editor's document
    * This is always synchronous
    * @param aSheet  The style sheet to be removed
    */
  NS_IMETHOD RemoveStyleSheet(nsICSSStyleSheet* aSheet)=0;

  /** Remove the given Override Style Sheet from the editor's document
    * This is always synchronous
    * @param aSheet  The style sheet to be removed.
    */
  NS_IMETHOD RemoveOverrideStyleSheet(nsICSSStyleSheet* aSheet)=0;

};


#endif // nsIEditorStyleSheets_h__

