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

#ifndef nsCopySupport_h__
#define nsCopySupport_h__

#include "nscore.h"

class nsISelection;
class nsIDocument;
class nsIImageLoadingContent;
class nsIContent;
class nsITransferable;
class nsACString;
class nsAString;
class nsIDOMNode;

class nsCopySupport
{
  // class of static helper functions for copy support
  public:
    static nsresult HTMLCopy(nsISelection *aSel, nsIDocument *aDoc, PRInt16 aClipboardID);
    static nsresult DoHooks(nsIDocument *aDoc, nsITransferable *aTrans,
                            PRBool *aDoPutOnClipboard);
    static nsresult IsPlainTextContext(nsISelection *aSel, nsIDocument *aDoc, PRBool *aIsPlainTextContext);

    // Get the selection, or entire document, in the format specified by the mime type
    // (text/html or text/plain). If aSel is non-null, use it, otherwise get the entire
    // doc.
    static nsresult GetContents(const nsACString& aMimeType, PRUint32 aFlags, nsISelection *aSel, nsIDocument *aDoc, nsAString& outdata);
    
    static nsresult ImageCopy(nsIImageLoadingContent* aImageElement,
                              PRInt32 aCopyFlags);

    // Given the current selection, find the target that
    // before[copy,cut,paste] and [copy,cut,paste] events will fire on.
    static nsresult GetClipboardEventTarget(nsISelection *aSel,
                                            nsIDOMNode **aEventTarget);

    // Get the selection as a transferable. Similar to HTMLCopy except does
    // not deal with the clipboard.
    static nsresult GetTransferableForSelection(nsISelection * aSelection,
                                                nsIDocument * aDocument,
                                                nsITransferable ** aTransferable);
};

#endif
