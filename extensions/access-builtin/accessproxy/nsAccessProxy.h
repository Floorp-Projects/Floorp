/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Aaron Leventhal.
 * Portions created by Aaron Leventhal are Copyright (C) 2001
 * Aaron Leventhal. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU General Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.  If you
 * wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under
 * the MPL, indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by the
 * GPL. If you do not delete the provisions above, a recipient may use
 * your version of this file under either the MPL or the GPL.
 *
 * Contributor(s):
 */

/**
 * This file is the header of an implementation
 * nsAccessProxy of the nsIAccessProxy interface.
 */

#include "nsIAccessProxy.h"
#include "nsIDOMEventListener.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"
#include "nsIObserver.h"
#include "nsIAppStartupNotifier.h"


/**
 * AccessProxy is an implementation of the nsIAccessProxy interface.  In XPCOM,
 * there can be more than one implementation of an given interface.  Class
 * IDs (CIDs) uniquely identify a particular implementation of an interface.
 * Interface IDs (IIDs) uniquely identify an interface.
 *
 * The CID is also a unique number that looks just like an IID
 * and uniquely identifies an implementation
 * {7CB5B7A0-07D7-11d3-BDE2-000064657374}
 */


class nsAccessProxy : public nsIDOMEventListener,
                      public nsIObserver,
                      public nsIWebProgressListener,
                      public nsSupportsWeakReference
{
public:
  nsAccessProxy();
  virtual ~nsAccessProxy();

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_ACCESSPROXY_CID);

  NS_DECL_ISUPPORTS  // This macro expands into declaration of nsISupports interface
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIWEBPROGRESSLISTENER

  //NS_DECL_NSIACCESSPROXY
  NS_IMETHOD HandleEvent(nsIDOMEvent *event);  

  static nsAccessProxy *GetInstance();
  static void ReleaseInstance(void);

private:
  static nsAccessProxy *mInstance;
};
