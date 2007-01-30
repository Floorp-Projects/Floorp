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
 * The Original Code is Spatial Navigation
 *
 * The Initial Developer of the Original Code is 
 * Douglas F. Turner II  <dougt@meer.net>
 * Portions created by the Initial Developer are Copyright (C) 2004-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "memory.h"
#include "stdlib.h"

#include "nspr.h"

#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsString.h"

#include "nsISpatialNavigation.h"

#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsFrameTraversal.h"
#include "nsIArray.h"
#include "nsIBaseWindow.h"
#include "nsICategoryManager.h"
#include "nsIChromeEventHandler.h"
#include "nsIComponentManager.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventGroup.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLHtmlElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMNode.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIEventStateManager.h"
#include "nsIFile.h"
#include "nsIFocusController.h"
#include "nsIFormControl.h"
#include "nsIFrameFrame.h"
#include "nsIFrameLoader.h"
#include "nsIFrameTraversal.h"
#include "nsIGenericFactory.h"
#include "nsIHTMLDocument.h"
#include "nsIImageFrame.h"
#include "nsIImageMap.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILineIterator.h"
#include "nsILocalFile.h"
#include "nsINameSpaceManager.h"
#include "nsIObserver.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefService.h"
#include "nsIProperties.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsISound.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWindowWatcher.h"
#include "nsLayoutCID.h"
#include "nsPIDOMWindow.h"
#include "nsStyleContext.h"

class nsSpatialNavigationService;
class nsSpatialNavigation;

class nsSpatialNavigation : public nsISpatialNavigation, public nsIDOMKeyListener
{
public:
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSISPATIALNAVIGATION
  
  NS_DECL_NSIDOMEVENTLISTENER
  
  // ----- nsIDOMKeyListener ----------------------------
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);
  
  nsSpatialNavigation(nsSpatialNavigationService* aService);
  
private:
  ~nsSpatialNavigation();
  
  nsPresContext* getPresContext(nsIContent* aContent);
  
  PRInt64 spatialDistance(int direction, nsRect& a, nsRect& b);
  
  nsIDOMWindow* getContentWindow();
  
  void setFocusedContent(nsIContent* aContent);
  void getFocusedContent(int direction, nsIContent** aContent);
  
  nsresult handleMove(int direction);
  nsresult getContentInDirection(int direction, nsPresContext* presContext, nsRect& focusedRect, nsIFrame* focusedFrame, PRBool aFocusDocuments, PRBool isAREA, nsIContent** aContent);

  nsCOMPtr<nsIDOMWindow> mTopWindow;

  nsSpatialNavigationService* mService;

  PRBool mNavigationFramesState;
};



class nsSpatialNavigationService: public nsIObserver 
{
public:  
  nsSpatialNavigationService();  
  virtual ~nsSpatialNavigationService();  
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsCOMArray<nsISpatialNavigation> mObjects;

  PRBool  mEnabled;
  PRBool  mIgnoreTextFields;
  PRBool  mDisableJSWhenFocusing;

  PRInt32 mKeyCodeLeft;
  PRInt32 mKeyCodeRight;
  PRInt32 mKeyCodeUp;
  PRInt32 mKeyCodeDown;
  PRInt32 mKeyCodeModifier;

};

extern PRInt32 gRectFudge;
extern PRInt32 gDirectionalBias;

enum {
  eNavLeft = 0,
  eNavUp,
  eNavRight,
  eNavDown
};


// Utils

nscoord* lo_parse_coord_list          (char *str, PRInt32* value_cnt);
nsresult createFrameTraversal         (nsPresContext* aPresContext,
                                       PRInt32 aType,
                                       PRBool aVisual,
                                       PRBool aLockInScrollView,
                                       PRBool aFollowOOFs,
                                       nsIBidirectionalEnumerator** outTraversal);
nsresult getEventTargetFromWindow     (nsIDOMWindow* aWindow, nsIDOM3EventTarget** aEventTarget, nsIDOMEventGroup** aSystemGroup);
void     getContentFromFrame          (nsIContent* c, nsIContent** outContent);
nsresult getFrameForContent           (nsIContent* aContent, nsIFrame** aFrame);
PRBool   isContentOfType              (nsIContent* content, const char* type);
PRBool   isArea                       (nsIContent* content);
PRBool   isMap                        (nsIFrame* frame);
PRBool   isTargetable                 (PRBool focusDocuments, nsIFrame* frame);
nsRect   makeRectRelativeToGlobalView (nsIFrame *aFrame);
void     poly2Rect                    (int sides, nscoord* coord, nsRect* rect);
void     getRectOfAreaElement         (nsIFrame* f, nsIDOMHTMLAreaElement* e, nsRect* r);
PRBool   isRectInDirection            (int direction, nsRect& focusedRect, nsRect& frameRect);
PRInt64  spatialDistance              (int direction, nsRect& a, nsRect& b);
void     GetWindowFromDocument        (nsIDOMDocument* aDocument, nsIDOMWindowInternal** aWindow);
PRBool   IsPartiallyVisible           (nsIPresShell* shell, nsIFrame* frame);
void     ScrollWindow                 (int direction, nsIDOMWindow* window);



class DisableJSScope
{
public:
  DisableJSScope(nsIDOMWindow* window) :
    isEnabled(PR_FALSE), scriptContext(nsnull)
  {
    // My passing null, we don't do a thing
    if (!window)
      return;

    // so, we want to set the focus to the next element, but
    // we do not want onFocus to fire.  The reason for this is
    // that we want to have the "enter" key be able to trigger
    // the targeted link after we have focused it.  However,
    // we have found a popular portal has a toolbar that, on
    // onFocus, decides to move the focus as if the targeted
    // content was clicked.  The only way that I know how to
    // do this is to disable javascript during this call.

    nsCOMPtr<nsIScriptGlobalObject> sgo (do_QueryInterface(window));
    if (!sgo)
      return;
    
    scriptContext = sgo->GetContext();
    if (!scriptContext)
      return;

    isEnabled = scriptContext->GetScriptsEnabled();
    
    if (isEnabled)
      scriptContext->SetScriptsEnabled(PR_FALSE, PR_TRUE);
  }

  ~DisableJSScope()
  {
    if (isEnabled && scriptContext)
    {
      // enable javascript again..
      scriptContext->SetScriptsEnabled(PR_TRUE, PR_TRUE);
    }
      
  }
  PRBool isEnabled;
  nsIScriptContext *scriptContext;
};


