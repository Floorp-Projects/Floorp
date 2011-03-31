/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is worker threads.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
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

#ifndef __NSDOMWORKERXHRPROXIEDFUNCTIONS_H__
#define __NSDOMWORKERXHRPROXIEDFUNCTIONS_H__

#define MAKE_PROXIED_FUNCTION0(_name) \
  class _name : public SyncEventCapturingRunnable \
  { \
  public: \
    virtual nsresult RunInternal() \
    { \
      nsCOMPtr<nsIXMLHttpRequest> xhr = mXHR->GetXMLHttpRequest(); \
      if (xhr) { \
        return xhr-> _name (); \
      } \
      return NS_OK; \
    } \
  }

#define MAKE_PROXIED_FUNCTION1(_name, _arg1) \
  class _name : public SyncEventCapturingRunnable \
  { \
  public: \
    _name (_arg1 aArg1) : mArg1(aArg1) { } \
  \
    virtual nsresult RunInternal() \
    { \
      nsCOMPtr<nsIXMLHttpRequest> xhr = mXHR->GetXMLHttpRequest(); \
      if (xhr) { \
        return xhr-> _name (mArg1); \
      } \
      return NS_OK; \
    } \
  private: \
     _arg1 mArg1; \
  }

#define MAKE_PROXIED_FUNCTION2(_name, _arg1, _arg2) \
  class _name : public SyncEventCapturingRunnable \
  { \
  public: \
    _name (_arg1 aArg1, _arg2 aArg2) : mArg1(aArg1), mArg2(aArg2) { } \
  \
    virtual nsresult RunInternal() \
    { \
      nsCOMPtr<nsIXMLHttpRequest> xhr = mXHR->GetXMLHttpRequest(); \
      if (xhr) { \
        return xhr-> _name (mArg1, mArg2); \
      } \
      return NS_OK; \
    } \
  private: \
    _arg1 mArg1; \
    _arg2 mArg2; \
  }

namespace nsDOMWorkerProxiedXHRFunctions
{
  typedef nsDOMWorkerXHRProxy::SyncEventQueue SyncEventQueue;

  class SyncEventCapturingRunnable : public nsRunnable
  {
  public:
    SyncEventCapturingRunnable()
    : mXHR(nsnull), mQueue(nsnull) { }

    void Init(nsDOMWorkerXHRProxy* aXHR,
              SyncEventQueue* aQueue) {
      NS_ASSERTION(aXHR, "Null pointer!");
      NS_ASSERTION(aQueue, "Null pointer!");

      mXHR = aXHR;
      mQueue = aQueue;
    }

    virtual nsresult RunInternal() = 0;

    NS_IMETHOD Run() {
      NS_ASSERTION(mXHR && mQueue, "Forgot to call Init!");

      SyncEventQueue* oldQueue = mXHR->SetSyncEventQueue(mQueue);

      nsresult rv = RunInternal();

      mXHR->SetSyncEventQueue(oldQueue);

      return rv;
    }

  protected:
    nsRefPtr<nsDOMWorkerXHRProxy> mXHR;
    SyncEventQueue* mQueue;
  };

  class Abort : public SyncEventCapturingRunnable
  {
  public:
    virtual nsresult RunInternal() {
      return mXHR->Abort();
    }
  };

  class Open : public SyncEventCapturingRunnable
  {
  public:
    Open(const nsACString& aMethod, const nsACString& aUrl,
         PRBool aAsync, const nsAString& aUser,
         const nsAString& aPassword)
    : mMethod(aMethod), mUrl(aUrl), mAsync(aAsync), mUser(aUser),
      mPassword(aPassword) { }
  
    virtual nsresult RunInternal() {
      return mXHR->Open(mMethod, mUrl, mAsync, mUser, mPassword);
    }

  private:
    nsCString mMethod;
    nsCString mUrl;
    PRBool mAsync;
    nsString mUser;
    nsString mPassword;
  };

  MAKE_PROXIED_FUNCTION1(GetAllResponseHeaders, char**);

  MAKE_PROXIED_FUNCTION2(GetResponseHeader, const nsACString&, nsACString&);

  MAKE_PROXIED_FUNCTION1(Send, nsIVariant*);

  MAKE_PROXIED_FUNCTION1(SendAsBinary, const nsAString&);

  MAKE_PROXIED_FUNCTION2(SetRequestHeader, const nsACString&,
                         const nsACString&);

  MAKE_PROXIED_FUNCTION1(OverrideMimeType, const nsACString&);

  MAKE_PROXIED_FUNCTION1(SetMultipart, PRBool);

  MAKE_PROXIED_FUNCTION1(GetMultipart, PRBool*);

  MAKE_PROXIED_FUNCTION1(GetWithCredentials, PRBool*);

  MAKE_PROXIED_FUNCTION1(SetWithCredentials, PRBool);
}

#endif /* __NSDOMWORKERXHRPROXIEDFUNCTIONS_H__ */
