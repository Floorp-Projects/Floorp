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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include "inLayoutUtils.h"

#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMNodeList.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIContentViewer.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsPresContext.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"

///////////////////////////////////////////////////////////////////////////////

nsIDOMWindowInternal*
inLayoutUtils::GetWindowFor(nsIDOMNode* aNode)
{
  nsCOMPtr<nsIDOMDocument> doc1;
  aNode->GetOwnerDocument(getter_AddRefs(doc1));
  return GetWindowFor(doc1.get());
}

nsIDOMWindowInternal*
inLayoutUtils::GetWindowFor(nsIDOMDocument* aDoc)
{
  nsCOMPtr<nsIDOMDocumentView> doc = do_QueryInterface(aDoc);
  if (!doc) return nsnull;
  
  nsCOMPtr<nsIDOMAbstractView> view;
  doc->GetDefaultView(getter_AddRefs(view));
  if (!view) return nsnull;
  
  nsCOMPtr<nsIDOMWindowInternal> window = do_QueryInterface(view);
  return window;
}

nsIPresShell* 
inLayoutUtils::GetPresShellFor(nsISupports* aThing)
{
  nsCOMPtr<nsIScriptGlobalObject> so = do_QueryInterface(aThing);

  nsCOMPtr<nsIPresShell> presShell;
  so->GetDocShell()->GetPresShell(getter_AddRefs(presShell));

  return presShell;
}

/*static*/
nsIFrame*
inLayoutUtils::GetFrameFor(nsIDOMElement* aElement, nsIPresShell* aShell)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  nsIFrame* frame = nsnull;
  aShell->GetPrimaryFrameFor(content, &frame);

  return frame;
}

already_AddRefed<nsIRenderingContext>
inLayoutUtils::GetRenderingContextFor(nsIPresShell* aShell)
{
  nsCOMPtr<nsIWidget> widget;
  aShell->GetViewManager()->GetWidget(getter_AddRefs(widget));
  return widget->GetRenderingContext(); // AddRefs
}

nsIEventStateManager*
inLayoutUtils::GetEventStateManagerFor(nsIDOMElement *aElement)
{
  NS_PRECONDITION(aElement, "Passing in a null element is bad");

  nsCOMPtr<nsIDOMDocument> domDoc;
  aElement->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);

  if (!doc) {
    NS_WARNING("Could not get an nsIDocument!");
    return nsnull;
  }

  nsIPresShell *shell = doc->GetShellAt(0);
  NS_ASSERTION(shell, "No pres shell");

  nsCOMPtr<nsPresContext> presContext;
  shell->GetPresContext(getter_AddRefs(presContext));
  NS_ASSERTION(presContext, "No pres context");

  return presContext->EventStateManager();
}

nsPoint
inLayoutUtils::GetClientOrigin(nsPresContext* aPresContext,
                               nsIFrame* aFrame)
{
  nsPoint result(0,0);
  nsIView* view;
  aFrame->GetOffsetFromView(aPresContext, result, &view);
  nsIView* rootView = nsnull;
  if (view) {
      nsIViewManager* viewManager = view->GetViewManager();
      NS_ASSERTION(viewManager, "View must have a viewmanager");
      viewManager->GetRootView(rootView);
  }
  while (view) {
    result += view->GetPosition();
    if (view == rootView) {
      break;
    }
    view = view->GetParent();
  }
  return result;
}

nsRect& 
inLayoutUtils::GetScreenOrigin(nsIDOMElement* aElement)
{
  nsRect* rect = new nsRect(0,0,0,0);
 
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  nsCOMPtr<nsIDocument> doc = content->GetDocument();

  if (doc) {
    // Get Presentation shell 0
    nsIPresShell *presShell = doc->GetShellAt(0);

    if (presShell) {
      // Flush all pending notifications so that our frames are uptodate
      doc->FlushPendingNotifications(Flush_Layout);

      nsCOMPtr<nsPresContext> presContext;
      presShell->GetPresContext(getter_AddRefs(presContext));
      
      if (presContext) {
        nsIFrame* frame = nsnull;
        presShell->GetPrimaryFrameFor(content, &frame);
        
        PRInt32 offsetX = 0;
        PRInt32 offsetY = 0;
        nsIWidget* widget = nsnull;
        
        while (frame) {
          // Look for a widget so we can get screen coordinates
          nsIView* view = frame->GetViewExternal();
          if (view) {
            widget = view->GetWidget();
            if (widget)
              break;
          }
          
          // No widget yet, so count up the coordinates of the frame 
          nsPoint origin = frame->GetPosition();
          offsetX += origin.x;
          offsetY += origin.y;
      
          frame = frame->GetParent();
        }
        
        if (widget) {
          // Get the widget's screen coordinates
          nsRect oldBox(0,0,0,0);
          widget->WidgetToScreen(oldBox, *rect);

          // Get the scale from that Presentation Context
          float p2t;
          p2t = presContext->PixelsToTwips();

          // Convert screen rect to twips
          rect->x = NSIntPixelsToTwips(rect->x, p2t);
          rect->y = NSIntPixelsToTwips(rect->y, p2t);

          //  Add the offset we've counted
          rect->x += offsetX;
          rect->y += offsetY;
        }
      }
    }
  }
  
  return *rect;
}

nsIBindingManager* 
inLayoutUtils::GetBindingManagerFor(nsIDOMNode* aNode)
{
  nsCOMPtr<nsIDOMDocument> domdoc;
  aNode->GetOwnerDocument(getter_AddRefs(domdoc));
  if (domdoc) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
    return doc->GetBindingManager();
  }
  
  return nsnull;
}

nsIDOMDocument*
inLayoutUtils::GetSubDocumentFor(nsIDOMNode* aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (content) {
    nsCOMPtr<nsIDocument> doc = content->GetDocument();
    if (doc) {
      nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(doc->GetSubDocumentFor(content)));

      return domdoc;
    }
  }
  
  return nsnull;
}

nsIDOMNode*
inLayoutUtils::GetContainerFor(nsIDOMDocument* aDoc)
{
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDoc));

  nsCOMPtr<nsIDOMWindowInternal> win = inLayoutUtils::GetWindowFor(aDoc);
  nsCOMPtr<nsIDOMElement> elem;
  win->GetFrameElement(getter_AddRefs(elem));

  return elem;
}

