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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
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

#ifndef __nsCocoaBrowserListener_h__
#define __nsCocoaBrowserListener_h__

#include "nsWeakReference.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebProgressListener2.h"
#include "nsIEmbeddingSiteWindow2.h"
#include "nsIWindowCreator.h"
#include "nsIWindowProvider.h"
#include "nsIDOMEventListener.h"

#include "nsIContextMenuListener.h"
#include "nsITooltipListener.h"

@class CHBrowserView;

class CHBrowserListener : public nsSupportsWeakReference,
                               public nsIInterfaceRequestor,
                               public nsIWebBrowserChrome,
                               public nsIWindowCreator,
                               public nsIWindowProvider,
                               public nsIEmbeddingSiteWindow2,
                               public nsIWebProgressListener2,
                               public nsIContextMenuListener,
                               public nsIDOMEventListener,
                               public nsITooltipListener
{
public:
  CHBrowserListener(CHBrowserView* aView);
  virtual ~CHBrowserListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIWEBBROWSERCHROME
  NS_DECL_NSIWINDOWCREATOR
  NS_DECL_NSIWINDOWPROVIDER
  NS_DECL_NSIEMBEDDINGSITEWINDOW
  NS_DECL_NSIEMBEDDINGSITEWINDOW2
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIWEBPROGRESSLISTENER2
  NS_DECL_NSICONTEXTMENULISTENER
  NS_DECL_NSITOOLTIPLISTENER
  NS_DECL_NSIDOMEVENTLISTENER
    
  void AddListener(id <CHBrowserListener> aListener);
  void RemoveListener(id <CHBrowserListener> aListener);

  void SetContainer(NSView<CHBrowserListener, CHBrowserContainer>* aContainer);

protected:

  nsresult HandleBlockedPopupEvent(nsIDOMEvent* inEvent);
  nsresult HandleLinkAddedEvent(nsIDOMEvent* inEvent);
  
private:
  CHBrowserView*          mView;     // WEAK - it owns us
  NSMutableArray*         mListeners;
  NSView<CHBrowserListener, CHBrowserContainer>* mContainer;
  PRBool                  mIsModal;
  PRUint32                mChromeFlags;
};


#endif // __nsCocoaBrowserListener_h__
