/* 
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
 * The Original Code is the mozilla.org LDAP XPCOM component.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifndef nsLDAPChannel_h__
#define nsLDAPChannel_h__

#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsILDAPMessageListener.h"

// if the code related to the following #define ever gets removed, also
// be sure to remove mCallback as well as the most (but not all) of the 
// various mUnproxied stuff
//
#define INVOKE_LDAP_CALLBACKS_ON_MAIN_THREAD 0

/* Header file */
class nsLDAPChannel : public nsIChannel, public nsILDAPMessageListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSILDAPMESSAGELISTENER

  nsLDAPChannel();
  virtual ~nsLDAPChannel();

  nsresult Init(nsIURI *uri);

  // this actually only gets called by nsLDAPHandler::NewChannel()
  //
  static NS_METHOD
  Create(nsISupports* aOuter, REFNSIID aIID, void **aResult);

protected:

  // these are internal functions, called by the dispatcher function
  // OnLDAPMessage()
  //
  nsresult OnLDAPSearchResult(nsILDAPMessage *aMessage);
  nsresult OnLDAPSearchEntry(nsILDAPMessage *aMessage);
  nsresult OnLDAPBind(nsILDAPMessage *aMessage);

  // write down the pipe to whoever is consuming our data
  //
  nsresult pipeWrite(char *str);
  
  // instance vars for read/write nsIChannel attributes
  //
  nsresult mStatus;
  nsCOMPtr<nsIURI> mURI; // the URI we're processing
  nsCOMPtr<nsILoadGroup> mUnproxiedLoadGroup; // the load group we belong to
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks; 
  nsCOMPtr<nsIURI> mOriginalURI; // the URI we started prcessing
  nsLoadFlags mLoadAttributes; // load attributes for this channel
  nsCOMPtr<nsISupports> mOwner; // entity responsible for this channel

  // various other instance vars
  //
  nsCOMPtr<nsIStreamListener> mUnproxiedListener; // for calls on main thread
  nsCOMPtr<nsILoadGroup> mLoadGroup; // possibly an nsISupports proxy
  nsCOMPtr<nsILDAPConnection> mConnection; // LDAP connection for this channel
  nsCOMPtr<nsIStreamListener> mListener; // for calls on LDAP callback thread
                                         // which _might_ be the main thread
  nsCOMPtr<nsISupports> mResponseContext; 
  nsCOMPtr<nsIInputStream> mReadPipeIn; // this end given to the listener
  nsCOMPtr<nsIOutputStream> mReadPipeOut; // for writes from the channel
  nsCOMPtr<nsILDAPOperation> mCurrentOperation; // current ldap operation
  PRUint32 mReadPipeOffset; // how many bytes written so far?
  PRBool mReadPipeClosed; // has the pipe already been closed?
  nsCOMPtr<nsILDAPMessageListener> mCallback; // callback

};

#endif // nsLDAPChannel_h__
