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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Author: Eric D Vaughan <evaughan@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsBoxLayoutState.h"
#include "nsBox.h"
#include "nsBoxFrame.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsHTMLContainerFrame.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsFrameManager.h"
#include "nsLayoutAtoms.h"
#include "nsXULAtoms.h"
#include "nsIDOMNode.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMAttr.h"
#include "nsIWidget.h"
#include "nsIRenderingContext.h"
#include "nsIDocument.h"
#include "nsIDeviceContext.h"
#include "nsITheme.h"
#include "nsIServiceManager.h"

#ifdef DEBUG_COELESCED
static PRInt32 coelesced = 0;
#endif

#ifdef DEBUG_LAYOUT
PRInt32 gIndent = 0;
#endif

#ifdef DEBUG_LAYOUT
void
nsBoxAddIndents()
{
    for(PRInt32 i=0; i < gIndent; i++)
    {
        printf(" ");
    }
}
#endif

#ifdef DEBUG_LAYOUT
void
nsBox::AppendAttribute(const nsAutoString& aAttribute, const nsAutoString& aValue, nsAutoString& aResult)
{
   aResult.Append(aAttribute);
   aResult.AppendLiteral("='");
   aResult.Append(aValue);
   aResult.AppendLiteral("' ");
}

void
nsBox::ListBox(nsAutoString& aResult)
{
    nsAutoString name;
    nsIFrame* frame;
    GetFrame(&frame);
    GetBoxName(name);

    char addr[100];
    sprintf(addr, "[@%p] ", NS_STATIC_CAST(void*, frame));

    aResult.AppendASCII(addr);
    aResult.Append(name);
    aResult.AppendLiteral(" ");

    nsIContent* content = frame->GetContent();

    // add on all the set attributes
    if (content) {
      nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
      nsCOMPtr<nsIDOMNamedNodeMap> namedMap;

      node->GetAttributes(getter_AddRefs(namedMap));
      PRUint32 length;
      namedMap->GetLength(&length);

      nsCOMPtr<nsIDOMNode> attribute;
      for (PRUint32 i = 0; i < length; ++i)
      {
        namedMap->Item(i, getter_AddRefs(attribute));
        nsCOMPtr<nsIDOMAttr> attr(do_QueryInterface(attribute));
        attr->GetName(name);
        nsAutoString value;
        attr->GetValue(value);
        AppendAttribute(name, value, aResult);
      }
    }
}

NS_IMETHODIMP
nsBox::DumpBox(FILE* aFile)
{
  nsAutoString s;
  ListBox(s);
  fprintf(aFile, "%s", NS_LossyConvertUCS2toASCII(s).get());
  return NS_OK;
}
#endif

/**
 * Hack for deck who requires that all its children has widgets
 */
NS_IMETHODIMP 
nsBox::ChildrenMustHaveWidgets(PRBool& aMust)
{
  aMust = PR_FALSE;
  return NS_OK;
}

#ifdef DEBUG_LAYOUT
void
nsBox::GetBoxName(nsAutoString& aName)
{
  aName.AssignLiteral("Box");
}
#endif

NS_IMETHODIMP
nsBox::BeginLayout(nsBoxLayoutState& aState)
{
  #ifdef DEBUG_LAYOUT 

      nsBoxAddIndents();

      nsAutoString reason;
      switch(aState.LayoutReason())
      {
        case nsBoxLayoutState::Dirty:
           reason.AssignLiteral("Dirty");
        break;
        case nsBoxLayoutState::Initial:
           reason.AssignLiteral("Initial");
        break;
        case nsBoxLayoutState::Resize:
           reason.AssignLiteral("Resize");
        break;
      }

      char ch[100];
      reason.ToCString(ch,100);
      printf("%s Layout: ", ch);
      DumpBox(stdout);
      printf("\n");
      gIndent++;
  #endif

  return NS_OK;
}

NS_IMETHODIMP
nsBox::DoLayout(nsBoxLayoutState& aState)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBox::EndLayout(nsBoxLayoutState& aState)
{

  #ifdef DEBUG_LAYOUT
      --gIndent;
  #endif

  return SyncLayout(aState);
}

#ifdef REFLOW_COELESCED
void Coelesced()
{
   printf("Coelesed=%d\n", ++coelesced);

}

#endif

PRBool nsBox::gGotTheme = PR_FALSE;
nsITheme* nsBox::gTheme = nsnull;

MOZ_DECL_CTOR_COUNTER(nsBox)

nsBox::nsBox(nsIPresShell* aShell):mMouseThrough(unset),
                                   mNextChild(nsnull),
                                   mParentBox(nsnull)
                                   
{
  MOZ_COUNT_CTOR(nsBox);
  //mX = 0;
  //mY = 0;
  if (!gGotTheme) {
    gGotTheme = PR_TRUE;
    CallGetService("@mozilla.org/chrome/chrome-native-theme;1", &gTheme);
  }
}

nsBox::~nsBox()
{
  // NOTE:  This currently doesn't get called for |nsBoxToBlockAdaptor|
  // objects, so don't rely on putting anything here.
  MOZ_COUNT_DTOR(nsBox);
}

/* static */ void
nsBox::Shutdown()
{
  gGotTheme = PR_FALSE;
  NS_IF_RELEASE(gTheme);
}

NS_IMETHODIMP 
nsBox::GetInset(nsMargin& margin)
{
  margin.SizeTo(0,0,0,0);
  return NS_OK;
}

NS_IMETHODIMP
nsBox::IsDirty(PRBool& aDirty)
{
  nsIFrame* frame;
  GetFrame(&frame);
  aDirty = (frame->GetStateBits() & NS_FRAME_IS_DIRTY) != 0;
  return NS_OK;
}

NS_IMETHODIMP
nsBox::HasDirtyChildren(PRBool& aDirty)
{
  nsIFrame* frame;
  GetFrame(&frame);
  aDirty = (frame->GetStateBits() & NS_FRAME_HAS_DIRTY_CHILDREN) != 0;
  return NS_OK;
}

NS_IMETHODIMP
nsBox::MarkDirty(nsBoxLayoutState& aState)
{
  nsIFrame* frame;
  GetFrame(&frame);
  // only reflow if we aren't already dirty.
  if (frame->GetStateBits() & NS_FRAME_IS_DIRTY) {      
#ifdef DEBUG_COELESCED
      Coelesced();
#endif
      return NS_OK;
  }

  frame->AddStateBits(NS_FRAME_IS_DIRTY);

  NeedsRecalc();

  nsCOMPtr<nsIBoxLayout> layout;
  GetLayoutManager(getter_AddRefs(layout));
  if (layout)
    layout->BecameDirty(this, aState);

  if (frame->GetStateBits() & NS_FRAME_HAS_DIRTY_CHILDREN) {   
#ifdef DEBUG_COELESCED
      Coelesced();
#endif
      return NS_OK;
  }

  nsIBox* parent = nsnull;
  GetParentBox(&parent);
  if (parent)
     return parent->RelayoutDirtyChild(aState, this);
  else {
    return frame->GetParent()->ReflowDirtyChild(aState.PresShell(), frame);
  }
}

NS_IMETHODIMP
nsBox::MarkDirtyChildren(nsBoxLayoutState& aState)
{
  return RelayoutDirtyChild(aState, nsnull);
}

NS_IMETHODIMP
nsBox::MarkStyleChange(nsBoxLayoutState& aState)
{
  NeedsRecalc();

  if (HasStyleChange())
    return NS_OK;

  // iterate through all children making them dirty
  MarkChildrenStyleChange();

  nsCOMPtr<nsIBoxLayout> layout;
  GetLayoutManager(getter_AddRefs(layout));
  if (layout)
    layout->BecameDirty(this, aState);

  nsIBox* parent = nsnull;
  GetParentBox(&parent);
  if (parent)
     return parent->RelayoutDirtyChild(aState, this);
  else {
    /*
    nsIFrame* frame = nsnull;
    GetFrame(&frame);
    nsFrame::CreateAndPostReflowCommand(aState.PresShell(), frame, 
      nsHTMLReflowCommand::StyleChange, nsnull, nsnull, nsnull);
    return NS_OK;
    */
    nsIFrame* frame = nsnull;
    GetFrame(&frame);
    return frame->GetParent()->ReflowDirtyChild(aState.PresShell(), frame);
  }

  return NS_OK;
}

PRBool
nsBox::HasStyleChange()
{
  PRBool aDirty = PR_FALSE;
  IsDirty(aDirty);
  return aDirty;
}

void
nsBox::SetStyleChangeFlag(PRBool aDirty)
{
  NeedsRecalc();

  nsIFrame* frame;
  GetFrame(&frame);
  frame->AddStateBits(NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN);
}

NS_IMETHODIMP
nsBox::MarkChildrenStyleChange()
{
  // only reflow if we aren't already dirty.
  if (HasStyleChange()) {   
#ifdef DEBUG_COELESCED
    printf("StyleChange reflows coelesced=%d\n", ++StyleCoelesced);  
#endif
    return NS_OK;
  }

  SetStyleChangeFlag(PR_TRUE);

  nsIBox* child = nsnull;
  GetChildBox(&child);
  while(child)
  {
    child->MarkChildrenStyleChange();
    child->GetNextBox(&child);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBox::RelayoutStyleChange(nsBoxLayoutState& aState, nsIBox* aChild)
{
    NS_ERROR("Don't call this!!");

    /*
    // if we are not dirty mark ourselves dirty and tell our parent we are dirty too.
    if (!HasStyleChange()) {      
      // Mark yourself as dirty and needing to be recalculated
      SetStyleChangeFlag(PR_TRUE);
      NeedsRecalc();

      if (aChild != nsnull) {
          nsCOMPtr<nsIBoxLayout> layout;
          GetLayoutManager(getter_AddRefs(layout));
          if (layout)
            layout->ChildBecameDirty(this, aState, aChild);
      }

      nsIBox* parent = nsnull;
      GetParentBox(&parent);
      if (parent)
         return parent->RelayoutStyleChange(aState, this);
      else {
        nsIFrame* frame = nsnull;
        aChild->GetFrame(&frame);
        nsFrame::CreateAndPostReflowCommand(aState.PresShell(), frame, 
          eReflowType_StyleChanged, nsnull, nsnull, nsnull);
        return NS_OK;
      }
    } else {
#ifdef DEBUG_COELESCED
      Coelesced();
#endif
    }
   */

    return NS_OK;
}

NS_IMETHODIMP
nsBox::RelayoutDirtyChild(nsBoxLayoutState& aState, nsIBox* aChild)
{
    nsIFrame* frame;
    GetFrame(&frame);

    if (aChild != nsnull) {
        nsCOMPtr<nsIBoxLayout> layout;
        GetLayoutManager(getter_AddRefs(layout));
        if (layout)
          layout->ChildBecameDirty(this, aState, aChild);
    }

    // if we are not dirty mark ourselves dirty and tell our parent we are dirty too.
    if (!(frame->GetStateBits() & NS_FRAME_HAS_DIRTY_CHILDREN)) {      
      // Mark yourself as dirty and needing to be recalculated
      frame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);

      if (frame->GetStateBits() & NS_FRAME_REFLOW_ROOT) {
        nsFrame::CreateAndPostReflowCommand(aState.PresShell(), frame, 
                                            eReflowType_ReflowDirty,
                                            nsnull, nsnull, nsnull);
        return NS_OK;
      }

      NeedsRecalc();

      nsIBox* parentBox = nsnull;
      GetParentBox(&parentBox);
      if (parentBox)
         return parentBox->RelayoutDirtyChild(aState, this);
      return frame->GetParent()->ReflowDirtyChild(aState.PresShell(), frame);
    } else {
#ifdef DEBUG_COELESCED
      Coelesced();
#endif
    }

    return NS_OK;
}

NS_IMETHODIMP
nsBox::RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIBox* aChild)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetVAlign(Valignment& aAlign)
{
  aAlign = vAlign_Top;
   return NS_OK;
}

NS_IMETHODIMP
nsBox::GetHAlign(Halignment& aAlign)
{
  aAlign = hAlign_Left;
   return NS_OK;
}

NS_IMETHODIMP
nsBox::GetClientRect(nsRect& aClientRect)
{
  GetContentRect(aClientRect);

  nsMargin borderPadding;
  GetBorderAndPadding(borderPadding);

  aClientRect.Deflate(borderPadding);

  nsMargin insets;
  GetInset(insets);

  aClientRect.Deflate(insets);
  if (aClientRect.width < 0)
     aClientRect.width = 0;

  if (aClientRect.height < 0)
     aClientRect.height = 0;

 // NS_ASSERTION(aClientRect.width >=0 && aClientRect.height >= 0, "Content Size < 0");

  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetContentRect(nsRect& aContentRect)
{
  GetBounds(aContentRect);
  aContentRect.x = 0;
  aContentRect.y = 0;
  NS_BOX_ASSERTION(this, aContentRect.width >=0 && aContentRect.height >= 0, "Content Size < 0");
  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetBounds(nsRect& aRect)
{
   nsIFrame* frame = nsnull;
   GetFrame(&frame);
   aRect = frame->GetRect();
   return NS_OK;
}

NS_IMETHODIMP
nsBox::SetBounds(nsBoxLayoutState& aState, const nsRect& aRect, PRBool aRemoveOverflowArea)
{
    NS_BOX_ASSERTION(this, aRect.width >=0 && aRect.height >= 0, "SetBounds Size < 0");

    nsRect rect(0,0,0,0);
    GetBounds(rect);

    nsIFrame* frame = nsnull;
    GetFrame(&frame);

    nsPresContext* presContext = aState.PresContext();

    PRUint32 flags = 0;
    GetLayoutFlags(flags);

    PRUint32 stateFlags = aState.LayoutFlags();

    flags |= stateFlags;

    if (flags & NS_FRAME_NO_MOVE_FRAME)
      frame->SetSize(nsSize(aRect.width, aRect.height));
    else
      frame->SetRect(aRect);

    // Nuke the overflow area. The caller is responsible for restoring
    // it if necessary.
    if (aRemoveOverflowArea && (frame->GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN)) {
      // remove the previously stored overflow area 
      presContext->PropertyTable()->DeleteProperty(frame,
                                          nsLayoutAtoms::overflowAreaProperty);
      frame->RemoveStateBits(NS_FRAME_OUTSIDE_CHILDREN);
    }

    if (!(flags & NS_FRAME_NO_MOVE_VIEW))
    {
      nsContainerFrame::PositionFrameView(presContext, frame);
      if ((rect.x != aRect.x) || (rect.y != aRect.y))
        nsContainerFrame::PositionChildViews(presContext, frame);
    }
  

   /*  
    // only if the origin changed
    if ((rect.x != aRect.x) || (rect.y != aRect.y))  {
      if (frame->HasView()) {
        nsContainerFrame::PositionFrameView(presContext, frame,
                                            frame->GetView());
      } else {
        nsContainerFrame::PositionChildViews(presContext, frame);
      }
    }
    */
    

    return NS_OK;
}

void
nsBox::GetLayoutFlags(PRUint32& aFlags)
{
  aFlags = 0;
}


NS_IMETHODIMP
nsBox::GetBorderAndPadding(nsMargin& aBorderAndPadding)
{
  nsMargin border;
  nsresult rv = GetBorder(border);
  if (NS_FAILED(rv))
    return rv;

  nsMargin padding;
  rv = GetPadding(padding);
  if (NS_FAILED(rv))
    return rv;

  aBorderAndPadding.SizeTo(0,0,0,0);
  aBorderAndPadding += border;
  aBorderAndPadding += padding;

  return rv;
}

NS_IMETHODIMP
nsBox::GetBorder(nsMargin& aMargin)
{
  nsIFrame* frame = nsnull;
  GetFrame(&frame);

  aMargin.SizeTo(0,0,0,0);
    
  const nsStyleDisplay* disp = frame->GetStyleDisplay();
  if (disp->mAppearance && gTheme) {
    // Go to the theme for the border.
    nsPresContext *context = frame->GetPresContext();
    if (gTheme->ThemeSupportsWidget(context, frame, disp->mAppearance)) {
      nsMargin margin(0,0,0,0);
      gTheme->GetWidgetBorder(context->DeviceContext(), frame, 
                              disp->mAppearance, &margin);
      float p2t = context->ScaledPixelsToTwips();
      aMargin.top = NSIntPixelsToTwips(margin.top, p2t);
      aMargin.right = NSIntPixelsToTwips(margin.right, p2t);
      aMargin.bottom = NSIntPixelsToTwips(margin.bottom, p2t);
      aMargin.left = NSIntPixelsToTwips(margin.left, p2t);
      return NS_OK;
    }
  }

  frame->GetStyleBorder()->GetBorder(aMargin);

  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetPadding(nsMargin& aMargin)
{
  nsIFrame* frame = nsnull;
  GetFrame(&frame);

  const nsStyleDisplay* disp = frame->GetStyleDisplay();
  if (disp->mAppearance && gTheme) {
    // Go to the theme for the padding.
    nsPresContext *context = frame->GetPresContext();
    if (gTheme->ThemeSupportsWidget(context, frame, disp->mAppearance)) {
      nsMargin margin(0,0,0,0);
      PRBool useThemePadding;

      useThemePadding = gTheme->GetWidgetPadding(context->DeviceContext(),
                                                 frame, disp->mAppearance,
                                                 &margin);
      if (useThemePadding) {
        float p2t = context->ScaledPixelsToTwips();
        aMargin.top = NSIntPixelsToTwips(margin.top, p2t);
        aMargin.right = NSIntPixelsToTwips(margin.right, p2t);
        aMargin.bottom = NSIntPixelsToTwips(margin.bottom, p2t);
        aMargin.left = NSIntPixelsToTwips(margin.left, p2t);
        return NS_OK;
      }
    }
  }

  aMargin.SizeTo(0,0,0,0);
  frame->GetStylePadding()->GetPadding(aMargin);

  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetMargin(nsMargin& aMargin)
{
  nsIFrame* frame = nsnull;
  GetFrame(&frame);

  aMargin.SizeTo(0,0,0,0);
  frame->GetStyleMargin()->GetMargin(aMargin);

  return NS_OK;
}

NS_IMETHODIMP 
nsBox::GetChildBox(nsIBox** aBox)
{
    *aBox = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsBox::GetNextBox(nsIBox** aBox)
{
  *aBox = mNextChild;
  return NS_OK;
}

NS_IMETHODIMP
nsBox::SetNextBox(nsIBox* aBox)
{
  mNextChild = aBox;
  return NS_OK;
}

NS_IMETHODIMP
nsBox::SetParentBox(nsIBox* aParent)
{
  mParentBox = aParent;
  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetParentBox(nsIBox** aParent)
{
  *aParent = mParentBox;
  return NS_OK;
}

NS_IMETHODIMP
nsBox::NeedsRecalc()
{
  return NS_OK;
}

void
nsBox::SizeNeedsRecalc(nsSize& aSize)
{
  aSize.width  = -1;
  aSize.height = -1;
}

void
nsBox::CoordNeedsRecalc(PRInt32& aFlex)
{
  aFlex = -1;
}

PRBool
nsBox::DoesNeedRecalc(const nsSize& aSize)
{
  return (aSize.width == -1 || aSize.height == -1);
}

PRBool
nsBox::DoesNeedRecalc(nscoord aCoord)
{
  return (aCoord == -1);
}

PRBool
nsBox::GetWasCollapsed(nsBoxLayoutState& aState)
{
  nsIFrame* frame = nsnull;
  GetFrame(&frame);
  return (frame->GetStateBits() & NS_STATE_IS_COLLAPSED) != 0;
}

void
nsBox::SetWasCollapsed(nsBoxLayoutState& aState, PRBool aCollapsed)
{
  nsIFrame* frame;
  GetFrame(&frame);
  if (aCollapsed)
     frame->AddStateBits(NS_STATE_IS_COLLAPSED);
  else
     frame->RemoveStateBits(NS_STATE_IS_COLLAPSED);
}

NS_IMETHODIMP 
nsBox::Collapse(nsBoxLayoutState& aState)
{
 // if (GetWasCollapsed(aState))
 //   return NS_OK;

  SetWasCollapsed(aState, PR_TRUE);
  nsIFrame* frame;
  GetFrame(&frame);
  SetBounds(aState, nsRect(0,0,0,0), PR_TRUE);

  return CollapseChild(aState, frame, PR_TRUE);
}

nsresult 
nsBox::CollapseChild(nsBoxLayoutState& aState, nsIFrame* aFrame, PRBool aHide)
{
      nsPresContext* presContext = aState.PresContext();

    // shrink the view
      nsIView* view = aFrame->GetView();

      // if we find a view stop right here. All views under it
      // will be clipped.
      if (view) {
         // already hidden? We are done.
         //nsViewVisibility v = view->GetVisibility();
         //if (v == nsViewVisibility_kHide)
           //return NS_OK;

         if (aHide) {
           view->GetViewManager()->SetViewVisibility(view, nsViewVisibility_kHide);
         } else {
           view->GetViewManager()->SetViewVisibility(view, nsViewVisibility_kShow);
         }

         if (view->HasWidget()) {
           return NS_OK;
         }
      }

      // collapse the child
      nsIFrame* child = aFrame->GetFirstChild(nsnull);
       
      while (nsnull != child) 
      {
         CollapseChild(aState, child, aHide);
         child = child->GetNextSibling();
      }

      return NS_OK;
}

NS_IMETHODIMP 
nsBox::UnCollapse(nsBoxLayoutState& aState)
{
  if (!GetWasCollapsed(aState))
    return NS_OK;

  SetWasCollapsed(aState, PR_FALSE);

  return UnCollapseChild(aState, this);
}

nsresult 
nsBox::UnCollapseChild(nsBoxLayoutState& aState, nsIBox* aBox)
{
      nsIFrame* frame;
      aBox->GetFrame(&frame);
     
      // collapse the child
      nsIBox* child = nsnull;
      aBox->GetChildBox(&child);

      if (child == nsnull) {
        frame->AddStateBits(NS_FRAME_IS_DIRTY);
      } else {
          child->GetFrame(&frame);
          frame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
       
          while (nsnull != child) 
          {
             UnCollapseChild(aState, child);
             nsresult rv = child->GetNextBox(&child);
             NS_ASSERTION(rv == NS_OK,"failed to get next child");
          }
      }

      return NS_OK;
}

NS_IMETHODIMP
nsBox::SetLayoutManager(nsIBoxLayout* aLayout)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetLayoutManager(nsIBoxLayout** aLayout)
{
  *aLayout = nsnull;
  return NS_OK;
}


NS_IMETHODIMP
nsBox::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  aSize.width = 0;
  aSize.height = 0;

  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed)
    return NS_OK;

  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSPrefSize(aState, this, aSize);

  nsSize minSize(0, 0), maxSize(0, 0);
  GetMinSize(aState, minSize);
  GetMaxSize(aState, maxSize);

  BoundsCheck(minSize, aSize, maxSize);

  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetMinSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  aSize.width = 0;
  aSize.height = 0;

  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed)
    return NS_OK;

  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMinSize(aState, this, aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetMaxSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  aSize.width = NS_INTRINSICSIZE;
  aSize.height = NS_INTRINSICSIZE;

  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed)
    return NS_OK;

  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMaxSize(aState, this, aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetFlex(nsBoxLayoutState& aState, nscoord& aFlex)
{
  aFlex = 0;

  GetDefaultFlex(aFlex);
  nsIBox::AddCSSFlex(aState, this, aFlex);

  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetOrdinal(nsBoxLayoutState& aState, PRUint32& aOrdinal)
{
  aOrdinal = DEFAULT_ORDINAL_GROUP;
  nsIBox::AddCSSOrdinal(aState, this, aOrdinal);

  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetAscent(nsBoxLayoutState& aState, nscoord& aAscent)
{
  aAscent = 0;
  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed)
    return NS_OK;

  nsSize size(0,0);
  nsresult rv = GetPrefSize(aState, size);
  aAscent = size.height;
  return rv;
}

NS_IMETHODIMP
nsBox::IsCollapsed(nsBoxLayoutState& aState, PRBool& aCollapsed)
{
  aCollapsed = PR_FALSE;
  nsIBox::AddCSSCollapsed(aState, this, aCollapsed);

  return NS_OK;
}

NS_IMETHODIMP
nsBox::Layout(nsBoxLayoutState& aState)
{
  BeginLayout(aState);

  DoLayout(aState);

  EndLayout(aState);

  return NS_OK;
}

NS_IMETHODIMP 
nsBox::GetOrientation(PRBool& aIsHorizontal)
{
   nsIFrame* frame = nsnull;
   GetFrame(&frame);
   aIsHorizontal = (frame->GetStateBits() & NS_STATE_IS_HORIZONTAL) != 0;
   return NS_OK;
}

NS_IMETHODIMP 
nsBox::GetDirection(PRBool& aIsNormal)
{
   nsIFrame* frame = nsnull;
   GetFrame(&frame);
   aIsNormal = (frame->GetStateBits() & NS_STATE_IS_DIRECTION_NORMAL) != 0;
   return NS_OK;
}

nsresult
nsBox::SyncLayout(nsBoxLayoutState& aState)
{
  /*
  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed) {
    CollapseChild(aState, this, PR_TRUE);
    return NS_OK;
  }
  */
  

  PRBool dirty = PR_FALSE;
  IsDirty(dirty);
  if (dirty || aState.LayoutReason() == nsBoxLayoutState::Initial)
     Redraw(aState);

  nsIFrame* frame;
  GetFrame(&frame);
  frame->RemoveStateBits(NS_FRAME_HAS_DIRTY_CHILDREN | NS_FRAME_IS_DIRTY
                         | NS_FRAME_FIRST_REFLOW | NS_FRAME_IN_REFLOW);

  nsPresContext* presContext = aState.PresContext();

  PRUint32 flags = 0;
  GetLayoutFlags(flags);

  PRUint32 stateFlags = aState.LayoutFlags();

  flags |= stateFlags;

  nsRect rect(nsPoint(0, 0), frame->GetSize());

  if (ComputesOwnOverflowArea()) {
    if (frame->GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN) {
      nsRect* overflow = frame->GetOverflowAreaProperty();
      NS_ASSERTION(overflow, "should have overflow area property");
      rect = *overflow;
    }
  } else {
    if (!DoesClipChildren()) {
      // See if our child frames caused us to overflow after being laid
      // out. If so, store the overflow area.  This normally can't happen
      // in XUL, but it can happen with the CSS 'outline' property and
      // possibly with other exotic stuff (e.g. relatively positioned
      // frames in HTML inside XUL).
      nsIBox* box;
      GetChildBox(&box);
      while (box)
        {    
          nsIFrame* f = nsnull;
          box->GetFrame(&f);
  
          if (f) {
            nsRect bounds;
            if (f->GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN) {
              nsRect* overflowArea = f->GetOverflowAreaProperty();
              NS_ASSERTION(overflowArea, "Should have created property for overflowing frame");
              bounds = *overflowArea + f->GetPosition();
            } else {
              bounds = f->GetRect();
            }
            rect.UnionRect(rect, bounds);
          }

          box->GetNextBox(&box);
        }
    }

    frame->FinishAndStoreOverflow(&rect, frame->GetSize());
  }

  nsIView* view = frame->GetView();
  if (view) {
    // Make sure the frame's view is properly sized and positioned and has
    // things like opacity correct
    nsHTMLContainerFrame::SyncFrameViewAfterReflow(
                             presContext, 
                             frame, 
                             view,
                             &rect,
                             flags);
  } 

  return NS_OK;
}

NS_IMETHODIMP
nsBox::Redraw(nsBoxLayoutState& aState,
              const nsRect*   aDamageRect,
              PRBool          aImmediate)
{
  if (aState.PaintingDisabled())
    return NS_OK;

  nsPresContext* presContext = aState.PresContext();
  const nsHTMLReflowState* s = aState.GetReflowState();
  if (s) {
    if (s->reason != eReflowReason_Incremental)
      return NS_OK;
  }

  nsIFrame* frame = nsnull;
  GetFrame(&frame);

  nsRect damageRect(0,0,0,0);
  if (aDamageRect)
    damageRect = *aDamageRect;
  else
    damageRect = frame->GetOverflowRect();

  frame->Invalidate(damageRect, aImmediate);

  return NS_OK;
}

PRBool 
nsIBox::AddCSSPrefSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize)
{
    PRBool widthSet = PR_FALSE;
    PRBool heightSet = PR_FALSE;
    nsIFrame* frame = nsnull;
    aBox->GetFrame(&frame);

    // add in the css min, max, pref
    const nsStylePosition* position = frame->GetStylePosition();

    // see if the width or height was specifically set
    if (position->mWidth.GetUnit() == eStyleUnit_Coord)  {
        aSize.width = position->mWidth.GetCoordValue();
        widthSet = PR_TRUE;
    }

    if (position->mHeight.GetUnit() == eStyleUnit_Coord) {
        aSize.height = position->mHeight.GetCoordValue();     
        heightSet = PR_TRUE;
    }
    
    nsIContent* content = frame->GetContent();
    // ignore 'height' and 'width' attributes if the actual element is not XUL
    // For example, we might be magic XUL frames whose primary content is an HTML
    // <select>
    if (content && content->IsContentOfType(nsIContent::eXUL)) {
        nsPresContext* presContext = aState.PresContext();

        nsAutoString value;
        PRInt32 error;

        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::width, value))
        {
            value.Trim("%");

            aSize.width =
              presContext->IntScaledPixelsToTwips(value.ToInteger(&error));
            widthSet = PR_TRUE;
        }

        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::height, value))
        {
            value.Trim("%");

            aSize.height = presContext->IntScaledPixelsToTwips(value.ToInteger(&error));
            heightSet = PR_TRUE;
        }
    }

    return (widthSet && heightSet);
}


PRBool 
nsIBox::AddCSSMinSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize)
{

    PRBool widthSet = PR_FALSE;
    PRBool heightSet = PR_FALSE;
    PRBool canOverride = PR_TRUE;

    nsIFrame* frame = nsnull;
    aBox->GetFrame(&frame);

    // See if a native theme wants to supply a minimum size.
    const nsStyleDisplay* display = frame->GetStyleDisplay();
    if (display->mAppearance) {
      nsITheme *theme = aState.PresContext()->GetTheme();
      if (theme && theme->ThemeSupportsWidget(aState.PresContext(), frame, display->mAppearance)) {
        nsSize size;
        const nsHTMLReflowState* reflowState = aState.GetReflowState();
        if (reflowState) {
          theme->GetMinimumWidgetSize(reflowState->rendContext, frame, 
                                      display->mAppearance, &size, &canOverride);
          float p2t = aState.PresContext()->ScaledPixelsToTwips();
          if (size.width) {
            aSize.width = NSIntPixelsToTwips(size.width, p2t);
            widthSet = PR_TRUE;
          }
          if (size.height) {
            aSize.height = NSIntPixelsToTwips(size.height, p2t);
            heightSet = PR_TRUE;
          }
        }
      }
    }

    // add in the css min, max, pref
    const nsStylePosition* position = frame->GetStylePosition();

    // same for min size. Unfortunately min size is always set to 0. So for now
    // we will assume 0 means not set.
    if (position->mMinWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinWidth.GetCoordValue();
        if (min && (!widthSet || (min > aSize.width && canOverride))) {
           aSize.width = min;
           widthSet = PR_TRUE;
        }
    }

    if (position->mMinHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinHeight.GetCoordValue();
        if (min && (!heightSet || (min > aSize.height && canOverride))) {
           aSize.height = min;
           heightSet = PR_TRUE;
        }
    }

    nsIContent* content = frame->GetContent();
    if (content) {
        nsPresContext* presContext = aState.PresContext();

        nsAutoString value;
        PRInt32 error;

        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::minwidth, value))
        {
            value.Trim("%");

            nscoord val =
              presContext->IntScaledPixelsToTwips(value.ToInteger(&error));
            if (val > aSize.width)
              aSize.width = val;
            widthSet = PR_TRUE;
        }

        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::minheight, value))
        {
            value.Trim("%");

            nscoord val =
              presContext->IntScaledPixelsToTwips(value.ToInteger(&error));
            if (val > aSize.height)
              aSize.height = val;

            heightSet = PR_TRUE;
        }
    }

    return (widthSet && heightSet);
}

PRBool 
nsIBox::AddCSSMaxSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize)
{  

    PRBool widthSet = PR_FALSE;
    PRBool heightSet = PR_FALSE;

    nsIFrame* frame = nsnull;
    aBox->GetFrame(&frame);

    // add in the css min, max, pref
    const nsStylePosition* position = frame->GetStylePosition();

    // and max
    if (position->mMaxWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxWidth.GetCoordValue();
        aSize.width = max;
        widthSet = PR_TRUE;
    }

    if (position->mMaxHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxHeight.GetCoordValue();
        aSize.height = max;
        heightSet = PR_TRUE;
    }

    nsIContent* content = frame->GetContent();
    if (content) {
        nsPresContext* presContext = aState.PresContext();

        nsAutoString value;
        PRInt32 error;

        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::maxwidth, value))
        {
            value.Trim("%");

            nscoord val =
              presContext->IntScaledPixelsToTwips(value.ToInteger(&error));
            aSize.width = val;
            widthSet = PR_TRUE;
        }

        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::maxheight, value))
        {
            value.Trim("%");

            nscoord val =
              presContext->IntScaledPixelsToTwips(value.ToInteger(&error));
            aSize.height = val;

            heightSet = PR_TRUE;
        }
    }

    return (widthSet || heightSet);
}

PRBool 
nsIBox::AddCSSFlex(nsBoxLayoutState& aState, nsIBox* aBox, nscoord& aFlex)
{
    PRBool flexSet = PR_FALSE;

    nsIFrame* frame = nsnull;
    aBox->GetFrame(&frame);

    // get the flexibility
    nsIContent* content = frame->GetContent();
    if (content) {
        PRInt32 error;
        nsAutoString value;

        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::flex, value))
        {
            value.Trim("%");
            aFlex = value.ToInteger(&error);
            flexSet = PR_TRUE;
        }
        else {
          // No attribute value.  Check CSS.
          const nsStyleXUL* boxInfo = frame->GetStyleXUL();
          if (boxInfo->mBoxFlex > 0.0f) {
            // The flex was defined in CSS.
            aFlex = (nscoord)boxInfo->mBoxFlex;
            flexSet = PR_TRUE;
          }
        }
    }

    return flexSet;
}

PRBool 
nsIBox::AddCSSCollapsed(nsBoxLayoutState& aState, nsIBox* aBox, PRBool& aCollapsed)
{
  nsIFrame* frame = nsnull;
  aBox->GetFrame(&frame);
  aCollapsed = frame->GetStyleVisibility()->mVisible ==
               NS_STYLE_VISIBILITY_COLLAPSE;
  return PR_TRUE;
}

PRBool 
nsIBox::AddCSSOrdinal(nsBoxLayoutState& aState, nsIBox* aBox, PRUint32& aOrdinal)
{
  PRBool ordinalSet = PR_FALSE;
  
  nsIFrame* frame = nsnull;
  aBox->GetFrame(&frame);

  // get the flexibility
  nsIContent* content = frame->GetContent();
  if (content) {
    PRInt32 error;
    nsAutoString value;

    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::ordinal, value)) {
      aOrdinal = value.ToInteger(&error);
      ordinalSet = PR_TRUE;
    }
    else {
      // No attribute value.  Check CSS.
      const nsStyleXUL* boxInfo = frame->GetStyleXUL();
      if (boxInfo->mBoxOrdinal > 1) {
        // The ordinal group was defined in CSS.
        aOrdinal = (nscoord)boxInfo->mBoxOrdinal;
        ordinalSet = PR_TRUE;
      }
    }
  }

  return ordinalSet;
}

void
nsBox::AddBorderAndPadding(nsSize& aSize)
{
  AddBorderAndPadding(this, aSize);
}

void
nsBox::AddMargin(nsSize& aSize)
{
  AddMargin(this, aSize);
}

void
nsBox::AddInset(nsSize& aSize)
{
  AddInset(this, aSize);
}

void
nsBox::AddBorderAndPadding(nsIBox* aBox, nsSize& aSize)
{
  nsMargin borderPadding(0,0,0,0);
  aBox->GetBorderAndPadding(borderPadding);
  AddMargin(aSize, borderPadding);
}

void
nsBox::AddMargin(nsIBox* aChild, nsSize& aSize)
{
  nsMargin margin(0,0,0,0);
  aChild->GetMargin(margin);
  AddMargin(aSize, margin);
}

void
nsBox::AddMargin(nsSize& aSize, const nsMargin& aMargin)
{
  if (aSize.width != NS_INTRINSICSIZE)
    aSize.width += aMargin.left + aMargin.right;

  if (aSize.height != NS_INTRINSICSIZE)
     aSize.height += aMargin.top + aMargin.bottom;
}

void
nsBox::AddInset(nsIBox* aBox, nsSize& aSize)
{
  nsMargin margin(0,0,0,0);
  aBox->GetInset(margin);
  AddMargin(aSize, margin);
}

void
nsBox::BoundsCheck(nscoord& aMin, nscoord& aPref, nscoord& aMax)
{
   if (aMin > aMax)
       aMin = aMax;

   if (aPref > aMax)
       aPref = aMax;

   if (aPref < aMin)
       aPref = aMin;
}

void
nsBox::BoundsCheck(nsSize& aMinSize, nsSize& aPrefSize, nsSize& aMaxSize)
{
   BoundsCheck(aMinSize.width, aPrefSize.width, aMaxSize.width);
   BoundsCheck(aMinSize.height, aPrefSize.height, aMaxSize.height);
}

#ifdef DEBUG_LAYOUT
nsresult
nsBox::SetDebug(nsBoxLayoutState& aState, PRBool aDebug)
{
    return NS_OK;
}

NS_IMETHODIMP
nsBox::GetDebugBoxAt( const nsPoint& aPoint,
                      nsIBox**     aBox)
{
  nsRect rect;
  GetBounds(rect);

  if (!rect.Contains(aPoint))
    return NS_ERROR_FAILURE;

  nsIBox* child = nsnull;
  nsIBox* hit = nsnull;
  GetChildBox(&child);

  *aBox = nsnull;
  nsPoint tmp;
  tmp.MoveTo(aPoint.x - rect.x, aPoint.y - rect.y);
  while (nsnull != child) {
    nsresult rv = child->GetDebugBoxAt(tmp, &hit);

    if (NS_SUCCEEDED(rv) && hit) {
      *aBox = hit;
    }
    child->GetNextBox(&child);
  }

  // found a child
  if (*aBox) {
    return NS_OK;
  }

 // see if it is in our in our insets
  nsMargin m;
  GetBorderAndPadding(m);
  rect.Deflate(m);
  if (rect.Contains(aPoint)) {
    GetInset(m);
    rect.Deflate(m);
    if (!rect.Contains(aPoint)) {
      *aBox = this;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsBox::GetDebug(PRBool& aDebug)
{
  aDebug = PR_FALSE;
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsBox::GetMouseThrough(PRBool& aMouseThrough)
{
  switch(mMouseThrough)
  {
    case always:
      aMouseThrough = PR_TRUE;
      return NS_OK;
    case never:
      aMouseThrough = PR_FALSE;      
      return NS_OK;
    case unset:
    {
      nsIBox* parent = nsnull;
      GetParentBox(&parent);
      if (parent)
        return parent->GetMouseThrough(aMouseThrough);
      else {
        aMouseThrough = PR_FALSE;      
        return NS_OK;
      }
    }
  }

  return NS_ERROR_FAILURE;
}

PRBool
nsBox::GetDefaultFlex(PRInt32& aFlex) 
{ 
  aFlex = 0; 
  return PR_TRUE; 
}


// nsISupports
NS_IMETHODIMP_(nsrefcnt) 
nsBox::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsBox::Release(void)
{
    return NS_OK;
}

NS_IMETHODIMP
nsBox::GetIndexOf(nsIBox* aChild, PRInt32* aIndex)
{
  // return -1. We have no children
  *aIndex = -1;
  return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsBox)
  NS_INTERFACE_MAP_ENTRY(nsIBox)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

