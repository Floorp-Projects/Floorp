/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Seth Spitzer <sspitzer@netscape.com>
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

#ifndef nsSubscribableServer_h__
#define nsSubscribableServer_h__

#include "nsCOMPtr.h"
#include "nsISubscribableServer.h"
#include "nsIMsgIncomingServer.h"
#include "nsIRDFService.h"
#include "nsSubscribeDataSource.h"
#include "nsIRDFResource.h"
#include "nsString.h"

typedef struct _subscribeTreeNode {
  char *name;
  PRBool isSubscribed;
  struct _subscribeTreeNode *prevSibling;
  struct _subscribeTreeNode *nextSibling;
  struct _subscribeTreeNode *firstChild;
  struct _subscribeTreeNode *lastChild;
  struct _subscribeTreeNode *parent;
  struct _subscribeTreeNode *cachedChild;
#ifdef HAVE_SUBSCRIBE_DESCRIPTION
  PRUnichar *description;
#endif
#ifdef HAVE_SUBSCRIBE_MESSAGES
  PRUint32 messages;
#endif
  PRBool isSubscribable;
} SubscribeTreeNode;

#if defined(DEBUG_sspitzer) || defined(DEBUG_seth)
#define DEBUG_SUBSCRIBE 1
#endif

class nsSubscribableServer : public nsISubscribableServer
{
 public:
  nsSubscribableServer();
  virtual ~nsSubscribableServer();

  nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISUBSCRIBABLESERVER
  
private:
  nsresult ConvertNameToUnichar(const char *inStr, PRUnichar **outStr);
  nsCOMPtr <nsISubscribeListener> mSubscribeListener;
  nsCOMPtr <nsIMsgIncomingServer> mIncomingServer;
  nsCOMPtr <nsISubscribeDataSource> mSubscribeDS;
  char mDelimiter;
  PRBool mShowFullName;
  PRBool mStopped;

  nsCOMPtr <nsIRDFResource>      kNC_Child;
  nsCOMPtr <nsIRDFResource>      kNC_Subscribed;
  nsCOMPtr <nsIRDFLiteral>       kTrueLiteral;
  nsCOMPtr <nsIRDFLiteral>       kFalseLiteral;

  nsCOMPtr <nsIRDFService>       mRDFService;

  SubscribeTreeNode *mTreeRoot;
  nsresult FreeSubtree(SubscribeTreeNode *node);
  nsresult CreateNode(SubscribeTreeNode *parent, const char *name, SubscribeTreeNode **result);
  nsresult AddChildNode(SubscribeTreeNode *parent, const char *name, SubscribeTreeNode **child);
  nsresult FindAndCreateNode(const char *path, SubscribeTreeNode **result);
  nsresult NotifyAssert(SubscribeTreeNode *subjectNode, nsIRDFResource *property, SubscribeTreeNode *objectNode);
  nsresult NotifyChange(SubscribeTreeNode *subjectNode, nsIRDFResource *property, PRBool value);
  nsresult Notify(nsIRDFResource *subject, nsIRDFResource *property, nsIRDFNode *object, PRBool isAssert, PRBool isChange);
  void BuildURIFromNode(SubscribeTreeNode *node, nsCAutoString &uri);
  nsresult EnsureSubscribeDS();
  nsresult EnsureRDFService();
};

#endif // nsSubscribableServer_h__
