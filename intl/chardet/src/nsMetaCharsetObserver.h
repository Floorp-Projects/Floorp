/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsMetaCharsetObserverFactory_h__
#define nsMetaCharsetObserverFactory_h__

#include "nsIFactory.h"
#include "nsIMetaCharsetService.h"
#include "nsIElementObserver.h"
#include "nsIObserver.h"
#include "nsObserverBase.h"
#include "nsWeakReference.h"

//========================================================================== 
//
// Class declaration for the class 
//
//========================================================================== 
class nsMetaCharsetObserver: public nsIElementObserver, 
                             public nsIObserver, 
                             public nsObserverBase,
                             public nsIMetaCharsetService,
                             public nsSupportsWeakReference {
public:
  nsMetaCharsetObserver();
  virtual ~nsMetaCharsetObserver();

  /* methode for nsIElementObserver */
  /*
   *   This method return the tag which the observer care about
   */
  NS_IMETHOD_(const char*)GetTagNameAt(PRUint32 aTagIndex);

  /*
   *   Subject call observer when the parser hit the tag
   *   @param aDocumentID- ID of the document
   *   @param aTag- the tag
   *   @param numOfAttributes - number of attributes
   *   @param nameArray - array of name. 
   *   @param valueArray - array of value
   */
  NS_IMETHOD Notify(PRUint32 aDocumentID, eHTMLTags aTag, PRUint32 numOfAttributes, 
                    const PRUnichar* nameArray[], const PRUnichar* valueArray[]);
  NS_IMETHOD Notify(PRUint32 aDocumentID, const PRUnichar* aTag, PRUint32 numOfAttributes, 
                    const PRUnichar* nameArray[], const PRUnichar* valueArray[]);

  NS_IMETHOD Notify(nsISupports* aDocumentID, const PRUnichar* aTag, const nsStringArray* keys, const nsStringArray* values);

  NS_DECL_ISUPPORTS

  /* methode for nsIObserver */
  NS_DECL_NSIOBSERVER

  /* methode for nsIMetaCharsetService */
  NS_IMETHOD Start();
  NS_IMETHOD End();
 
private:

  NS_IMETHOD Notify(PRUint32 aDocumentID, PRUint32 numOfAttributes, 
                    const PRUnichar* nameArray[], const PRUnichar* valueArray[]);

  NS_IMETHOD Notify(nsISupports* aDocumentID, const nsStringArray* keys, const nsStringArray* values);

  NS_IMETHOD GetCharsetFromCompatibilityTag(const nsStringArray* keys, 
                                            const nsStringArray* values, 
                                            nsAWritableString& aCharset);

  nsCOMPtr<nsICharsetAlias> mAlias;

  PRBool bMetaCharsetObserverStarted;
};

#endif // nsMetaCharsetObserverFactory_h__
