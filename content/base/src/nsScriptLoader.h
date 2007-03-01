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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao <vidur@netscape.com> (original author)
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

/*
 * A class that handles loading and evaluation of <script> elements.
 */

#ifndef __nsScriptLoader_h__
#define __nsScriptLoader_h__

#include "nsCOMPtr.h"
#include "nsIScriptElement.h"
#include "nsIURI.h"
#include "nsCOMArray.h"
#include "nsIDocument.h"
#include "nsIStreamLoader.h"

class nsScriptLoadRequest;

//////////////////////////////////////////////////////////////
// Script loader implementation
//////////////////////////////////////////////////////////////

class nsScriptLoader : public nsIStreamLoaderObserver
{
public:
  nsScriptLoader(nsIDocument* aDocument);
  virtual ~nsScriptLoader();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER

  /**
   * The loader maintains a weak reference to the document with
   * which it is initialized. This call forces the reference to
   * be dropped.
   */
  void DropDocumentReference()
  {
    mDocument = nsnull;
  }

  /**
   * Add an observer for all scripts loaded through this loader.
   *
   * @param aObserver observer for all script processing.
   */
  nsresult AddObserver(nsIScriptLoaderObserver* aObserver)
  {
    return mObservers.AppendObject(aObserver) ? NS_OK :
      NS_ERROR_OUT_OF_MEMORY;
  }

  /**
   * Remove an observer.
   *
   * @param aObserver observer to be removed
   */
  void RemoveObserver(nsIScriptLoaderObserver* aObserver)
  {
    mObservers.RemoveObject(aObserver);
  }
  
  /**
   * Process a script element. This will include both loading the 
   * source of the element if it is not inline and evaluating
   * the script itself.
   *
   * If the script is an inline script that can be executed immediately
   * (i.e. there are no other scripts pending) then ScriptAvailable
   * and ScriptEvaluated will be called before the function returns.
   *
   * If NS_ERROR_HTMLPARSER_BLOCK is returned the script could not be
   * executed immediately. In this case ScriptAvailable is guaranteed
   * to be called at a later point (as well as possibly ScriptEvaluated).
   *
   * @param aElement The element representing the script to be loaded and
   *        evaluated.
   */
  nsresult ProcessScriptElement(nsIScriptElement* aElement);

  /**
   * Gets the currently executing script. This is useful if you want to
   * generate a unique key based on the currently executing script.
   */
  nsIScriptElement* GetCurrentScript()
  {
    return mCurrentScript;
  }

  /**
   * Whether the loader is enabled or not.
   * When disabled, processing of new script elements is disabled. 
   * Any call to ProcessScriptElement() will fail with a return code of
   * NS_ERROR_NOT_AVAILABLE. Note that this DOES NOT disable
   * currently loading or executing scripts.
   */
  PRBool GetEnabled()
  {
    return mEnabled;
  }
  void SetEnabled(PRBool aEnabled)
  {
    if (!mEnabled && aEnabled) {
      ProcessPendingRequestsAsync();
    }
    mEnabled = aEnabled;
  }

  /**
   * Add/remove blocker. Blockers will stop scripts from executing, but not
   * from loading.
   * NOTE! Calling RemoveExecuteBlocker could potentially execute pending
   * scripts synchronously. In other words, it should not be done at 'unsafe'
   * times
   */
  void AddExecuteBlocker()
  {
    if (!mBlockerCount++) {
      mHadPendingScripts = mPendingRequests.Count() != 0;
    }
  }
  void RemoveExecuteBlocker()
  {
    if (!--mBlockerCount) {
      // If there were pending scripts then the newly added scripts will
      // execute once whatever event triggers the pending scripts fires.
      // However, due to synchronous loads and pushed event queues it's
      // possible that the requests that were there have already been processed
      // if so we need to process any new requests asynchronously.
      // Ideally that should be fixed such that it can't happen.
      if (mHadPendingScripts) {
        ProcessPendingRequestsAsync();
      }
      else {
        ProcessPendingRequests();
      }
    }
  }

  /**
   * Convert the given buffer to a UTF-16 string.
   * @param aChannel     Channel corresponding to the data. May be null.
   * @param aData        The data to convert
   * @param aLength      Length of the data
   * @param aHintCharset Hint for the character set (e.g., from a charset
   *                     attribute). May be the empty string.
   * @param aDocument    Document which the data is loaded for. Must not be
   *                     null.
   * @param aString      [out] Data as converted to unicode
   */
  static nsresult ConvertToUTF16(nsIChannel* aChannel, const PRUint8* aData,
                                 PRUint32 aLength,
                                 const nsString& aHintCharset,
                                 nsIDocument* aDocument, nsString& aString);

  /**
   * Processes any pending requests that are ready for processing.
   */
  void ProcessPendingRequests();

protected:
  /**
   * Process any pending requests asyncronously (i.e. off an event) if there
   * are any. Note that this is a no-op if there aren't any currently pending
   * requests.
   */
  virtual void ProcessPendingRequestsAsync();

  PRBool ReadyToExecuteScripts()
  {
    return mEnabled && !mBlockerCount;
  }

  nsresult ProcessRequest(nsScriptLoadRequest* aRequest);
  void FireScriptAvailable(nsresult aResult,
                           nsScriptLoadRequest* aRequest);
  void FireScriptEvaluated(nsresult aResult,
                           nsScriptLoadRequest* aRequest);
  nsresult EvaluateScript(nsScriptLoadRequest* aRequest,
                          const nsAFlatString& aScript);

  nsresult PrepareLoadedRequest(nsScriptLoadRequest* aRequest,
                                nsIStreamLoader* aLoader,
                                nsresult aStatus,
                                PRUint32 aStringLen,
                                const PRUint8* aString);

  nsIDocument* mDocument;                   // [WEAK]
  nsCOMArray<nsIScriptLoaderObserver> mObservers;
  nsCOMArray<nsScriptLoadRequest> mPendingRequests;
  nsCOMPtr<nsIScriptElement> mCurrentScript;
  PRUint32 mBlockerCount;
  PRPackedBool mEnabled;
  PRPackedBool mHadPendingScripts;
};

#endif //__nsScriptLoader_h__
