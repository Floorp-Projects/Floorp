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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick McManus <mcmanus@ducksong.com>
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

#ifndef nsHTMLDNSPrefetch_h___
#define nsHTMLDNSPrefetch_h___

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsString.h"

#include "nsIDNSListener.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"
#include "nsIObserver.h"

class nsIDocument;
class nsITimer;
namespace mozilla {
namespace dom {
class Link;
} // namespace dom
} // namespace mozilla

namespace mozilla {
namespace net {
class NeckoParent;
} // namespace net
} // namespace mozilla

class nsHTMLDNSPrefetch 
{
public:
  // The required aDocument parameter is the context requesting the prefetch - under
  // certain circumstances (e.g. headers, or security context) associated with
  // the context the prefetch will not be performed. 
  static bool     IsAllowed(nsIDocument *aDocument);
 
  static nsresult Initialize();
  static nsresult Shutdown();
  
  // Call one of the Prefetch* methods to start the lookup.
  //
  // The URI versions will defer DNS lookup until pageload is
  // complete, while the string versions submit the lookup to 
  // the DNS system immediately. The URI version is somewhat lighter
  // weight, but its request is also more likely to be dropped due to a 
  // full queue and it may only be used from the main thread.

  static nsresult PrefetchHigh(mozilla::dom::Link *aElement);
  static nsresult PrefetchMedium(mozilla::dom::Link *aElement);
  static nsresult PrefetchLow(mozilla::dom::Link *aElement);
  static nsresult PrefetchHigh(nsAString &host);
  static nsresult PrefetchMedium(nsAString &host);
  static nsresult PrefetchLow(nsAString &host);

private:
  static nsresult Prefetch(nsAString &host, PRUint16 flags);
  static nsresult Prefetch(mozilla::dom::Link *aElement, PRUint16 flags);
  
public:
  class nsListener : public nsIDNSListener
  {
    // This class exists to give a safe callback no-op DNSListener
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDNSLISTENER

    nsListener()  {}
  private:
    ~nsListener() {}
  };
  
  class nsDeferrals : public nsIWebProgressListener
                    , public nsSupportsWeakReference
                    , public nsIObserver
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIOBSERVER

    nsDeferrals();
    
    void Activate();
    nsresult Add(PRUint16 flags, mozilla::dom::Link *aElement);
    
  private:
    ~nsDeferrals();
    void Flush();
    
    void SubmitQueue();
    
    PRUint16                  mHead;
    PRUint16                  mTail;
    PRUint32                  mActiveLoaderCount;

    nsCOMPtr<nsITimer>        mTimer;
    bool                      mTimerArmed;
    static void Tick(nsITimer *aTimer, void *aClosure);
    
    static const int          sMaxDeferred = 512;  // keep power of 2 for masking
    static const int          sMaxDeferredMask = (sMaxDeferred - 1);
    
    struct deferred_entry
    {
      PRUint16                         mFlags;
      nsWeakPtr                        mElement;
    } mEntries[sMaxDeferred];
  };

  friend class mozilla::net::NeckoParent;
};

#endif 
