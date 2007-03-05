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

PRInt32 gRectFudge = 20;
PRInt32 gDirectionalBias = 1;

NS_INTERFACE_MAP_BEGIN(nsSpatialNavigation)
  NS_INTERFACE_MAP_ENTRY(nsISpatialNavigation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMKeyListener)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsSpatialNavigation)
NS_IMPL_RELEASE(nsSpatialNavigation)


nsSpatialNavigation::nsSpatialNavigation(nsSpatialNavigationService* aService)
{
  NS_ASSERTION(aService, "Should not create this object without a valid service");

  mService = aService; // back pointer -- no reference

  mNavigationFramesState = PR_FALSE;
}

nsSpatialNavigation::~nsSpatialNavigation()
{
}

NS_IMETHODIMP
nsSpatialNavigation::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSpatialNavigation::KeyUp(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSpatialNavigation::KeyPress(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSpatialNavigation::KeyDown(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRBool enabled;
  prefBranch->GetBoolPref("snav.enabled", &enabled);
  if (!enabled) //  this doesn't work.  wtf? if (!mService->mEnabled)
    return NS_OK;
 

  nsCOMPtr<nsIDOMNSUIEvent> uiEvent(do_QueryInterface(aEvent));
  if (uiEvent)
  {
    // If a web page wants to use the keys mapped to our
    // move, they have to use evt.preventDefault() after
    // they get the key

    PRBool preventDefault;
    uiEvent->GetPreventDefault(&preventDefault);
    if (preventDefault)
      return NS_OK;
  }

  PRInt32 formControlType = -1;
  // check to see if we are in a text field.
  // based on nsTypeAheadFind.
    
  //nsEvent should be renamed.
  nsCOMPtr<nsIDOMNSEvent> nsEvent = do_QueryInterface(aEvent);
  if (!nsEvent)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMEventTarget> domEventTarget;
  nsEvent->GetOriginalTarget(getter_AddRefs(domEventTarget));
  
  nsCOMPtr<nsIContent> targetContent = do_QueryInterface(domEventTarget);

  if (targetContent->IsNodeOfType(nsINode::eXUL)) 
    return NS_OK;
  
  if (targetContent->IsNodeOfType(nsINode::eHTML_FORM_CONTROL)) 
  {
      nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(targetContent));
      formControlType = formControl->GetType();
      
      if (mService->mIgnoreTextFields)
      {
        if (formControlType == NS_FORM_TEXTAREA ||
            formControlType == NS_FORM_INPUT_TEXT ||
            formControlType == NS_FORM_INPUT_PASSWORD ||
            formControlType == NS_FORM_INPUT_FILE) 
        {
          return NS_OK;
        }
      }
  }
  else if (mService->mIgnoreTextFields && targetContent->IsNodeOfType(nsINode::eHTML)) 
  {
    // Test for isindex, a deprecated kind of text field. We're using a string 
    // compare because <isindex> is not considered a form control, so it does 
    // not support nsIFormControl or eHTML_FORM_CONTROL, and it's not worth 
    // having a table of atoms just for it. 
    
      if (isContentOfType(targetContent, "isindex"))
        return NS_OK;
  }

  PRUint32 keyCode;
  PRBool isModifier;
  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aEvent));
  
  if (!keyEvent)
    return NS_ERROR_FAILURE;

  if (NS_FAILED(keyEvent->GetKeyCode(&keyCode)))
	return NS_ERROR_FAILURE;
  
  // figure out what modifier to use  
  
  /************************************************
    Value of the keyCodeModifier is
  
    SHIFT          = 0x00100000
    CONTROL        = 0x00001100
    ALT            = 0x00000012
  *************************************************/
  
  
  if (mService->mKeyCodeModifier & 0x00100000)
  {
    if (NS_FAILED(keyEvent->GetShiftKey(&isModifier)))
      return NS_ERROR_FAILURE;
    if (!isModifier)
      return NS_OK;  
  }
  
  if (mService->mKeyCodeModifier & 0x00001100)
  {
    if (NS_FAILED(keyEvent->GetCtrlKey(&isModifier)))
      return NS_ERROR_FAILURE;
    if (!isModifier)
      return NS_OK;  
  }
  
  if (mService->mKeyCodeModifier & 0x00000012)
  {
    if (NS_FAILED(keyEvent->GetAltKey(&isModifier)))
      return NS_ERROR_FAILURE;
    if (!isModifier)
      return NS_OK;  
  }
  
  
  if (keyCode == mService->mKeyCodeLeft)
  {
  
   // ************************************************************************************
    // NS_FORM_TEXTAREA cases:

    // ************************************************************************************
    // NS_FORM_INPUT_TEXT | NS_FORM_INPUT_PASSWORD | NS_FORM_INPUT_FILE cases


    if (formControlType == NS_FORM_INPUT_TEXT || 
        formControlType == NS_FORM_INPUT_PASSWORD)
    {
      PRInt32 selectionStart, textLength;
      nsCOMPtr<nsIDOMNSHTMLInputElement> input = do_QueryInterface(targetContent);
      if (input) {
        input->GetSelectionStart (&selectionStart);
        input->GetTextLength (&textLength);
      } else {
        nsCOMPtr<nsIDOMNSHTMLTextAreaElement> textArea = do_QueryInterface(targetContent);
        if (textArea) {
          textArea->GetSelectionStart (&selectionStart);
          textArea->GetTextLength (&textLength);
        }
      }
	  
      if (textLength != 0 && selectionStart != 0)
        return NS_OK;
    }

    // We're using this key, no one else should
    aEvent->StopPropagation();
	aEvent->PreventDefault();
	
    return Left();
  }
  
  if (keyCode == mService->mKeyCodeRight)
  {
    // ************************************************************************************
    // NS_FORM_TEXTAREA cases:

    // ************************************************************************************
    // NS_FORM_INPUT_TEXT | NS_FORM_INPUT_PASSWORD | NS_FORM_INPUT_FILE cases

    if (formControlType == NS_FORM_INPUT_TEXT || 
        formControlType == NS_FORM_INPUT_PASSWORD)
    {
      PRInt32 selectionEnd, textLength;
      nsCOMPtr<nsIDOMNSHTMLInputElement> input = do_QueryInterface(targetContent);
      if (input) {
        input->GetSelectionEnd (&selectionEnd);
        input->GetTextLength (&textLength);
      } else {
        nsCOMPtr<nsIDOMNSHTMLTextAreaElement> textArea = do_QueryInterface(targetContent);
        if (textArea) {
          textArea->GetSelectionEnd (&selectionEnd);
          textArea->GetTextLength (&textLength);
        }
      }
      
      // going down.

      if (textLength  != selectionEnd)
        return NS_OK;
    }
	
    aEvent->StopPropagation();
	aEvent->PreventDefault();
    return Right();
  }
  
  if (keyCode == mService->mKeyCodeUp)
  {

    // If we are going up or down, in a select, lets not
    // navigate.
    //
    // FIX: What we really want to do is determine if we are
    // at the start or the end fo the form element, and
    // based on the selected position we decide to nav. or
    // not.

    // ************************************************************************************
    // NS_FORM_SELECT cases:
    // * if it is a select form of 'size' attr != than '1' then we do as above.

    // * if it is a select form of 'size' attr == than '1', snav can take care of it.
    // if (formControlType == NS_FORM_SELECT)
    //   return NS_OK;

    aEvent->StopPropagation();
    aEvent->PreventDefault();
    return Up();
  }
  
  if (keyCode == mService->mKeyCodeDown)
  {
    // If we are going up or down, in a select, lets not
    // navigate.
    //
    // FIX: What we really want to do is determine if we are
    // at the start or the end fo the form element, and
    // based on the selected position we decide to nav. or
    // not.

    // ************************************************************************************
    // NS_FORM_SELECT cases:
    // * if it is a select form of 'size' attr != than '1' then we do as above.

    // * if it is a select form of 'size' attr == than '1', snav can take care of it.
    // if (formControlType == NS_FORM_SELECT)
    //   return NS_OK;

    aEvent->StopPropagation();  // We're using this key, no one else should
    aEvent->PreventDefault();
    return Down();
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsSpatialNavigation::Init(nsIDOMWindow *aWindow)
{
  mTopWindow = aWindow;

  nsCOMPtr<nsIDOM3EventTarget> target;
  nsCOMPtr<nsIDOMEventGroup> systemGroup;
  nsresult rv = getEventTargetFromWindow(aWindow, getter_AddRefs(target), getter_AddRefs(systemGroup));
  if (NS_FAILED(rv))
    return rv;  
  
  target->AddGroupedEventListener(NS_LITERAL_STRING("keypress"),
                                  NS_STATIC_CAST(nsIDOMKeyListener*, this),
                                  PR_FALSE, 
                                  systemGroup);
  
  return NS_OK;
}

NS_IMETHODIMP 
nsSpatialNavigation::Shutdown()
{
  nsCOMPtr<nsIDOM3EventTarget> target;
  nsCOMPtr<nsIDOMEventGroup> systemGroup;
  nsresult rv = getEventTargetFromWindow(mTopWindow, getter_AddRefs(target), getter_AddRefs(systemGroup));
  if (NS_FAILED(rv))
    return rv;
  
  target->RemoveGroupedEventListener(NS_LITERAL_STRING("keypress"),
                                     NS_STATIC_CAST(nsIDOMKeyListener*, this),
                                     PR_FALSE, 
                                     systemGroup);
  mTopWindow = nsnull;

  return NS_OK;
}

NS_IMETHODIMP 
nsSpatialNavigation::Up()
{
  return handleMove(eNavUp);
}

NS_IMETHODIMP 
nsSpatialNavigation::Down()
{
  return handleMove(eNavDown);
}

NS_IMETHODIMP 
nsSpatialNavigation::Left()
{
  return handleMove(eNavLeft);
}

NS_IMETHODIMP 
nsSpatialNavigation::Right()
{
  return handleMove(eNavRight);
}

NS_IMETHODIMP 
nsSpatialNavigation::GetAttachedWindow(nsIDOMWindow * *aAttachedWindow)
{
  NS_IF_ADDREF(*aAttachedWindow = mTopWindow);
  return NS_OK;
}

void DoTraversal(int aDirection,
                 nsIBidirectionalEnumerator* aFrameTraversal, 
                 nsIFrame* aFocusedFrame,
                 nsRect& aFocusedRect,
                 PRBool isAREA,
                 PRBool focusDocuments,
                 nsPresContext* aPresContext, 
                 PRInt64* aDSoFar,                 
                 nsIContent** aCurrentContent)
{

  *aCurrentContent = nsnull;

  // There are some rects with zero width or height.
  // calculating distance from a non point is troublesome.
  // we will fix this up here by setting such a point to at
  // least 1 px.

  if (aFocusedRect.width == 0) {
    aFocusedRect.width = 1;
  }

  if (aFocusedRect.height == 0) {
    aFocusedRect.height = 1;
  }

  nsIFrame* frame;
  nsRect frameRect;
  
  PRInt64 d;
  
  while (1) 
  {
    aFrameTraversal->Next();
    
    nsISupports* currentItem;
    aFrameTraversal->CurrentItem(&currentItem);
    frame = NS_STATIC_CAST(nsIFrame*, currentItem);

    if (!frame)
      break;
#ifdef DEBUG_outputframes
    printf("got frame %x\n", frame);
#endif
    // So, here we want to make sure that the frame that we
    // nav to isn't part of the flow of the currently
    // focused frame

	if (!isAREA) 
    {
      nsIFrame* flowFrame = aFocusedFrame;
      PRBool currentFrameIsFocusedFrame = PR_FALSE;
      while (flowFrame) 
      {
        if (flowFrame == frame) 
        {
          currentFrameIsFocusedFrame = PR_TRUE;
          break;
        }
        flowFrame = flowFrame->GetNextInFlow();
      }
      if (currentFrameIsFocusedFrame)
        continue;
	}

    ////////////////////////////////////////////////////////////////////////////////////
    // a special case for area's which are not enumerated by the nsIFrameTraversal
    ////////////////////////////////////////////////////////////////////////////////////    
    if (isMap(frame)) 
    {
      nsIContent* c = frame->GetContent();
      nsCOMPtr<nsIDOMHTMLMapElement> element = do_QueryInterface(c);
      if (!element)
        continue;

      nsCOMPtr<nsIDOMHTMLCollection> mapAreas;
      element->GetAreas(getter_AddRefs(mapAreas));
      if (!mapAreas)
        continue;
      
      PRUint32 length;
      mapAreas->GetLength(&length);
      
      for (PRUint32 i = 0; i < length; i++) 
      {
        nsCOMPtr<nsIDOMNode> domNode;
        mapAreas->Item(i,getter_AddRefs(domNode));
        
        nsCOMPtr<nsIDOMHTMLAreaElement> e = do_QueryInterface(domNode);        
		nsCOMPtr<nsIContent> content= do_QueryInterface(domNode);
		getFrameForContent(content, &frame);
        
        getRectOfAreaElement(frame, e, &frameRect);
        
        if (!isRectInDirection(aDirection, aFocusedRect, frameRect))
          continue;
        
        d = spatialDistance(aDirection, aFocusedRect, frameRect);
        
        if ((*aDSoFar) > d) 
        {
          (*aDSoFar) = d;
          NS_IF_RELEASE(*aCurrentContent);
          NS_ADDREF(*aCurrentContent = content);
        }      
      }
      continue;
    }
    ////////////////////////////////////////////////////////////////////////////////////
    
    // we don't want to worry about the same frame if we
    // aren't an area
    if (frame == aFocusedFrame ||
        (aFocusedFrame && (frame->GetContent() == aFocusedFrame->GetContent())))
      continue;

	// make sure the frame is targetable for focus
	if (!isTargetable(focusDocuments, frame))
      continue;
	
    // RECT !!
    frameRect = makeRectRelativeToGlobalView(frame);

    // no frames without size and be navigated to
    // successfully.
    if (frameRect.width == 0 || frameRect.height == 0)
      continue;
    
    // deflate the rect to avoid overlapping with other
    // rects.
    frameRect.Deflate(gRectFudge, gRectFudge);

    if (!isRectInDirection(aDirection, aFocusedRect, frameRect))
      continue;
    
    d = spatialDistance(aDirection, aFocusedRect, frameRect);
    
    if ((*aDSoFar) >= d) 
    {
#ifdef DEBUG_dougt
      if (d == 0)
      {
        printf("there is overlapping content;\n");
      }
#endif
      (*aDSoFar) = d;
      NS_IF_RELEASE(*aCurrentContent);
      NS_ADDREF(*aCurrentContent = frame->GetContent());
    }
  }
}

inline void centerRect(int aDirection, nsRect& aRect)
{
  if (aDirection == eNavLeft)
    aRect.x = 1000000;
  else if (aDirection == eNavRight)
    aRect.x = 0;
  else if (aDirection == eNavUp)
    aRect.y = 1000000;
  else
    aRect.y = 0;

  aRect.height = 1;
  aRect.width = 1;
}

nsresult
nsSpatialNavigation::getContentInDirection(int aDirection, 
                                           nsPresContext* aPresContext, 
                                           nsRect& aFocusedRect, 
                                           nsIFrame* aFocusedFrame, 
                                           PRBool aIsAREA,
                                           PRBool aFocusDocuments,
                                           nsIContent** aContent)
{  

  // Check to see if we should decend into subdoc
  nsIContent* subFrameContent = aFocusedFrame->GetContent();
  nsCOMPtr<nsIDOMHTMLHtmlElement> hhElement = do_QueryInterface(subFrameContent);
  nsCOMPtr<nsIDOMHTMLIFrameElement> iFrameElement = do_QueryInterface(subFrameContent);

  if ( (hhElement || iFrameElement) && mNavigationFramesState)
  {
    aPresContext = getPresContext(subFrameContent);
    centerRect(aDirection, aFocusedRect);
  }

  
  nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;
  nsresult result = createFrameTraversal(aPresContext,
                                         ePreOrder,
                                         PR_FALSE, // aVisual
                                         PR_FALSE, // aLockInScrollView
                                         PR_TRUE,  // aFollowOOFs
                                         getter_AddRefs(frameTraversal));  
  if (NS_FAILED(result))
    return result;
  
  nsCOMPtr<nsIContent> currentContent;
  PRInt64 currentDistance = LL_MaxInt();

  DoTraversal(aDirection,
              frameTraversal, 
              aFocusedFrame,
              aFocusedRect,
              aIsAREA,
              aFocusDocuments,
              aPresContext, 
              &currentDistance,
              aContent);


  if ( (hhElement || iFrameElement) && mNavigationFramesState)
  {
    mNavigationFramesState = PR_FALSE;
  }

  return NS_OK;
}


nsresult
nsSpatialNavigation::handleMove(int direction)
{
  nsCOMPtr<nsIContent> focusedContent;
  getFocusedContent(direction, getter_AddRefs(focusedContent));

  // there are some websites which have no focusable elements,
  // only text, for example. In these cases, scrolling have to be
  // performed by snav.
  if (!focusedContent) {
     ScrollWindow(direction, getContentWindow());
     return NS_OK;
  }
  nsPresContext* presContext = getPresContext(focusedContent);
  if(!presContext)
    return NS_ERROR_NULL_POINTER;

  nsIFrame* focusedFrame;
  getFrameForContent(focusedContent, &focusedFrame);

  nsRect focusedRect;
  PRBool isAREA = isArea(focusedContent);
  if (!isAREA) 
  {
    // RECT !!
    focusedRect = makeRectRelativeToGlobalView(focusedFrame);

    // deflate the rect to avoid overlapping with other
    // rects.
    focusedRect.Deflate(gRectFudge, gRectFudge);
  }
  else
  {
    nsCOMPtr<nsIDOMHTMLAreaElement> e = do_QueryInterface(focusedContent);
    getRectOfAreaElement(focusedFrame, e, &focusedRect);
  }

  nsCOMPtr<nsIContent> c;
  getContentInDirection(direction, presContext, focusedRect, focusedFrame, PR_FALSE, isAREA, getter_AddRefs(c));
  
  if (c) {
   
    nsIDocument* doc = c->GetDocument();
    if (!doc)
      return NS_ERROR_FAILURE;
    
/*    nsIPresShell *presShell = doc->GetShellAt(0);

    nsIFrame* cframe = presShell->GetPrimaryFrameFor(c);
    
    PRBool b = IsPartiallyVisible(presShell, cframe); 
    
    if (b)
      setFocusedContent(c);
    else
      ScrollWindow(direction, getContentWindow());*/

    setFocusedContent(c);
    return NS_OK;
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////
  // do it all again at the parent document
  ///////////////////////////////////////////////////////////////////////////////////////////////////

  {
    nsCOMPtr<nsIDOMWindow> contentWindow = getContentWindow();
    if (!contentWindow)
      return NS_OK;

    nsCOMPtr<nsIDOMDocument> domDoc;
    contentWindow->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);

    nsIPresShell *shell = doc->GetShellAt(0);
    if (!shell) return NS_OK;
  
    presContext = shell->GetPresContext();

	nsIFrame* parentFrame = shell->GetRootFrame();

    nsCOMPtr<nsIDocument> subdoc = focusedContent->GetDocument();
    if (!subdoc) return NS_OK;
    
    nsCOMPtr<nsIDOMDocument> subdomdoc = do_QueryInterface(subdoc);

    nsCOMPtr<nsIDOMWindowInternal> domWindowInternal;
    GetWindowFromDocument(subdomdoc, getter_AddRefs(domWindowInternal));
    if (!domWindowInternal) return NS_OK;

    nsCOMPtr<nsIDOMWindowInternal> domWindowInternal2 = domWindowInternal;
	domWindowInternal2->GetOpener(getter_AddRefs(domWindowInternal));
    if (!domWindowInternal) 
      domWindowInternal = domWindowInternal2;

    nsCOMPtr<nsIDOMWindow> subdocWindow = do_QueryInterface(domWindowInternal);
    if (!subdocWindow) return NS_OK;

    subdocWindow->GetDocument(getter_AddRefs(subdomdoc));
    if (!subdoc) return NS_OK;

    nsIPresShell *subdocShell = subdoc->GetShellAt(0);
    if (!subdocShell) return NS_OK;
  
    nsPresContext *subdocPresContext = subdocShell->GetPresContext();

    nsIFrame* subdocFrame = subdocShell->GetRootFrame();

    nsRect subdocRect = subdocFrame->GetRect();

    nsPoint frame_offset = subdocFrame->GetOffsetToExternal(parentFrame);
	
	subdocRect.x = frame_offset.x;
	subdocRect.y = frame_offset.y;

    getContentInDirection(direction, presContext, subdocRect, subdocFrame, PR_TRUE, PR_FALSE, getter_AddRefs(c));
  }

  if (c) {
    nsCOMPtr<nsIContent> subdocContent;
    getContentFromFrame(c, getter_AddRefs(subdocContent));

    if (subdocContent) {
      mNavigationFramesState = PR_TRUE;
      setFocusedContent(c);
      return NS_OK;
    }

    setFocusedContent(c);
    return NS_OK;
  }
  
  // if everything fails, default is to move the focus just as if the user hit tab.
  //  presContext->EventStateManager()->ShiftFocus(PR_TRUE, focusedContent);

  // how about this, if we find anything, we just scroll the
  // page in the direction of the navigation??
  ScrollWindow(direction, getContentWindow());

  ///////////////////////////////////////////////////////////////////////////////////////////////////

  return NS_OK;

}

void
nsSpatialNavigation::getFocusedContent(int direction, nsIContent** aContent)
{
  *aContent = nsnull;
  
  nsCOMPtr<nsIDOMWindow> contentWindow = getContentWindow();
  if (!contentWindow)
    return;
  
  nsCOMPtr<nsPIDOMWindow> privateWindow = do_QueryInterface(contentWindow);
  nsIFocusController *focusController = privateWindow->GetRootFocusController();
  
  if (!focusController)
    return;
  
  nsCOMPtr<nsIDOMElement> element;
  focusController->GetFocusedElement(getter_AddRefs(element));
  
  if (element)
  {
    nsCOMPtr<nsIContent> content = do_QueryInterface(element);
    NS_IF_ADDREF(*aContent = content);
    return;
  }

  //xxxx should/can we prevent it from going into chrome???
  if (direction == eNavLeft || direction == eNavUp)
    focusController->MoveFocus(PR_FALSE, nsnull);
  else
    focusController->MoveFocus(PR_TRUE, nsnull);
  
  // so there is no focused content -- lets make some up, hightlight it and return.  
  focusController->GetFocusedElement(getter_AddRefs(element));
}

void 
nsSpatialNavigation::setFocusedContent(nsIContent* c)
{
  if (!c)
    return;

  nsCOMPtr<nsIContent> subdocContent;
  getContentFromFrame(c, getter_AddRefs(subdocContent));

  if (subdocContent) {
    c = subdocContent;
  }

  nsIContent* currentContent = c;
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(currentContent);
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(element));

  nsCOMPtr<nsIDOMWindow> contentWindow;
  if (mService->mDisableJSWhenFocusing)
    contentWindow = getContentWindow();

  // We do not want to have JS disable at anytime - see bug 51075
  // DisableJSScope foopy (contentWindow);

  //#ifdef OLDER_LAYOUT  
  nsPresContext* presContext = getPresContext(c);
  
  nsIPresShell *presShell = presContext->PresShell();
  nsIFrame* frame = presShell->GetPrimaryFrameFor(c);
  
  if (frame) {
    presContext->EventStateManager()->SetContentState(c, NS_EVENT_STATE_FOCUS);
    
    presShell->ScrollFrameIntoView(frame, 
                                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,
                                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
    
    presContext->EventStateManager()->MoveCaretToFocus();
  }

  //#else
  nsCOMPtr<nsIDOMNSHTMLElement> nsElement = do_QueryInterface(element);
  if (nsElement) 
    nsElement->Focus();
  //#endif

}


nsIDOMWindow*
nsSpatialNavigation::getContentWindow()
{

  nsIDOMWindow* resultWindow = nsnull;

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mTopWindow);
  nsIDOMWindowInternal *rootWindow = window->GetPrivateRoot();
  
  nsCOMPtr<nsIDOMWindow> windowContent;
  rootWindow->GetContent(getter_AddRefs(windowContent));
  
  if (!windowContent)
	return nsnull;

  NS_ADDREF(resultWindow = windowContent);
  return resultWindow;
}


nsPresContext* 
nsSpatialNavigation::getPresContext(nsIContent* content)
{
  if (!content) return nsnull;
  
  nsCOMPtr<nsIDocument> doc = content->GetDocument();
  if (!doc) return nsnull;
  
  // the only case where there could be more shells in printpreview
  nsIPresShell *shell = doc->GetShellAt(0);
  if (!shell) return nsnull;
  
  nsPresContext *presContext = shell->GetPresContext();
  return presContext;
}


