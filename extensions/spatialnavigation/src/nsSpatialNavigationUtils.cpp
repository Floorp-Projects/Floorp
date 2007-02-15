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

#include "nsSpatialNavigationPrivate.h"

#ifdef DEBUG_outputframes
#include "nsIFrameDebug.h"
#endif

static PRBool is_space(char c)
{
  return (c == ' '  || c == '\f' || c == '\n' ||
          c == '\r' || c == '\t' || c == '\v');
}

nscoord* lo_parse_coord_list(char *str, PRInt32* value_cnt)
{
  char *tptr;
  char *n_str;
  PRInt32 i, cnt;
  PRInt32 *value_list;
  
  /*
   * Nothing in an empty list
   */
  *value_cnt = 0;
  if (!str || *str == '\0')
  {
    return nsnull;
  }
  
  /*
   * Skip beginning whitespace, all whitespace is empty list.
   */
  n_str = str;
  while (is_space(*n_str))
  {
    n_str++;
  }
  if (*n_str == '\0')
  {
    return nsnull;
  }
  
  /*
   * Make a pass where any two numbers separated by just whitespace
   * are given a comma separator.  Count entries while passing.
   */
  cnt = 0;
  while (*n_str != '\0')
  {
    PRBool has_comma;
    
    /*
     * Skip to a separator
     */
    tptr = n_str;
    while (!is_space(*tptr) && *tptr != ',' && *tptr != '\0')
    {
      tptr++;
    }
    n_str = tptr;
    
    /*
     * If no more entries, break out here
     */
    if (*n_str == '\0')
    {
      break;
    }
    
    /*
     * Skip to the end of the separator, noting if we have a
     * comma.
     */
    has_comma = PR_FALSE;
    while (is_space(*tptr) || *tptr == ',')
    {
      if (*tptr == ',')
      {
        if (has_comma == PR_FALSE)
        {
          has_comma = PR_TRUE;
        }
        else
        {
          break;
        }
      }
      tptr++;
    }
    /*
     * If this was trailing whitespace we skipped, we are done.
     */
    if ((*tptr == '\0')&&(has_comma == PR_FALSE))
    {
      break;
    }
    /*
     * Else if the separator is all whitespace, and this is not the
     * end of the string, add a comma to the separator.
     */
    else if (has_comma == PR_FALSE)
    {
      *n_str = ',';
    }
    
    /*
     * count the entry skipped.
     */
    cnt++;
    
    n_str = tptr;
  }
  /*
   * count the last entry in the list.
   */
  cnt++;
  
  /*
   * Allocate space for the coordinate array.
   */
  value_list = new nscoord[cnt];
  if (!value_list)
  {
    return nsnull;
  }
  
  /*
   * Second pass to copy integer values into list.
   */
  tptr = str;
  for (i=0; i<cnt; i++)
  {
    char *ptr;
    
    ptr = strchr(tptr, ',');
    if (ptr)
    {
      *ptr = '\0';
    }
    /*
     * Strip whitespace in front of number because I don't
     * trust atoi to do it on all platforms.
     */
    while (is_space(*tptr))
    {
      tptr++;
    }
    if (*tptr == '\0')
    {
      value_list[i] = 0;
    }
    else
    {
      value_list[i] = (nscoord) ::atoi(tptr);
    }
    if (ptr)
    {
      *ptr = ',';
      tptr = ptr + 1;
    }
  }
  
  *value_cnt = cnt;
  return value_list;
}

nsresult createFrameTraversal(nsPresContext* aPresContext,
                              PRInt32 aType,
                              PRBool aVisual,
                              PRBool aLockInScrollView,
                              PRBool aFollowOOFs,
                              nsIBidirectionalEnumerator** outTraversal)
{
  nsresult result;
  if (!aPresContext)
    return NS_ERROR_FAILURE;    
  
  nsIPresShell* presShell = aPresContext->PresShell();
  if (!presShell)
    return NS_ERROR_FAILURE;
  
  nsIFrame* frame = presShell->GetRootFrame();
  
  if (!frame)
    return NS_ERROR_FAILURE;

#ifdef DEBUG_outputframes
  nsIFrameDebug* fd;
  frame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**) &fd);
  if (fd)
    fd->List(frame->GetPresContext(), stdout, 0);
#endif

  nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;
  
  static NS_DEFINE_CID(kFrameTraversalCID, NS_FRAMETRAVERSAL_CID);
  nsCOMPtr<nsIFrameTraversal> trav(do_CreateInstance(kFrameTraversalCID,&result));
  if (NS_FAILED(result))
    return result;
  
  result = trav->NewFrameTraversal(getter_AddRefs(frameTraversal),
                                   aPresContext, frame,
                                   aType, aVisual, aLockInScrollView, aFollowOOFs);
  if (NS_FAILED(result))
    return result;
  
  NS_IF_ADDREF(*outTraversal = frameTraversal);
  return NS_OK;
}

nsresult getEventTargetFromWindow(nsIDOMWindow* aWindow, nsIDOM3EventTarget** aEventTarget, nsIDOMEventGroup** aSystemGroup)
{
  *aEventTarget = nsnull;
  nsCOMPtr<nsPIDOMWindow> privateWindow = do_QueryInterface(aWindow);
  
  if (!privateWindow)
    return NS_ERROR_UNEXPECTED; // assert
  
  nsPIDOMEventTarget *chromeEventHandler = privateWindow->GetChromeEventHandler();
  
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(chromeEventHandler));
  if (!receiver)
    return NS_ERROR_UNEXPECTED; // assert
  
  nsCOMPtr<nsIDOMEventGroup> systemGroup;
  receiver->GetSystemEventGroup(getter_AddRefs(systemGroup));
  nsCOMPtr<nsIDOM3EventTarget> target(do_QueryInterface(receiver));
  
  if (!target || !systemGroup)
    return NS_ERROR_FAILURE;
  
  NS_ADDREF(*aEventTarget = target);
  NS_ADDREF(*aSystemGroup = systemGroup);
  return NS_OK;
}

void getContentFromFrame(nsIContent* c, nsIContent** outContent)
{
  *outContent = nsnull;

  nsCOMPtr<nsIContent> result;
  nsCOMPtr<nsIDOMDocument> contentDocument;

  nsCOMPtr<nsIDOMHTMLFrameElement> domFrameElement = do_QueryInterface(c);

  if (domFrameElement) {
    domFrameElement->GetContentDocument(getter_AddRefs(contentDocument));
  }  
  else {
    nsCOMPtr<nsIDOMHTMLIFrameElement> domIFrameElement = do_QueryInterface(c);
	if (domIFrameElement)
      domIFrameElement->GetContentDocument(getter_AddRefs(contentDocument));
  }

  if (contentDocument) {
    nsCOMPtr<nsIDOMElement> documentElement;
    contentDocument->GetDocumentElement(getter_AddRefs(documentElement));
    result = do_QueryInterface(documentElement);
  }
  NS_IF_ADDREF(*outContent = result);
}

nsresult getFrameForContent(nsIContent* aContent, nsIFrame** aFrame)
{
  *aFrame = nsnull;

  if (!aContent)
    return NS_ERROR_FAILURE;
  
  nsIDocument* doc = aContent->GetDocument();
  if (!doc)
    return NS_ERROR_FAILURE;
  
  nsIPresShell *presShell = doc->GetShellAt(0);
  nsIFrame* frame = presShell->GetPrimaryFrameFor(aContent);
  
  if (!frame)
    return NS_ERROR_FAILURE;
  
  *aFrame = frame;
  return NS_OK;
}

PRBool
isContentOfType(nsIContent* content, const char* type)
{
  if (!content)
	return PR_FALSE;

  if (content->IsNodeOfType(nsINode::eELEMENT))
  {
    nsIAtom* atom =  content->NodeInfo()->NameAtom();
    if (atom)
      return atom->EqualsUTF8(nsDependentCString(type));
  }
  return PR_FALSE;
}

PRBool
isArea(nsIContent* content)
{
  if (!content || !content->IsNodeOfType(nsINode::eHTML))
      return PR_FALSE;

  return isContentOfType(content, "area");
}

PRBool
isMap(nsIFrame* frame)
{
  nsIContent* content = frame->GetContent();

  if (!content || !content->IsNodeOfType(nsINode::eHTML))
      return PR_FALSE;

  return isContentOfType(content, "map");
}

PRBool 
isTargetable(PRBool focusDocuments, nsIFrame* frame)
{
  nsIContent* currentContent = frame->GetContent();

  if (!currentContent)
    return PR_FALSE;

  if (!currentContent->IsNodeOfType(nsINode::eHTML))
      return PR_FALSE;

  if (isContentOfType(currentContent, "map"))
    return PR_TRUE;

  if (isContentOfType(currentContent, "button"))
    return PR_TRUE;

  if (isContentOfType(currentContent, "a"))
  {
    // an anchor isn't targetable unless it has a non-null href.
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchorElement = do_QueryInterface(currentContent);
    nsAutoString uri;
    anchorElement->GetHref(uri);
    if (uri.IsEmpty()) {
      return PR_FALSE; 
    }
    return PR_TRUE;
  }

  nsCOMPtr<nsIFrameFrame> frameFrame(do_QueryInterface(frame));
  if (frameFrame) 
    return PR_TRUE;

  nsCOMPtr<nsIDOMHTMLIFrameElement> iFrameElement = do_QueryInterface(currentContent);
  if (iFrameElement) 
    return PR_TRUE;

  if (focusDocuments) {
    nsCOMPtr<nsIDOMHTMLHtmlElement> hhElement(do_QueryInterface(currentContent));
    if (hhElement) 
      return PR_TRUE;
  }

  // need to figure out how to determine if a element is
  // either disabled, hidden, or it is inaccessible due to
  // its parent being one of these.

  PRBool disabled = PR_FALSE;
  
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement = do_QueryInterface(currentContent);
  if (selectElement && NS_SUCCEEDED(selectElement->GetDisabled(&disabled)))
    return !disabled;

  nsCOMPtr<nsIDOMHTMLOptionElement> optionElement = do_QueryInterface(currentContent);
  if (optionElement && NS_SUCCEEDED(optionElement->GetDisabled(&disabled)))
    return !disabled;

  nsAutoString inputType;
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement = do_QueryInterface(currentContent);
  if (inputElement && NS_SUCCEEDED(inputElement->GetDisabled(&disabled)) && NS_SUCCEEDED(inputElement->GetType(inputType)))
    return !disabled && (! inputType.EqualsIgnoreCase("hidden"));

  return PR_FALSE;
}

nsRect makeRectRelativeToGlobalView(nsIFrame *aFrame)
{
  nsRect result;
  result.SetRect(0,0,0,0);

  nsPoint offset;

  if (!aFrame)
    return result;

  result = aFrame->GetRect();

  nsIView* view;
  aFrame->GetOffsetFromView(offset, &view);
  
  nsIView* rootView = nsnull;
  if (view) {
    nsIViewManager* viewManager = view->GetViewManager();
    NS_ASSERTION(viewManager, "View must have a viewmanager");
    viewManager->GetRootView(rootView);
  }
  while (view) {
    offset  += view->GetPosition();
    if (view == rootView) {
      break;
    }
    view = view->GetParent();
  }
  
  result.MoveTo(offset);
  return result;
}


void poly2Rect(int sides, nscoord* coord, nsRect* rect)
{
  nscoord x1, x2, y1, y2, xtmp, ytmp;
  x1 = x2 = coord[0];
  y1 = y2 = coord[1];
  for (PRInt32 i = 2; i < sides; i += 2) 
  {
    xtmp = coord[i];
    ytmp = coord[i+1];
    x1 = x1 < xtmp ? x1 : xtmp;
    y1 = y1 < ytmp ? y1 : ytmp;
    x2 = x2 > xtmp ? x2 : xtmp;
    y2 = y2 > ytmp ? y2 : ytmp;
  }
  rect->SetRect(x1, y1, x2, y2);
}

void getRectOfAreaElement(nsIFrame* f, nsIDOMHTMLAreaElement* e, nsRect* r)
{
  if (!f || !e || !r)
    return;

  // RECT !!
  nsRect frameRect = makeRectRelativeToGlobalView(f);
  
  if (frameRect.width == 0)
    frameRect.width = 1;
  if (frameRect.height == 0)
    frameRect.height = 1;
  
  nsPoint offset;
  offset.x = frameRect.x;
  offset.y = frameRect.y;
  

  nsAutoString coordstr;
  e->GetCoords(coordstr);

  NS_ConvertUTF16toUTF8 cp (coordstr);
  
  PRInt32 count;
  nscoord* coords = lo_parse_coord_list((char*) cp.get(), &count);
  
  // FIX what about other shapes?
  
  if (count == 4) 
  {
    //                x          y          width                  height
    frameRect.SetRect(coords[0], coords[1], coords[2] - coords[0], coords[3] - coords[1]);
    
    frameRect.x += offset.x;
    frameRect.y += offset.y;
  }
  else if (count >=6)
    poly2Rect(count, coords, &frameRect);
#ifdef DEBUG_dougt
  else
    printf("area count not supported!! %d\n", count);
#endif

  // find the center of the rect.

  frameRect.x = frameRect.x + (frameRect.width / 2) - 1;
  frameRect.width = 2;
  frameRect.y = frameRect.y + (frameRect.height / 2) - 1;
  frameRect.height = 2;

  *r = frameRect;
}

// we want to determine if going in the |direction|
// direction from focusedRect will intercept the frameRect

PRBool isRectInDirection(int direction, nsRect& focusedRect, nsRect& frameRect)
{
  if (direction == eNavLeft)
  {
    return (frameRect.x < focusedRect.x);
  }
  
  if (direction == eNavRight)
  {
    return (frameRect.x + frameRect.width > focusedRect.x + focusedRect.width);
  }
  
  if (direction == eNavUp)
  {
    return (frameRect.y < focusedRect.y);
  }
  
  if (direction == eNavDown)
  {
    return (frameRect.y + frameRect.height > focusedRect.y + focusedRect.height);
  }
  return PR_FALSE;
}

PRInt64 spatialDistance(int direction, nsRect& a, nsRect& b)
{
  PRBool inlineNavigation = PR_FALSE;
  nsPoint m, n;
  
  if (direction == eNavLeft)
  {
    //  |---|
    //  |---|
    //
    //  |---|  |---|
    //  |---|  |---|
    //
    //  |---|
    //  |---|
    //
    
    if (a.y > b.y + b.height) 
    {
      // the b rect is above a.
      m.x = a.x;
      m.y = a.y;
      n.x = b.x + b.width;
      n.y = b.y + b.height;
    }
    else if (a.y + a.height < b.y)
    {
      // the b rect is below a.
      m.x = a.x;
      m.y = a.y + a.height;
      n.x = b.x + b.width;
      n.y = b.y;       
    }
    else
    {
      m.x = a.x;
      m.y = 0;
      n.x = b.x + b.width; 
      n.y = 0;

      //      m.x = (a.x + (a.width / 2));
      //      m.y = (a.y + (a.height / 2));
      //      n.x = (b.x + (b.width / 2));
      //      n.y = (b.y + (b.height / 2));

    }
  }
  else if (direction == eNavRight)
  {
    
    //         |---|
    //         |---|
    //
    //  |---|  |---|
    //  |---|  |---|
    //
    //         |---|
    //         |---|
    //
    
    if (a.y > b.y + b.height) 
    {
      // the b rect is above a.
      m.x = a.x + a.width;
      m.y = a.y;
      n.x = b.x;
      n.y = b.y + b.height;
    }
    else if (a.y + a.height < b.y)
    {
      // the b rect is below a.
      m.x = a.x + a.width;
      m.y = a.y + a.height;
      n.x = b.x;
      n.y = b.y;       
    }
    else
    {
      m.x = a.x + a.width;
      m.y = 0;
      n.x = b.x; 
      n.y = 0;
      
      //      m.x = (a.x + (a.width / 2));
      //      m.y = (a.y + (a.height / 2));
      //      n.x = (b.x + (b.width / 2));
      //      n.y = (b.y + (b.height / 2));
    }
  }
  else if (direction == eNavUp)
  {
    
    //  |---|  |---|  |---|
    //  |---|  |---|  |---|
    //
    //         |---|
    //         |---|
    //
    
    if (a.x > b.x + b.width) 
    {
      // the b rect is to the left of a.
      m.x = a.x;
      m.y = a.y;
      n.x = b.x + b.width;
      n.y = b.y + b.height;
    }
    else if (a.x + a.width < b.x)
    {
      // the b rect is to the right of a
      m.x = a.x + a.width;
      m.y = a.y;
      n.x = b.x;
      n.y = b.y + b.height;       
    }
    else
    {
      // both b and a share some common x's.
      m.x = 0;
      m.y = a.y;
      n.x = 0;
      n.y = b.y + b.height;

      //      m.x = (a.x + (a.width / 2));
      //      m.y = (a.y + (a.height / 2));     
      //      n.x = (b.x + (b.width / 2));
      //      n.y = (b.y + (b.height / 2));
    }
  }
  else if (direction == eNavDown)
  {
    //         |---|
    //         |---|
    //
    //  |---|  |---|  |---|
    //  |---|  |---|  |---|
    //
    
    if (a.x > b.x + b.width) 
    {
      // the b rect is to the left of a.
      m.x = a.x;
      m.y = a.y + a.height;
      n.x = b.x + b.width;
      n.y = b.y;
    }
    else if (a.x + a.width < b.x)
    {
      // the b rect is to the right of a
      m.x = a.x + a.width;
      m.y = a.y + a.height;
      n.x = b.x;
      n.y = b.y;       
    }
    else
    {
      // both b and a share some common x's.
      m.x = 0;
      m.y = a.y + a.height;
      n.x = 0;
      n.y = b.y;

      //      m.x = (a.x + (a.width / 2));
      //      m.y = (a.y + (a.height / 2));     
      //      n.x = (b.x + (b.width / 2));
      //      n.y = (b.y + (b.height / 2));
    }
  }
  
  // a is always the currently focused rect.
  
  nsRect scopedRect = a;
  scopedRect.Inflate(gRectFudge, gRectFudge);
  
  if (direction == eNavLeft)
  {
    scopedRect.x = 0;
    scopedRect.width = nscoord_MAX;
    if (scopedRect.Intersects(b))
      inlineNavigation = PR_TRUE;
  }
  else if (direction == eNavRight)
  {
    scopedRect.width = nscoord_MAX;
    if (scopedRect.Intersects(b))
      inlineNavigation = PR_TRUE;
  }
  else if (direction == eNavUp)
  {
    scopedRect.y = 0;
    scopedRect.height = nscoord_MAX;
    if (scopedRect.Intersects(b))
      inlineNavigation = PR_TRUE;
  }
  else if (direction == eNavDown)
  {
    scopedRect.height = nscoord_MAX;
    if (scopedRect.Intersects(b))
      inlineNavigation = PR_TRUE;
  }
  
  PRInt64 d = ((m.x-n.x)*(m.x-n.x)) + ((m.y-n.y)*(m.y-n.y));
  
  if(d<0)
    d=d*(-1);
  
  if (inlineNavigation)
	d /= gDirectionalBias;
  
  return d;
}

void GetWindowFromDocument(nsIDOMDocument* aDocument, nsIDOMWindowInternal** aWindow)
{
  nsCOMPtr<nsIDOMDocumentView> docview = do_QueryInterface(aDocument);

  nsCOMPtr<nsIDOMAbstractView> view;
  docview->GetDefaultView(getter_AddRefs(view));
  if (!view) return;
  
  nsCOMPtr<nsIDOMWindowInternal> window = do_QueryInterface(view);
  NS_IF_ADDREF(*aWindow = window);
}



PRBool IsPartiallyVisible(nsIPresShell* shell, nsIFrame* frame) 
{
   // We need to know if at least a kMinPixels around the object is visible
   // Otherwise it will be marked STATE_OFFSCREEN and STATE_INVISIBLE
   
   const PRUint16 kMinPixels  = 12;
    // Set up the variables we need, return false if we can't get at them all
 
   nsIViewManager* viewManager = shell->GetViewManager();
   if (!viewManager)
     return PR_FALSE;
 
 
   // If visibility:hidden or visibility:collapsed then mark with STATE_INVISIBLE
   if (!frame->GetStyleVisibility()->IsVisible())
   {
       return PR_FALSE;
   }
 
   nsPresContext *presContext = shell->GetPresContext();
   if (!presContext)
     return PR_FALSE;
 
   // Get the bounds of the current frame, relative to the current view.
   // We don't use the more accurate GetBoundsRect, because that is more expensive 
   // and the STATE_OFFSCREEN flag that this is used for only needs to be a rough indicator
 
   nsRect relFrameRect = frame->GetRect();
   nsPoint frameOffset;
   nsIView *containingView = frame->GetViewExternal();
   if (!containingView) {
     frame->GetOffsetFromView(frameOffset, &containingView);
     if (!containingView)
       return PR_FALSE;  // no view -- not visible
     relFrameRect.x = frameOffset.x;
     relFrameRect.y = frameOffset.y;
   }
 
   float p2t;
   p2t = presContext->PixelsToTwips();
   nsRectVisibility rectVisibility;
   viewManager->GetRectVisibility(containingView, relFrameRect, 
                                  NS_STATIC_CAST(PRUint16, (kMinPixels * p2t)), 
                                  &rectVisibility);
 
   if (rectVisibility == nsRectVisibility_kVisible ||
       (rectVisibility == nsRectVisibility_kZeroAreaRect && frame->GetNextInFlow())) {
     // This view says it is visible, but we need to check the parent view chain :(
     // Note: zero area rects can occur in the first frame of a multi-frame text flow,
     //       in which case the next frame exists because the text flow is visible
     while ((containingView = containingView->GetParent()) != nsnull) {
       if (containingView->GetVisibility() == nsViewVisibility_kHide) {
         return PR_FALSE;
       }
     }
     return PR_TRUE;
   }
 
   return PR_FALSE;
 }


const static PRInt32 gScrollOffset = (26*3);


void ScrollWindow(int direction, nsIDOMWindow* contentWindow)
{
  if (!contentWindow)
    return;
  
  if (direction == eNavLeft)
    contentWindow->ScrollBy(-1* gScrollOffset, 0);
  else if (direction == eNavRight)
    contentWindow->ScrollBy(gScrollOffset, 0);
  else if (direction == eNavUp)
    contentWindow->ScrollBy(0, -1 * gScrollOffset);
  else
    contentWindow->ScrollBy(0, gScrollOffset);
}
