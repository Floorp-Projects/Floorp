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
 * Mike Pinkerton (pinkerton@netscape.com).
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <Gestalt.h>
#include "nsNativeThemeMac.h"
#include "nsRenderingContextMac.h"
#include "nsDeviceContextMac.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsTransform2D.h"
#include "nsThemeConstants.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIEventStateManager.h"
#include "nsINameSpaceManager.h"
#include "nsPresContext.h"
#include "nsILookAndFeel.h"
#include "nsRegionPool.h"
#include "nsGfxUtils.h"
#include "nsUnicharUtils.h"

static PRBool sInitializedBorders = PR_FALSE;

static void 
ConvertGeckoToNativeRect(const nsRect& aSrc, Rect& aDst) 
{
  aDst.top = aSrc.y;
  aDst.bottom = aSrc.y + aSrc.height;
  aDst.left = aSrc.x;
  aDst.right = aSrc.x + aSrc.width;
}

//
// DoNothing
//
// An eraseProc for drawing theme buttons so that we don't erase over anything
// that might be drawn before us in the background layer. Does absolutely
// nothing.
//
static pascal void
DoNothing(const Rect *bounds, UInt32 eraseData, SInt16 depth, Boolean isColorDev)
{
  // be gentle, erase nothing.
}


#ifdef XP_MAC
#pragma mark -
#endif

         
NS_IMPL_ISUPPORTS1(nsNativeThemeMac, nsITheme)

nsNativeThemeMac::nsNativeThemeMac()
  : mEraseProc(nsnull)
{
  mEraseProc = NewThemeEraseUPP(DoNothing);
  if (!sInitializedBorders) {
    sInitializedBorders = PR_TRUE;
    sTextfieldBorderSize.left = sTextfieldBorderSize.top = 2;
    sTextfieldBorderSize.right = sTextfieldBorderSize.bottom = 2;
    sTextfieldBGTransparent = PR_FALSE;
    sListboxBGTransparent = PR_TRUE;
  }
}

nsNativeThemeMac::~nsNativeThemeMac()
{
  if ( mEraseProc )
    ::DisposeThemeEraseUPP(mEraseProc);
}

#ifdef XP_MAC
#pragma mark -
#endif


void
nsNativeThemeMac::DrawCheckboxRadio ( ThemeButtonKind inKind, const Rect& inBoxRect, PRBool inChecked,
                                       PRBool inDisabled, PRInt32 inState )
{
  ThemeButtonDrawInfo info;
  if ( inDisabled )
    info.state = kThemeStateInactive;
  else
    info.state = ((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER)) ?
                     kThemeStatePressed : kThemeStateActive;
  info.value = inChecked ? kThemeButtonOn : kThemeButtonOff;
  info.adornment = (inState & NS_EVENT_STATE_FOCUS) ? kThemeAdornmentFocus : kThemeAdornmentNone;
  
  ::DrawThemeButton ( &inBoxRect, inKind, &info, nsnull, nsnull, nsnull, 0L );
}


void
nsNativeThemeMac::DrawCheckbox ( const Rect& inBoxRect, PRBool inChecked, PRBool inDisabled, PRInt32 inState )
{
  DrawCheckboxRadio(kThemeCheckBox, inBoxRect, inChecked, inDisabled, inState);
}

void
nsNativeThemeMac::DrawSmallCheckbox ( const Rect& inBoxRect, PRBool inChecked, PRBool inDisabled, PRInt32 inState )
{
  DrawCheckboxRadio(kThemeSmallCheckBox, inBoxRect, inChecked, inDisabled, inState);
}

void
nsNativeThemeMac::DrawRadio ( const Rect& inBoxRect, PRBool inChecked, PRBool inDisabled, PRInt32 inState )
{
  DrawCheckboxRadio(kThemeRadioButton, inBoxRect, inChecked, inDisabled, inState);
}

void
nsNativeThemeMac::DrawSmallRadio ( const Rect& inBoxRect, PRBool inChecked, PRBool inDisabled, PRInt32 inState )
{
  DrawCheckboxRadio(kThemeSmallRadioButton, inBoxRect, inChecked, inDisabled, inState);
}

void
nsNativeThemeMac::DrawButton ( ThemeButtonKind inKind, const Rect& inBoxRect, PRBool inIsDefault, 
                                  PRBool inDisabled, ThemeButtonValue inValue, ThemeButtonAdornment inAdornment,
                                  PRInt32 inState )
{
  ThemeButtonDrawInfo info;
  if ( inDisabled )
    info.state = kThemeStateUnavailableInactive;
  else
    info.state = ((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER)) ? 
                    kThemeStatePressed : kThemeStateActive;
  info.value = inValue;
  info.adornment = inAdornment;
  if ( inState & NS_EVENT_STATE_FOCUS ) {
    // There is a bug in OS 10.2 and higher where if we are in a CG context and
    // draw the focus ring with DrawThemeButton(), there are ugly lines all
    // through the button.  This may get fixed in a dot-release, but until it
    // does, we can't draw the focus ring.
    if (inKind != kThemePushButton || !nsRenderingContextMac::OnJaguar())
      info.adornment = kThemeAdornmentFocus;
  }
  if ( inIsDefault )
    info.adornment |= kThemeAdornmentDefault;

  ::DrawThemeButton ( &inBoxRect, inKind, &info, nsnull, mEraseProc, nsnull, 0L );
}


void
nsNativeThemeMac::DrawToolbar ( const Rect& inBoxRect )
{
#if 0
  const PRInt32 kThemeBrushToolbarBackground = 52;    // from 3.4.1 headers
  ::SetThemeBackground(kThemeBrushToolbarBackground, 24, true);
  ::EraseRect(&inBoxRect);
  ::SetThemeBackground(kThemeBrushWhite, 24, true);
printf("told to draw at %ld %ld w %ld h %ld\n", inBoxRect.left, inBoxRect.top, inBoxRect.right-inBoxRect.left,
        inBoxRect.bottom - inBoxRect.top);
#endif
  ThemeDrawState drawState = kThemeStateActive;
  ::DrawThemeWindowHeader(&inBoxRect, drawState);
}


void
nsNativeThemeMac::DrawEditText ( const Rect& inBoxRect, PRBool inIsDisabled )
{
#if TARGET_CARBON
  Pattern whitePat;
  ::BackColor(whiteColor);
  ::BackPat(GetQDGlobalsWhite(&whitePat));
  ::EraseRect(&inBoxRect);
  
  ThemeDrawState drawState = inIsDisabled ? kThemeStateDisabled : kThemeStateActive;
  ::DrawThemeEditTextFrame(&inBoxRect, drawState);
#endif
}


void
nsNativeThemeMac::DrawListBox ( const Rect& inBoxRect, PRBool inIsDisabled )
{
#if TARGET_CARBON
  Pattern whitePat;
  ::BackColor(whiteColor);
  ::BackPat(GetQDGlobalsWhite(&whitePat));
  ::EraseRect(&inBoxRect);
  
  ThemeDrawState drawState = inIsDisabled ? kThemeStateDisabled : kThemeStateActive;
  ::DrawThemeListBoxFrame(&inBoxRect, drawState);
#endif
}


void
nsNativeThemeMac::DrawProgress ( const Rect& inBoxRect, PRBool inIsDisabled, PRBool inIsIndeterminate, 
                                  PRBool inIsHorizontal, PRInt32 inValue )
{
  ThemeTrackDrawInfo info;
  static SInt32 sPhase = 0;
  
  info.kind = inIsIndeterminate ? kThemeMediumIndeterminateBar: kThemeMediumProgressBar;
  info.bounds = inBoxRect;
  info.min = 0;
  info.max = 100;
  info.value = inValue;
  info.attributes = inIsHorizontal ? kThemeTrackHorizontal : 0L;
  info.enableState = inIsDisabled ? kThemeTrackDisabled : kThemeTrackActive;
  info.trackInfo.progress.phase = sPhase++;       // animate for the next time we're called
  
  ::DrawThemeTrack(&info, nsnull, nsnull, 0L);
}


void
nsNativeThemeMac::DrawTabPanel ( const Rect& inBoxRect, PRBool inIsDisabled )
{
  ThemeDrawState drawState = inIsDisabled ? kThemeStateDisabled : kThemeStateActive;
  ::DrawThemeTabPane(&inBoxRect, drawState);
}


void
nsNativeThemeMac::DrawSeparator ( const Rect& inBoxRect, PRBool inIsDisabled )
{
  ThemeDrawState drawState = inIsDisabled ? kThemeStateDisabled : kThemeStateActive;
  ::DrawThemeSeparator(&inBoxRect, drawState);
}


void
nsNativeThemeMac::DrawTab ( const Rect& inBoxRect, PRBool inIsDisabled, PRBool inIsFrontmost, 
                              PRBool inIsHorizontal, PRBool inTabBottom, PRInt32 inState )
{
  ThemeTabStyle style = 0L;
  if ( inIsFrontmost ) {
    if ( inIsDisabled ) 
      style = kThemeTabFrontInactive;
    else
      style = kThemeTabFront;
  }
  else {
    if ( inIsDisabled )
      style = kThemeTabNonFrontInactive;
    else if ( (inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER) )
      style = kThemeTabNonFrontPressed;
    else
      style = kThemeTabNonFront;  
  }

  ThemeTabDirection direction = inTabBottom ? kThemeTabSouth : kThemeTabNorth; // don't yet support vertical tabs
  ::DrawThemeTab(&inBoxRect, style, direction, nsnull, 0L);
}

NS_IMETHODIMP
nsNativeThemeMac::DrawWidgetBackground(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                        PRUint8 aWidgetType, const nsRect& aRect, const nsRect& aClipRect)
{
  // setup to draw into the correct port
  nsIDrawingSurface* surf;
  aContext->GetDrawingSurface(&surf);
  nsDrawingSurfaceMac* macSurface = (nsDrawingSurfaceMac*)surf;
  CGrafPtr port = nsnull;
  NS_ASSERTION(macSurface,"no surface!!!\n");
  if ( macSurface )
    macSurface->GetGrafPtr(&port);
  else
    return NS_ERROR_FAILURE;      // we won't get far w/out something to draw into
  StPortSetter temp(port);

  // save off the old clip rgn for later restoration. however, we're currently
  // not using the cliprect because aqua likes to draw shadows and stuff outside
  // the bounds we give it, and clipping to the exact rect looks horrible.
  StRegionFromPool oldClip;
  ::GetClip(oldClip);

  // transform rect coordinates to correct coord system
  nsTransform2D* transformMatrix;
  aContext->GetCurrentTransform(transformMatrix);
  nsRect transRect(aRect), transClipRect(aClipRect);
  Rect macRect;
  transformMatrix->TransformCoord(&transRect.x, &transRect.y, &transRect.width, &transRect.height);
  ConvertGeckoToNativeRect(transRect, macRect);
#ifdef CLIP_DRAWING
  Rect clipRect;
  transformMatrix->TransformCoord(&transClipRect.x, &transClipRect.y, &transClipRect.width, &transClipRect.height);
  ConvertGeckoToNativeRect(transClipRect, clipRect);
  ::ClipRect(&clipRect);
#endif

  PRInt32 eventState = GetContentState(aFrame, aWidgetType);

  switch ( aWidgetType ) {
  
    case NS_THEME_DIALOG:
      ::SetThemeBackground(kThemeBrushDialogBackgroundActive, 24, true);
      ::EraseRect(&macRect);
      ::SetThemeBackground(kThemeBrushWhite, 24, true);
      break;
      
    case NS_THEME_MENUPOPUP:
      ::SetThemeBackground(kThemeBrushDialogBackgroundActive, 24, true);
      ::EraseRect(&macRect);
      ::SetThemeBackground(kThemeBrushWhite, 24, true);
      break;

    case NS_THEME_TOOLTIP:
    {
      RGBColor yellow = {65535,65535,45000};
      ::RGBBackColor(&yellow);
      ::EraseRect(&macRect);
      ::SetThemeBackground(kThemeBrushWhite, 24, true);
      break;
    }

    case NS_THEME_CHECKBOX:
      DrawCheckbox ( macRect, IsChecked(aFrame), IsDisabled(aFrame), eventState );
      break;    
    case NS_THEME_RADIO:
      DrawRadio ( macRect, IsSelected(aFrame), IsDisabled(aFrame), eventState );
      break;
    case NS_THEME_CHECKBOX_SMALL:
      if (transRect.height == 15) {
      	// draw at 14x16, see comment in GetMinimumWidgetSize
        ++macRect.bottom;
      }
      DrawSmallCheckbox ( macRect, IsChecked(aFrame), IsDisabled(aFrame), eventState );
      break;
    case NS_THEME_RADIO_SMALL:
      if (transRect.height == 14) {
        // draw at 14x15, see comment in GetMinimumWidgetSize
        ++macRect.bottom;
      }
      DrawSmallRadio ( macRect, IsSelected(aFrame), IsDisabled(aFrame), eventState );
      break;
    case NS_THEME_BUTTON:
    case NS_THEME_BUTTON_SMALL:
      DrawButton ( kThemePushButton, macRect, IsDefaultButton(aFrame), IsDisabled(aFrame), 
                    kThemeButtonOn, kThemeAdornmentNone, eventState );
      break;      
    case NS_THEME_BUTTON_BEVEL:
      DrawButton ( kThemeMediumBevelButton, macRect, IsDefaultButton(aFrame), IsDisabled(aFrame), 
                    kThemeButtonOff, kThemeAdornmentNone, eventState );
      break;
    case NS_THEME_TOOLBAR_BUTTON:
      DrawButton ( kThemePushButton, macRect, IsDefaultButton(aFrame), IsDisabled(aFrame),
                    kThemeButtonOn, kThemeAdornmentNone, eventState );
      break;
    case NS_THEME_TOOLBAR_SEPARATOR:
      DrawSeparator ( macRect, IsDisabled(aFrame) );
      break;
      
    case NS_THEME_TOOLBAR:
    case NS_THEME_TOOLBOX:
    case NS_THEME_STATUSBAR:
      DrawToolbar ( macRect );
      break;
      
    case NS_THEME_DROPDOWN:
      DrawButton ( kThemePopupButton, macRect, IsDefaultButton(aFrame), IsDisabled(aFrame), 
                    kThemeButtonOn, kThemeAdornmentNone, eventState );
      break;
    case NS_THEME_DROPDOWN_BUTTON:
      // do nothing, this is covered by the DROPDOWN case
      break;

    case NS_THEME_TEXTFIELD:
      DrawEditText ( macRect, IsDisabled(aFrame) );
      break;
      
    case NS_THEME_PROGRESSBAR:
      DrawProgress ( macRect, IsDisabled(aFrame), IsIndeterminateProgress(aFrame), PR_TRUE, GetProgressValue(aFrame) );
      break;
    case NS_THEME_PROGRESSBAR_VERTICAL:
      DrawProgress ( macRect, IsDisabled(aFrame), IsIndeterminateProgress(aFrame), PR_FALSE, GetProgressValue(aFrame) );
      break;
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
      // do nothing, covered by the progress bar cases above
      break;

    case NS_THEME_TREEVIEW_TWISTY:
      DrawButton ( kThemeDisclosureButton, macRect, PR_FALSE, IsDisabled(aFrame), 
                    kThemeDisclosureRight, kThemeAdornmentNone, eventState );
      break;
    case NS_THEME_TREEVIEW_TWISTY_OPEN:
      DrawButton ( kThemeDisclosureButton, macRect, PR_FALSE, IsDisabled(aFrame), 
                    kThemeDisclosureDown, kThemeAdornmentNone, eventState );
      break;
    case NS_THEME_TREEVIEW_HEADER_CELL:
      DrawButton ( kThemeListHeaderButton, macRect, PR_FALSE, IsDisabled(aFrame), 
                    IsSortedColumn(aFrame) ? kThemeButtonOn : kThemeButtonOff,
                    IsSortReversed(aFrame) ? kThemeAdornmentHeaderButtonSortUp : kThemeAdornmentNone,
                    eventState );      
      break;
    case NS_THEME_TREEVIEW_TREEITEM:
    case NS_THEME_TREEVIEW:
      ::SetThemeBackground(kThemeBrushWhite, 24, true);
      ::EraseRect ( &macRect );
      break;
    case NS_THEME_TREEVIEW_HEADER:
      // do nothing, taken care of by individual header cells
    case NS_THEME_TREEVIEW_HEADER_SORTARROW:
      // do nothing, taken care of by treeview header
    case NS_THEME_TREEVIEW_LINE:
      // do nothing, these lines don't exist on macos
      break;
    case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:
    case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL: 
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
      // Scrollbars are now native on mac, via nsNativeScrollbarFrame.
      // So, this should never be called.
      break;
    
    case NS_THEME_LISTBOX:
      DrawListBox(macRect, IsDisabled(aFrame));
      break;
    
    case NS_THEME_TAB:
      DrawTab(macRect, IsDisabled(aFrame), IsSelectedTab(aFrame), PR_TRUE, IsBottomTab(aFrame), eventState);
      break;      
    case NS_THEME_TAB_PANELS:
      DrawTabPanel(macRect, IsDisabled(aFrame));
      break;
  }

  ::SetClip(oldClip);
  
  return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#endif


NS_IMETHODIMP
nsNativeThemeMac::GetWidgetBorder(nsIDeviceContext* aContext, 
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsMargin* aResult)
{
  aResult->SizeTo(0,0,0,0);
      
  // XXX we should probably cache some of these metrics
  
#if TARGET_CARBON
  switch ( aWidgetType ) {
  
    case NS_THEME_BUTTON:
      if ( nsRenderingContextMac::OnMacOSX() )
        aResult->SizeTo(kAquaPushButtonEndcaps, kAquaPushButtonTopBottom, 
                            kAquaPushButtonEndcaps, kAquaPushButtonTopBottom);
      else
        aResult->SizeTo(5,2,5,2);     // 5px for AGA
      break;

    case NS_THEME_BUTTON_SMALL:
      aResult->SizeTo(kAquaSmallPushButtonEndcaps, kAquaPushButtonTopBottom,
                      kAquaSmallPushButtonEndcaps, kAquaPushButtonTopBottom);
      break;

    case NS_THEME_TOOLBAR_BUTTON:
      //aResult->SizeTo(5,5,5,5);    // 5px around the button in aqua
      break;

    case NS_THEME_DROPDOWN:
      if ( nsRenderingContextMac::OnMacOSX() )
        aResult->SizeTo(kAquaDropdownLeftEndcap, kAquaPushButtonTopBottom, 
                          kAquaDropwdonRightEndcap, kAquaPushButtonTopBottom);
      else
        aResult->SizeTo(3,0,3,0);     // 3px for AGA
      break;
    
    case NS_THEME_TEXTFIELD:
    {
      aResult->SizeTo(2, 2, 2, 2);
      break;
    }

    case NS_THEME_LISTBOX:
    {
      SInt32 frameOutset = 0;
      ::GetThemeMetric(kThemeMetricListBoxFrameOutset, &frameOutset);
      aResult->SizeTo(frameOutset, frameOutset, frameOutset, frameOutset);
      break;
    }
      
  }
#endif
  
  return NS_OK;
}

PRBool
nsNativeThemeMac::GetWidgetPadding(nsIDeviceContext* aContext, 
                                   nsIFrame* aFrame,
                                   PRUint8 aWidgetType,
                                   nsMargin* aResult)
{
  return PR_FALSE;
}

NS_IMETHODIMP
nsNativeThemeMac::GetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                       PRUint8 aWidgetType, nsSize* aResult, PRBool* aIsOverridable)
{
  // XXX we should probably cache some of these metrics
  aResult->SizeTo(0,0);
  *aIsOverridable = PR_TRUE;
  
#if TARGET_CARBON
  switch ( aWidgetType ) {
  
    case NS_THEME_BUTTON:
    {
      SInt32 buttonHeight = 0;
      ::GetThemeMetric(kThemeMetricPushButtonHeight, &buttonHeight);
      aResult->SizeTo(kAquaPushButtonEndcaps*2, buttonHeight);
      break;
    }
      
    case NS_THEME_BUTTON_SMALL:
    {
      SInt32 buttonHeight = 0;
      ::GetThemeMetric(kThemeMetricSmallPushButtonHeight, &buttonHeight);
      aResult->SizeTo(kAquaSmallPushButtonEndcaps*2, buttonHeight);
      break;
    }

    case NS_THEME_CHECKBOX:
    {
      SInt32 boxHeight = 0, boxWidth = 0;
      ::GetThemeMetric(kThemeMetricCheckBoxWidth, &boxWidth);
      ::GetThemeMetric(kThemeMetricCheckBoxHeight, &boxHeight);
      aResult->SizeTo(boxWidth, boxHeight);
      *aIsOverridable = PR_FALSE;
      break;
    }
    
    case NS_THEME_RADIO:
    {
      SInt32 radioHeight = 0, radioWidth = 0;
      ::GetThemeMetric(kThemeMetricRadioButtonWidth, &radioWidth);
      ::GetThemeMetric(kThemeMetricRadioButtonHeight, &radioHeight);
      aResult->SizeTo(radioWidth, radioHeight);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_CHECKBOX_SMALL:
    {
      // Appearance manager (and the Aqua HIG) will tell us that a small
      // checkbox is 14x16.  This includes a transparent row at the bottom
      // of the image.  In order to allow the baseline for text to be aligned
      // with the bottom of the checkbox, we report the size as 14x15, but
      // we'll always tell appearance manager to draw it at 14x16.  This
      // will result in Gecko aligning text with the real bottom of the
      // checkbox.

      aResult->SizeTo(14, 15);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_RADIO_SMALL:
    {
      // Same as above, but appearance manager reports 14x15, and we
      // tell gecko 14x14.

      aResult->SizeTo(14, 14);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_DROPDOWN:
    {
      SInt32 popupHeight = 0;
      ::GetThemeMetric(kThemeMetricPopupButtonHeight, &popupHeight);
      aResult->SizeTo(0, popupHeight);
      break;
    }
    
    case NS_THEME_DROPDOWN_BUTTON:
      // the drawing for this is done by the dropdown, so just make this
      // zero sized.
      aResult->SizeTo(0,0);
      break;
      
    case NS_THEME_TEXTFIELD:
    {
      // at minimum, we should be tall enough for 9pt text.
      // I'm using hardcoded values here because the appearance manager
      // values for the frame size are incorrect.
      aResult->SizeTo(0, (2 + 2) /* top */ + 9 + (1 + 1) /* bottom */ );
      break;
    }
      
    case NS_THEME_PROGRESSBAR:
    {
      SInt32 barHeight = 0;
      ::GetThemeMetric(kThemeMetricNormalProgressBarThickness, &barHeight);
      aResult->SizeTo(0, barHeight);
      break;
    }

    case NS_THEME_TREEVIEW_TWISTY:
    case NS_THEME_TREEVIEW_TWISTY_OPEN:   
    {
      SInt32 twistyHeight = 0, twistyWidth = 0;
      ::GetThemeMetric(kThemeMetricDisclosureButtonWidth, &twistyWidth);
      ::GetThemeMetric(kThemeMetricDisclosureButtonHeight, &twistyHeight);
      aResult->SizeTo(twistyWidth, twistyHeight);
      *aIsOverridable = PR_FALSE;
      break;
    }
    
    case NS_THEME_TREEVIEW_HEADER:
    case NS_THEME_TREEVIEW_HEADER_CELL:
    {
      SInt32 headerHeight = 0;
      ::GetThemeMetric(kThemeMetricListHeaderHeight, &headerHeight);
      aResult->SizeTo(0, headerHeight);
      break;
    }
      
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:
    case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    {
      // yeah, i know i'm cheating a little here, but i figure that it
      // really doesn't matter if the scrollbar is vertical or horizontal
      // and the width metric is a really good metric for every piece
      // of the scrollbar.
      SInt32 scrollbarWidth = 0;
      ::GetThemeMetric(kThemeMetricScrollBarWidth, &scrollbarWidth);
      aResult->SizeTo(scrollbarWidth, scrollbarWidth);
      *aIsOverridable = PR_FALSE;
      break;
    }

  }
#endif

  return NS_OK;
}


NS_IMETHODIMP
nsNativeThemeMac::WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                     nsIAtom* aAttribute, PRBool* aShouldRepaint)
{
  // Some widget types just never change state.
  switch ( aWidgetType ) {
    case NS_THEME_TOOLBOX:
    case NS_THEME_TOOLBAR:
    case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL: 
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_TOOLTIP:
    case NS_THEME_TAB_PANELS:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_DIALOG:
    case NS_THEME_MENUPOPUP:
      *aShouldRepaint = PR_FALSE;
      return NS_OK;
  }

  // XXXdwh Not sure what can really be done here.  Can at least guess for
  // specific widgets that they're highly unlikely to have certain states.
  // For example, a toolbar doesn't care about any states.
  if (!aAttribute) {
    // Hover/focus/active changed.  Always repaint.
    *aShouldRepaint = PR_TRUE;
  }
  else {
    // Check the attribute to see if it's relevant.  
    // disabled, checked, dlgtype, default, etc.
    *aShouldRepaint = PR_FALSE;
    if (aAttribute == mDisabledAtom || aAttribute == mCheckedAtom ||
        aAttribute == mSelectedAtom)
      *aShouldRepaint = PR_TRUE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNativeThemeMac::ThemeChanged()
{
  // what do we do here?
  return NS_OK;
}


PRBool 
nsNativeThemeMac::ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame,
                                      PRUint8 aWidgetType)
{
#ifndef MOZ_WIDGET_COCOA
  // Only support HTML widgets for Cocoa
  if (aFrame && aFrame->GetContent()->IsContentOfType(nsIContent::eHTML))
    return PR_FALSE;
#endif

  if (aPresContext && !aPresContext->PresShell()->IsThemeSupportEnabled())
    return PR_FALSE;

  PRBool retVal = PR_FALSE;
  
  switch ( aWidgetType ) {
    case NS_THEME_DIALOG:
    case NS_THEME_WINDOW:
      //    case NS_THEME_MENUPOPUP:     // no support for painting menu backgrounds
    case NS_THEME_TOOLTIP:
    
    case NS_THEME_CHECKBOX:
    case NS_THEME_CHECKBOX_SMALL:
    case NS_THEME_CHECKBOX_CONTAINER:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_SMALL:
    case NS_THEME_RADIO_CONTAINER:
    case NS_THEME_BUTTON:
    case NS_THEME_BUTTON_SMALL:
    case NS_THEME_BUTTON_BEVEL:
    case NS_THEME_TOOLBAR:
    case NS_THEME_STATUSBAR:
    case NS_THEME_TEXTFIELD:
    //case NS_THEME_TOOLBOX:
    //case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TOOLBAR_SEPARATOR:
    
    case NS_THEME_TAB_PANELS:
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
    
    case NS_THEME_TREEVIEW_TWISTY:
    case NS_THEME_TREEVIEW_TWISTY_OPEN:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TREEVIEW_HEADER:
    case NS_THEME_TREEVIEW_HEADER_CELL:
    case NS_THEME_TREEVIEW_HEADER_SORTARROW:
    case NS_THEME_TREEVIEW_TREEITEM:
    case NS_THEME_TREEVIEW_LINE:
    
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:
    case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
      retVal = PR_TRUE;
      break;
  
    case NS_THEME_LISTBOX:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_DROPDOWN_TEXT:
      // Support listboxes and dropdowns regardless of styling,
      // since non-themed ones look totally wrong.
      return PR_TRUE;
  }

  return retVal ? !IsWidgetStyled(aPresContext, aFrame, aWidgetType) : PR_FALSE;
}


PRBool
nsNativeThemeMac::WidgetIsContainer(PRUint8 aWidgetType)
{
  // XXXdwh At some point flesh all of this out.
  switch ( aWidgetType ) {
   case NS_THEME_DROPDOWN_BUTTON:
   case NS_THEME_RADIO:
   case NS_THEME_CHECKBOX:
   case NS_THEME_PROGRESSBAR:
    return PR_FALSE;
    break;
  }
  return PR_TRUE;
}
