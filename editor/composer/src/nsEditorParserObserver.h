/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   steve clark <buster@netscape.com>
 */

#include "nsIElementObserver.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"

#include "nsWeakReference.h"
#include "nsString.h"
#include "nsVoidArray.h"

class nsEditorParserObserver : public nsSupportsWeakReference,
                               public nsIElementObserver,
                               public nsIObserver
{
public:

                            nsEditorParserObserver();
  virtual                   ~nsEditorParserObserver();

  NS_DECL_ISUPPORTS

  /* method for nsIElementObserver */
  NS_IMETHOD_(const char*)  GetTagNameAt(PRUint32 aTagIndex);
  NS_IMETHOD                Notify(PRUint32 aDocumentID, eHTMLTags aTag, PRUint32 numOfAttributes, 
                                    const PRUnichar* nameArray[], const PRUnichar* valueArray[]);
  NS_IMETHOD                Notify(PRUint32 aDocumentID, const PRUnichar* aTag, PRUint32 numOfAttributes, 
                                    const PRUnichar* nameArray[], const PRUnichar* valueArray[]);
  NS_IMETHOD                Notify(nsISupports* aDocumentID, const PRUnichar* aTag, 
                                    const nsStringArray* aKeys, const nsStringArray* aValues);

  /* methods for nsIObserver */
  NS_DECL_NSIOBSERVER

  /* begin and end observing */
  NS_IMETHOD                Start();
  NS_IMETHOD                End();

  /* query, did we find a bad tag? */
  NS_IMETHOD                GetBadTagFound(PRBool *aFound);

  /* register a tag to watch for */
  NS_IMETHOD                RegisterTagToWatch(const char* tagName);

protected:

  virtual void              Notify();
  
protected:

  PRBool                    mBadTagFound;
  
  nsCStringArray            mWatchTags;
  
};


