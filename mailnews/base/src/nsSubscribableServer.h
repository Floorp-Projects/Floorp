/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Seth Spitzer <sspitzer@netscape.com>
 */

#ifndef nsSubscribableServer_h__
#define nsSubscribableServer_h__

#include "nsCOMPtr.h"
#include "nsISubscribableServer.h"
#include "nsIMsgIncomingServer.h"
#include "nsIRDFService.h"
#include "nsSubscribeDataSource.h"
#include "nsIRDFResource.h"

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

  nsCOMPtr <nsISubscribeDumpListener> mDumpListener;

  SubscribeTreeNode *mTreeRoot;
  nsresult FreeSubtree(SubscribeTreeNode *node);
  nsresult CreateNode(SubscribeTreeNode *parent, const char *name, SubscribeTreeNode **result);
  nsresult AddChildNode(SubscribeTreeNode *parent, const char *name, SubscribeTreeNode **child);
  nsresult FindAndCreateNode(const char *path, SubscribeTreeNode **result);
  nsresult NotifyAssert(SubscribeTreeNode *subjectNode, nsIRDFResource *property, SubscribeTreeNode *objectNode);
  nsresult NotifyChange(SubscribeTreeNode *subjectNode, nsIRDFResource *property, PRBool value);
  nsresult Notify(nsIRDFResource *subject, nsIRDFResource *property, nsIRDFNode *object, PRBool isAssert, PRBool isChange);
  void BuildURIFromNode(SubscribeTreeNode *node, nsCAutoString &uri);
  void BuildPathFromNode(SubscribeTreeNode *node, nsCAutoString &uri);
  nsresult EnsureSubscribeDS();
  nsresult EnsureRDFService();
  nsresult DumpSubtree(struct SubscribeTreeNode *);
};

#endif // nsSubscribableServer_h__
