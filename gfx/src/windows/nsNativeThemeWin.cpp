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
 * David Hyatt (hyatt@netscape.com).
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Hill (tim@prismelite.com)
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

#include <windows.h>
#include "nsNativeThemeWin.h"
#include "nsRenderingContextWin.h"
#include "nsDeviceContextWin.h"
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
#include "nsILookAndFeel.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIMenuFrame.h"
#include "nsUnicharUtils.h"
#include <malloc.h>

#define THEME_COLOR 204
#define THEME_FONT  210

// Generic state constants
#define TS_NORMAL    1
#define TS_HOVER     2
#define TS_ACTIVE    3
#define TS_DISABLED  4
#define TS_FOCUSED   5

// Button constants
#define BP_BUTTON    1
#define BP_RADIO     2
#define BP_CHECKBOX  3

// Textfield constants
#define TFP_TEXTFIELD 1
#define TFS_READONLY  6

// Treeview/listbox constants
#define TREEVIEW_BODY 1

// Scrollbar constants
#define SP_BUTTON          1
#define SP_THUMBHOR        2
#define SP_THUMBVERT       3
#define SP_TRACKSTARTHOR   4
#define SP_TRACKENDHOR     5
#define SP_TRACKSTARTVERT  6
#define SP_TRACKENDVERT    7
#define SP_GRIPPERHOR      8
#define SP_GRIPPERVERT     9

// Progress bar constants
#define PP_BAR             1
#define PP_BARVERT         2
#define PP_CHUNK           3
#define PP_CHUNKVERT       4

// Tab constants
#define TABP_TAB             4
#define TABP_TAB_SELECTED    5
#define TABP_PANELS          9
#define TABP_PANEL           10

// Tooltip constants
#define TTP_STANDARD         1

// Dropdown constants
#define CBP_DROPMARKER       1

NS_IMPL_ISUPPORTS1(nsNativeThemeWin, nsITheme)

typedef HANDLE (WINAPI*OpenThemeDataPtr)(HWND hwnd, LPCWSTR pszClassList);
typedef HRESULT (WINAPI*CloseThemeDataPtr)(HANDLE hTheme);
typedef HRESULT (WINAPI*DrawThemeBackgroundPtr)(HANDLE hTheme, HDC hdc, int iPartId, 
                                          int iStateId, const RECT *pRect,
                                          const RECT* pClipRect);
typedef HRESULT (WINAPI*GetThemeContentRectPtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                          int iStateId, const RECT* pRect,
                                          RECT* pContentRect);
typedef HRESULT (WINAPI*GetThemePartSizePtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                       int iStateId, RECT* prc, int ts,
                                       SIZE* psz);
typedef HRESULT (WINAPI*GetThemeSysFontPtr)(HANDLE hTheme, int iFontId, OUT LOGFONT* pFont);
typedef HRESULT (WINAPI*GetThemeColorPtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                   int iStateId, int iPropId, OUT COLORREF* pFont);

static OpenThemeDataPtr openTheme = NULL;
static CloseThemeDataPtr closeTheme = NULL;
static DrawThemeBackgroundPtr drawThemeBG = NULL;
static GetThemeContentRectPtr getThemeContentRect = NULL;
static GetThemePartSizePtr getThemePartSize = NULL;
static GetThemeSysFontPtr getThemeSysFont = NULL;
static GetThemeColorPtr getThemeColor = NULL;

static const char kThemeLibraryName[] = "uxtheme.dll";

nsNativeThemeWin::nsNativeThemeWin() {
  mThemeDLL = NULL;
  mButtonTheme = NULL;
  mTextFieldTheme = NULL;
  mTooltipTheme = NULL;
  mToolbarTheme = NULL;
  mRebarTheme = NULL;
  mProgressTheme = NULL;
  mScrollbarTheme = NULL;
  mStatusbarTheme = NULL;
  mTabTheme = NULL;
  mTreeViewTheme = NULL;
  mComboBoxTheme = NULL;
  mHeaderTheme = NULL;

  mThemeDLL = ::LoadLibrary(kThemeLibraryName);
  if (mThemeDLL) {
    openTheme = (OpenThemeDataPtr)GetProcAddress(mThemeDLL, "OpenThemeData");
    closeTheme = (CloseThemeDataPtr)GetProcAddress(mThemeDLL, "CloseThemeData");
    drawThemeBG = (DrawThemeBackgroundPtr)GetProcAddress(mThemeDLL, "DrawThemeBackground");
    getThemeContentRect = (GetThemeContentRectPtr)GetProcAddress(mThemeDLL, "GetThemeBackgroundContentRect");
    getThemePartSize = (GetThemePartSizePtr)GetProcAddress(mThemeDLL, "GetThemePartSize");
    getThemeSysFont = (GetThemeSysFontPtr)GetProcAddress(mThemeDLL, "GetThemeSysFont");
    getThemeColor = (GetThemeColorPtr)GetProcAddress(mThemeDLL, "GetThemeColor");
  }

  mInputAtom = do_GetAtom("input");
  mInputCheckedAtom = do_GetAtom("_moz-input-checked");
  mTypeAtom = do_GetAtom("type");

  // If there is a relevant change in platform-forms.css for windows platform,
  // static widget style variables (e.g. sButtonBorderSize) should be 
  // reinitialized here.
}

nsNativeThemeWin::~nsNativeThemeWin() {
  if (!mThemeDLL)
    return;

  CloseData();
  
  if (mThemeDLL)
    ::FreeLibrary(mThemeDLL);
}

static void GetNativeRect(const nsRect& aSrc, RECT& aDst) 
{
  aDst.top = aSrc.y;
  aDst.bottom = aSrc.y + aSrc.height;
  aDst.left = aSrc.x;
  aDst.right = aSrc.x + aSrc.width;
}

HANDLE
nsNativeThemeWin::GetTheme(PRUint8 aWidgetType)
{ 
  if (!mThemeDLL)
    return NULL;

  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX: {
      if (!mButtonTheme)
        mButtonTheme = openTheme(NULL, L"Button");
      return mButtonTheme;
    }
    case NS_THEME_TEXTFIELD:
    case NS_THEME_DROPDOWN: {
      if (!mTextFieldTheme)
        mTextFieldTheme = openTheme(NULL, L"Edit");
      return mTextFieldTheme;
    }
    case NS_THEME_TOOLTIP: {
      if (!mTooltipTheme)
        mTooltipTheme = openTheme(NULL, L"Tooltip");
      return mTooltipTheme;
    }
    case NS_THEME_TOOLBOX: {
      if (!mRebarTheme)
        mRebarTheme = openTheme(NULL, L"Rebar");
      return mRebarTheme;
    }
    case NS_THEME_TOOLBAR:
    case NS_THEME_TOOLBAR_BUTTON: {
      if (!mToolbarTheme)
        mToolbarTheme = openTheme(NULL, L"Toolbar");
      return mToolbarTheme;
    }
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL: {
      if (!mProgressTheme)
        mProgressTheme = openTheme(NULL, L"Progress");
      return mProgressTheme;
    }
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS: {
      if (!mTabTheme)
        mTabTheme = openTheme(NULL, L"Tab");
      return mTabTheme;
    }
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL:
    case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:
    {
      if (!mScrollbarTheme)
        mScrollbarTheme = openTheme(NULL, L"Scrollbar");
      return mScrollbarTheme;
    }
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_RESIZER:
    {
      if (!mStatusbarTheme)
        mStatusbarTheme = openTheme(NULL, L"Status");
      return mStatusbarTheme;
    }
    case NS_THEME_DROPDOWN_BUTTON: {
      if (!mComboBoxTheme)
        mComboBoxTheme = openTheme(NULL, L"Combobox");
      return mComboBoxTheme;
    }
    case NS_THEME_TREEVIEW_HEADER_CELL:
    case NS_THEME_TREEVIEW_HEADER_SORTARROW: {
      if (!mHeaderTheme)
        mHeaderTheme = openTheme(NULL, L"Header");
      return mHeaderTheme;
    }
    case NS_THEME_LISTBOX:
    case NS_THEME_LISTBOX_LISTITEM:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TREEVIEW_TWISTY_OPEN:
    case NS_THEME_TREEVIEW_TREEITEM: {
      if (!mTreeViewTheme)
        mTreeViewTheme = openTheme(NULL, L"Listview");
      return mTreeViewTheme;
    }
  }
  return NULL;
}

nsresult 
nsNativeThemeWin::GetThemePartAndState(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                       PRInt32& aPart, PRInt32& aState)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON: {
      aPart = BP_BUTTON;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      if (IsDisabled(aFrame)) {
        aState = TS_DISABLED;
        return NS_OK;
      }
      PRInt32 eventState = GetContentState(aFrame, aWidgetType);
      if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
        aState = TS_ACTIVE;
      else if (eventState & NS_EVENT_STATE_FOCUS)
        aState = TS_FOCUSED;
      else if (eventState & NS_EVENT_STATE_HOVER)
        aState = TS_HOVER;
      else 
        aState = TS_NORMAL;
      
      // Check for default dialog buttons.  These buttons should always look
      // focused.
      if (aState == TS_NORMAL && IsDefaultButton(aFrame))
        aState = TS_FOCUSED;
      return NS_OK;
    }
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO: {
      aPart = (aWidgetType == NS_THEME_CHECKBOX) ? BP_CHECKBOX : BP_RADIO; 

      // XXXdwh This check will need to be more complicated, since HTML radio groups
      // use checked, but XUL radio groups use selected.  There will need to be an
      // IsContentOfType test for HTML vs. XUL here.
      nsIAtom* atom = (aWidgetType == NS_THEME_CHECKBOX) ? mCheckedAtom : mSelectedAtom;

      PRBool isHTML = PR_FALSE;
      PRBool isHTMLChecked = PR_FALSE;
      PRBool isXULCheckboxRadio = PR_FALSE;
      
      if (!aFrame)
        aState = TS_NORMAL;
      else {
        // For XUL checkboxes and radio buttons, the state of the parent
        // determines our state.
        nsIContent* content = aFrame->GetContent();
        PRBool isXULCheckboxRadio = content->IsContentOfType(nsIContent::eXUL);
        if (!isXULCheckboxRadio) {
          // Attempt a QI.
          nsCOMPtr<nsIDOMHTMLInputElement> inputElt(do_QueryInterface(content));
          if (inputElt) {
            inputElt->GetChecked(&isHTMLChecked);
            isHTML = PR_TRUE;
          }
        }

        if (IsDisabled(isXULCheckboxRadio ? aFrame->GetParent(): aFrame))
          aState = TS_DISABLED;
        else {
          PRInt32 eventState = GetContentState(aFrame, aWidgetType);
          if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
            aState = TS_ACTIVE;
          else if (eventState & NS_EVENT_STATE_HOVER)
            aState = TS_HOVER;
          else 
            aState = TS_NORMAL;
        }
      }

      if (isHTML) {
        if (isHTMLChecked)
          aState += 4;
      }
      else if (aWidgetType == NS_THEME_CHECKBOX ?
               IsChecked(aFrame) : IsSelected(aFrame))
        aState += 4; // 4 unchecked states, 4 checked states.
      return NS_OK;
    }
    case NS_THEME_TEXTFIELD:
    case NS_THEME_DROPDOWN: {
      aPart = TFP_TEXTFIELD;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      if (IsDisabled(aFrame)) {
        aState = TS_DISABLED;
        return NS_OK;
      }

      if (IsReadOnly(aFrame)) {
        aState = TFS_READONLY;
        return NS_OK;
      }

      PRInt32 eventState = GetContentState(aFrame, aWidgetType);
      if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
        aState = TS_ACTIVE;
      else if (eventState & NS_EVENT_STATE_FOCUS)
        aState = TS_FOCUSED;
      else if (eventState & NS_EVENT_STATE_HOVER)
        aState = TS_HOVER;
      else 
        aState = TS_NORMAL;
      
      return NS_OK;
    }
    case NS_THEME_TOOLTIP: {
      aPart = TTP_STANDARD;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_PROGRESSBAR: {
      aPart = PP_BAR;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_PROGRESSBAR_CHUNK: {
      aPart = PP_CHUNK;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_PROGRESSBAR_VERTICAL: {
      aPart = PP_BARVERT;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL: {
      aPart = PP_CHUNKVERT;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TOOLBAR_BUTTON: {
      aPart = BP_BUTTON;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      if (IsDisabled(aFrame)) {
        aState = TS_DISABLED;
        return NS_OK;
      }
      PRInt32 eventState = GetContentState(aFrame, aWidgetType);
      if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
        aState = TS_ACTIVE;
      else if (eventState & NS_EVENT_STATE_HOVER)
        aState = TS_HOVER;
      else 
        aState = TS_NORMAL;
      
      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT: {
      aPart = SP_BUTTON;
      aState = (aWidgetType - NS_THEME_SCROLLBAR_BUTTON_UP)*4;
      if (!aFrame)
        aState += TS_NORMAL;
      else if (IsDisabled(aFrame))
        aState += TS_DISABLED;
      else {
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
        if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
          aState += TS_ACTIVE;
        else if (eventState & NS_EVENT_STATE_HOVER)
          aState += TS_HOVER;
        else
          aState += TS_NORMAL;
      }
      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL: {
      aPart = (aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL) ?
              SP_TRACKSTARTHOR : SP_TRACKSTARTVERT;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL: {
      aPart = (aWidgetType == NS_THEME_SCROLLBAR_THUMB_HORIZONTAL) ?
              SP_THUMBHOR : SP_THUMBVERT;
      if (!aFrame)
        aState = TS_NORMAL;
      else if (IsDisabled(aFrame))
        aState = TS_DISABLED;
      else {
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
        if (eventState & NS_EVENT_STATE_ACTIVE) // Hover is not also a requirement for
                                                // the thumb, since the drag is not canceled
                                                // when you move outside the thumb.
          aState = TS_ACTIVE;
        else if (eventState & NS_EVENT_STATE_HOVER)
          aState = TS_HOVER;
        else 
          aState = TS_NORMAL;
      }
      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL:
    case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL: {
      aPart = (aWidgetType == NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL) ?
              SP_GRIPPERHOR : SP_GRIPPERVERT;
      if (!aFrame)
        aState = TS_NORMAL;
      else if (IsDisabled(aFrame))
        aState = TS_DISABLED;
      else {
        // XXXdwh The gripper needs to get a hover attribute set on it, since it
        // never goes into :hover.
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
        if (eventState & NS_EVENT_STATE_ACTIVE) // Hover is not also a requirement for
                                                // the gripper, since the drag is not canceled
                                                // when you move outside the gripper.
          aState = TS_ACTIVE;
        else if (eventState & NS_EVENT_STATE_HOVER)
          aState = TS_HOVER;
        else 
          aState = TS_NORMAL;
      }
      return NS_OK;
    }
    case NS_THEME_TOOLBOX:
    case NS_THEME_STATUSBAR:
    case NS_THEME_SCROLLBAR: {
      aPart = aState = 0;
      return NS_OK; // These have no part or state.
    }
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_RESIZER: {
      aPart = (aWidgetType - NS_THEME_STATUSBAR_PANEL) + 1;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TREEVIEW:
    case NS_THEME_LISTBOX: {
      aPart = TREEVIEW_BODY;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TAB_PANELS: {
      aPart = TABP_PANELS;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TAB_PANEL: {
      aPart = TABP_PANEL;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE: {
      aPart = TABP_TAB;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }
      
      if (IsDisabled(aFrame)) {
        aState = TS_DISABLED;
        return NS_OK;
      }

      if (IsSelectedTab(aFrame)) {
        aPart = TABP_TAB_SELECTED;
        aState = TS_ACTIVE; // The selected tab is always "pressed".
      }
      else {
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
        if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
          aState = TS_ACTIVE;
        else if (eventState & NS_EVENT_STATE_FOCUS)
          aState = TS_FOCUSED;
        else if (eventState & NS_EVENT_STATE_HOVER)
          aState = TS_HOVER;
        else 
          aState = TS_NORMAL;
      }
      
      return NS_OK;
    }
    case NS_THEME_TREEVIEW_HEADER_SORTARROW: {
      // XXX Probably will never work due to a bug in the Luna theme.
      aPart = 4;
      aState = 1;
      return NS_OK;
    }
    case NS_THEME_TREEVIEW_HEADER_CELL: {
      aPart = 1;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }
      
      PRInt32 eventState = GetContentState(aFrame, aWidgetType);
      if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
        aState = TS_ACTIVE;
      else if (eventState & NS_EVENT_STATE_FOCUS)
        aState = TS_FOCUSED;
      else if (eventState & NS_EVENT_STATE_HOVER)
        aState = TS_HOVER;
      else 
        aState = TS_NORMAL;
      
      return NS_OK;
    }
    case NS_THEME_DROPDOWN_BUTTON: {
      aPart = CBP_DROPMARKER;

      nsIContent* content = aFrame->GetContent();

      nsIFrame* parentFrame = aFrame->GetParent();
      nsCOMPtr<nsIMenuFrame> menuFrame(do_QueryInterface(parentFrame));
      if (menuFrame || (content && content->IsContentOfType(nsIContent::eHTML)) )
         // XUL menu lists and HTML selects get state from parent         
         aFrame = parentFrame;

      if (IsDisabled(aFrame))
        aState = TS_DISABLED;
      else {     
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
        if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
          aState = TS_ACTIVE;
        else if (eventState & NS_EVENT_STATE_HOVER)
          aState = TS_HOVER;
        else 
          aState = TS_NORMAL;
      }

      return NS_OK;
    }
  }

  aPart = 0;
  aState = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNativeThemeWin::DrawWidgetBackground(nsIRenderingContext* aContext,
                                       nsIFrame* aFrame,
                                       PRUint8 aWidgetType,
                                       const nsRect& aRect,
                                       const nsRect& aClipRect)
{
  HANDLE theme = GetTheme(aWidgetType);
  if (!theme)
    return ClassicDrawWidgetBackground(aContext, aFrame, aWidgetType, aRect, aClipRect); 

  if (!drawThemeBG)
    return NS_ERROR_FAILURE;    

  PRInt32 part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return rv;

  nsTransform2D* transformMatrix;
  aContext->GetCurrentTransform(transformMatrix);
  RECT widgetRect;
  RECT clipRect;
	nsRect tr(aRect);
  nsRect cr(aClipRect);
	transformMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
  GetNativeRect(tr, widgetRect);
  transformMatrix->TransformCoord(&cr.x,&cr.y,&cr.width,&cr.height);
  GetNativeRect(cr, clipRect);
  HDC hdc = NS_STATIC_CAST(nsRenderingContextWin*, aContext)->mDC;
  if (!hdc)
    return NS_ERROR_FAILURE;
  
  // For left edge and right edge tabs, we need to adjust the widget
  // rects and clip rects so that the edges don't get drawn.
  if (aWidgetType == NS_THEME_TAB_LEFT_EDGE || aWidgetType == NS_THEME_TAB_RIGHT_EDGE) {
    // HACK ALERT: There appears to be no way to really obtain this value, so we're forced
    // to just use the default value for Luna (which also happens to be correct for
    // all the other skins I've tried).
    PRInt32 edgeSize = 2;
    
    // Armed with the size of the edge, we now need to either shift to the left or to the
    // right.  The clip rect won't include this extra area, so we know that we're
    // effectively shifting the edge out of view (such that it won't be painted).
    if (aWidgetType == NS_THEME_TAB_LEFT_EDGE)
      // The right edge should not be drawn.  Extend our rect by the edge size.
      widgetRect.right += edgeSize;
    else
      // The left edge should not be drawn.  Move the widget rect's left coord back.
      widgetRect.left -= edgeSize;
  }
  
  drawThemeBG(theme, hdc, part, state, &widgetRect, &clipRect);

  // Draw focus rectangles for XP HTML checkboxes and radio buttons
  // XXX it'd be nice to draw these outside of the frame
  if ((aWidgetType == NS_THEME_CHECKBOX || aWidgetType == NS_THEME_RADIO)
      && aFrame->GetContent()->IsContentOfType(nsIContent::eHTML)) {
      PRInt32 contentState ;
      contentState = GetContentState(aFrame, aWidgetType);  
            
      if (contentState & NS_EVENT_STATE_FOCUS) {
        // setup DC to make DrawFocusRect draw correctly
        ::SetBrushOrgEx(hdc, widgetRect.left, widgetRect.top, NULL);
        PRInt32 oldColor;
        oldColor = ::SetTextColor(hdc, 0);
        // draw focus rectangle
        ::DrawFocusRect(hdc, &widgetRect);
        ::SetTextColor(hdc, oldColor);
      }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeWin::GetWidgetBorder(nsIDeviceContext* aContext, 
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsMargin* aResult)
{
  HANDLE theme = GetTheme(aWidgetType);
  if (!theme)
    return ClassicGetWidgetBorder(aContext, aFrame, aWidgetType, aResult); 

  (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right = 0;

  if (!WidgetIsContainer(aWidgetType) ||
      aWidgetType == NS_THEME_TOOLBOX || aWidgetType == NS_THEME_TOOLBAR || 
      aWidgetType == NS_THEME_STATUSBAR || 
      aWidgetType == NS_THEME_RESIZER || aWidgetType == NS_THEME_TAB_PANEL)
    return NS_OK; // Don't worry about it.
 
  if (!getThemeContentRect)
    return NS_ERROR_FAILURE;

  PRInt32 part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return rv;

  // Get our info.
  RECT outerRect; // Create a fake outer rect.
  outerRect.top = outerRect.left = 100;
  outerRect.right = outerRect.bottom = 200;
  RECT contentRect(outerRect);
  HRESULT res = getThemeContentRect(theme, NULL, part, state, &outerRect, &contentRect);
  
  if (FAILED(res))
    return NS_ERROR_FAILURE;

  // Now compute the delta in each direction and place it in our
  // nsMargin struct.
  aResult->top = contentRect.top - outerRect.top;
  aResult->bottom = outerRect.bottom - contentRect.bottom;
  aResult->left = contentRect.left - outerRect.left;
  aResult->right = outerRect.right - contentRect.right;

  // Remove the edges for tabs that are before or after the selected tab,
  if (aWidgetType == NS_THEME_TAB_LEFT_EDGE)
    // Remove the right edge, since we won't be drawing it.
    aResult->right = 0;
  else if (aWidgetType == NS_THEME_TAB_RIGHT_EDGE)
    // Remove the left edge, since we won't be drawing it.
    aResult->left = 0;

  if (aFrame && aWidgetType == NS_THEME_TEXTFIELD) {
    nsIContent* content = aFrame->GetContent();
    if (content && content->IsContentOfType(nsIContent::eHTML)) {
      // We need to pad textfields by 1 pixel, since the caret will draw
      // flush against the edge by default if we don't.
      aResult->left++;
      aResult->right++;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeWin::GetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                       PRUint8 aWidgetType,
                                       nsSize* aResult, PRBool* aIsOverridable)
{
  (*aResult).width = (*aResult).height = 0;
  *aIsOverridable = PR_TRUE;

  HANDLE theme = GetTheme(aWidgetType);
  if (!theme)
    return ClassicGetMinimumWidgetSize(aContext, aFrame, aWidgetType, aResult, aIsOverridable);
 
  if (aWidgetType == NS_THEME_TOOLBOX || aWidgetType == NS_THEME_TOOLBAR || 
      aWidgetType == NS_THEME_STATUSBAR || aWidgetType == NS_THEME_PROGRESSBAR_CHUNK ||
      aWidgetType == NS_THEME_PROGRESSBAR_CHUNK_VERTICAL ||
      aWidgetType == NS_THEME_TAB_PANELS || aWidgetType == NS_THEME_TAB_PANEL ||
      aWidgetType == NS_THEME_LISTBOX || aWidgetType == NS_THEME_TREEVIEW)
    return NS_OK; // Don't worry about it.

  if (!getThemePartSize)
    return NS_ERROR_FAILURE;
  
  // Call GetSystemMetrics to determine size for WinXP scrollbars
  // (GetThemeSysSize API returns the optimal size for the theme, but 
  //  Windows appears to always use metrics when drawing standard scrollbars)
  switch (aWidgetType) {
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_DROPDOWN_BUTTON: 
      return ClassicGetMinimumWidgetSize(aContext, aFrame, aWidgetType, aResult, aIsOverridable);
  }

  PRInt32 part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return rv;

  HDC hdc = ((nsRenderingContextWin*)aContext)->mDC;
  if (!hdc)
    return NS_ERROR_FAILURE;

  PRInt32 sizeReq = 1; // Best-fit size.
  if (aWidgetType == NS_THEME_PROGRESSBAR ||
      aWidgetType == NS_THEME_PROGRESSBAR_VERTICAL)
    sizeReq = 0; // Best-fit size for progress meters is too large for most 
                 // themes.
                 // In our app, we want these widgets to be able to really shrink down,
                 // so use the min-size request value (of 0).
  
  SIZE sz;
  getThemePartSize(theme, hdc, part, state, NULL, sizeReq, &sz);
  aResult->width = sz.cx;
  aResult->height = sz.cy;

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeWin::WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                     nsIAtom* aAttribute, PRBool* aShouldRepaint)
{
  // Some widget types just never change state.
  if (aWidgetType == NS_THEME_TOOLBOX || aWidgetType == NS_THEME_TOOLBAR ||
      aWidgetType == NS_THEME_SCROLLBAR_TRACK_VERTICAL || 
      aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL || 
      aWidgetType == NS_THEME_STATUSBAR || aWidgetType == NS_THEME_STATUSBAR_PANEL ||
      aWidgetType == NS_THEME_STATUSBAR_RESIZER_PANEL ||
      aWidgetType == NS_THEME_PROGRESSBAR_CHUNK ||
      aWidgetType == NS_THEME_PROGRESSBAR_CHUNK_VERTICAL ||
      aWidgetType == NS_THEME_PROGRESSBAR ||
      aWidgetType == NS_THEME_PROGRESSBAR_VERTICAL ||
      aWidgetType == NS_THEME_TOOLTIP ||
      aWidgetType == NS_THEME_TAB_PANELS ||
      aWidgetType == NS_THEME_TAB_PANEL) {
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
        aAttribute == mSelectedAtom || aAttribute == mReadOnlyAtom)
      *aShouldRepaint = PR_TRUE;
  }

  return NS_OK;
}

void
nsNativeThemeWin::CloseData()
{
  if (mToolbarTheme) {
    closeTheme(mToolbarTheme);
    mToolbarTheme = NULL;
  }
  if (mScrollbarTheme) {
    closeTheme(mScrollbarTheme);
    mScrollbarTheme = NULL;
  }
  if (mRebarTheme) {
    closeTheme(mRebarTheme);
    mRebarTheme = NULL;
  }
  if (mProgressTheme) {
    closeTheme(mProgressTheme);
    mProgressTheme = NULL;
  }
  if (mButtonTheme) {
    closeTheme(mButtonTheme);
    mButtonTheme = NULL;
  }
  if (mTextFieldTheme) {
    closeTheme(mTextFieldTheme);
    mTextFieldTheme = NULL;
  }
  if (mTooltipTheme) {
    closeTheme(mTooltipTheme);
    mTooltipTheme = NULL;
  }
  if (mStatusbarTheme) {
    closeTheme(mStatusbarTheme);
    mStatusbarTheme = NULL;
  }
  if (mTabTheme) {
    closeTheme(mTabTheme);
    mTabTheme = NULL;
  }
  if (mTreeViewTheme) {
    closeTheme(mTreeViewTheme);
    mTreeViewTheme = NULL;
  }
  if (mComboBoxTheme) {
    closeTheme(mComboBoxTheme);
    mComboBoxTheme = NULL;
  }
  if (mHeaderTheme) {
    closeTheme(mHeaderTheme);
    mHeaderTheme = NULL;
  }
}

NS_IMETHODIMP
nsNativeThemeWin::ThemeChanged()
{
  CloseData();
  return NS_OK;
}

PRBool 
nsNativeThemeWin::ThemeSupportsWidget(nsPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      PRUint8 aWidgetType)
{
  // XXXdwh We can go even further and call the API to ask if support exists for
  // specific widgets.

  if (aPresContext && !aPresContext->PresShell()->IsThemeSupportEnabled())
    return PR_FALSE;

  HANDLE theme = NULL;
  if (aWidgetType == NS_THEME_CHECKBOX_CONTAINER)
    theme = GetTheme(NS_THEME_CHECKBOX);
  else if (aWidgetType == NS_THEME_RADIO_CONTAINER)
    theme = GetTheme(NS_THEME_RADIO);
  else
    theme = GetTheme(aWidgetType);

  if ((theme) || (!theme && ClassicThemeSupportsWidget(aPresContext, aFrame, aWidgetType)))
    // turn off theming for some HTML widgets styled by the page
    return (!IsWidgetStyled(aPresContext, aFrame, aWidgetType));
  
  return PR_FALSE;
}

PRBool 
nsNativeThemeWin::WidgetIsContainer(PRUint8 aWidgetType)
{
  // XXXdwh At some point flesh all of this out.
  if (aWidgetType == NS_THEME_DROPDOWN_BUTTON || 
      aWidgetType == NS_THEME_RADIO ||
      aWidgetType == NS_THEME_CHECKBOX)
    return PR_FALSE;
  return PR_TRUE;
}


/* Windows 9x/NT/2000/Classic XP Theme Support */

PRBool 
nsNativeThemeWin::ClassicThemeSupportsWidget(nsPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      PRUint8 aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_DROPDOWN:
    case NS_THEME_TOOLTIP:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_RESIZER:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS:
      return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult
nsNativeThemeWin::ClassicGetWidgetBorder(nsIDeviceContext* aContext, 
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsMargin* aResult)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON: {
      const nsStyleUserInterface *uiData = aFrame->GetStyleUserInterface();
      if (uiData->mUserFocus == NS_STYLE_USER_FOCUS_IGNORE) {
        // use different padding for non-focusable buttons
        (*aResult).top = (*aResult).left = 1;
        (*aResult).bottom = (*aResult).right = 2;
      }
      else
        (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 3; 
      break;  
    }
    case NS_THEME_STATUSBAR:
      (*aResult).bottom = (*aResult).left = (*aResult).right = 0;
      (*aResult).top = 2;
      break;
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 2;
      break;
    case NS_THEME_TEXTFIELD: {
      (*aResult).top = (*aResult).bottom = 2;
      nsIContent* content = aFrame->GetContent();
      if (content && content->IsContentOfType(nsIContent::eHTML))
        // HTML text-fields need extra padding
        (*aResult).left = (*aResult).right = 3;
      else
        (*aResult).left = (*aResult).right = 2;
      break;
    }
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL: {
      (*aResult).top = 1;      
      (*aResult).left = 1;
      (*aResult).bottom = 1;
      (*aResult).right = aFrame->GetNextSibling() ? 3 : 1;
      break;
    }    
    case NS_THEME_TOOLTIP:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 1;
      break;
    default:
      (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right = 0;
      break;
  }
  return NS_OK;
}

nsresult
nsNativeThemeWin::ClassicGetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                       PRUint8 aWidgetType,
                                       nsSize* aResult, PRBool* aIsOverridable)
{
  (*aResult).width = (*aResult).height = 0;
  *aIsOverridable = PR_TRUE;
  switch (aWidgetType) {
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX: 
      (*aResult).width = (*aResult).height = 13;
      break;
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
      (*aResult).width = ::GetSystemMetrics(SM_CXVSCROLL);
      (*aResult).height = ::GetSystemMetrics(SM_CYVSCROLL);
      *aIsOverridable = PR_FALSE;
      break;
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
      (*aResult).width = ::GetSystemMetrics(SM_CXHSCROLL);
      (*aResult).height = ::GetSystemMetrics(SM_CYHSCROLL);
      *aIsOverridable = PR_FALSE;
      break;
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:        
      (*aResult).width = ::GetSystemMetrics(SM_CYVTHUMB);
      (*aResult).height = (*aResult).width >> 1;
      *aIsOverridable = PR_FALSE;
      break;
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
      (*aResult).height = ::GetSystemMetrics(SM_CXHTHUMB);
      (*aResult).width = (*aResult).height >> 1;
      *aIsOverridable = PR_FALSE;
      break;
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
      // XXX HACK We should be able to have a minimum height for the scrollbar
      // track.  However, this causes problems when uncollapsing a scrollbar
      // inside a tree.  See bug 201379 for details.

        //      (*aResult).height = ::GetSystemMetrics(SM_CYVTHUMB) << 1;
      break;
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
      (*aResult).width = ::GetSystemMetrics(SM_CXHTHUMB) << 1;
      break;
    case NS_THEME_DROPDOWN_BUTTON:
      (*aResult).width = ::GetSystemMetrics(SM_CXVSCROLL);
      break;
    case NS_THEME_DROPDOWN:
    case NS_THEME_BUTTON:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TEXTFIELD:          
    case NS_THEME_DROPDOWN_TEXTFIELD:      
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:      
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TOOLTIP:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS:
      // no minimum widget size
      break;
    case NS_THEME_RESIZER: {     
      NONCLIENTMETRICS nc;
      nc.cbSize = sizeof(nc);
      if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(nc), &nc, 0))
        (*aResult).width = (*aResult).height = abs(nc.lfStatusFont.lfHeight) + 4;
      else
        (*aResult).width = (*aResult).height = 15;
      break;
    }
    default:
      return NS_ERROR_FAILURE;
  }  
  return NS_OK;
}


nsresult nsNativeThemeWin::ClassicGetThemePartAndState(nsIFrame* aFrame, PRUint8 aWidgetType,
                                 PRInt32& aPart, PRInt32& aState, PRBool& aFocused)
{  
  switch (aWidgetType) {
    case NS_THEME_BUTTON: {
      PRInt32 contentState;

      aPart = DFC_BUTTON;
      aState = DFCS_BUTTONPUSH;
      aFocused = PR_FALSE;

      contentState = GetContentState(aFrame, aWidgetType);
      if (IsDisabled(aFrame))
        aState |= DFCS_INACTIVE;
      else {
        if (contentState & NS_EVENT_STATE_ACTIVE && contentState & NS_EVENT_STATE_HOVER) {
          aState |= DFCS_PUSHED;
          const nsStyleUserInterface *uiData = aFrame->GetStyleUserInterface();
          // The down state is flat if the button is focusable
          if (uiData->mUserFocus == NS_STYLE_USER_FOCUS_NORMAL) {
            if (!aFrame->GetContent()->IsContentOfType(nsIContent::eHTML))
              aState |= DFCS_FLAT;
            aFocused = PR_TRUE;
          }
        }
        if ((contentState & NS_EVENT_STATE_FOCUS) || 
          (aState == DFCS_BUTTONPUSH && IsDefaultButton(aFrame))) {
          aFocused = PR_TRUE;          
        }

      }

      return NS_OK;
    }
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO: {
      PRInt32 contentState ;
      aFocused = PR_FALSE;

      aPart = DFC_BUTTON;
      aState = (aWidgetType == NS_THEME_CHECKBOX) ? DFCS_BUTTONCHECK : DFCS_BUTTONRADIO;
      nsIContent* content = aFrame->GetContent();
           
      if (content->IsContentOfType(nsIContent::eXUL)) {
        // XUL
        if (aWidgetType == NS_THEME_CHECKBOX) {
          if (IsChecked(aFrame))
            aState |= DFCS_CHECKED;
        }
        else
          if (IsSelected(aFrame))
            aState |= DFCS_CHECKED;
        contentState = GetContentState(aFrame, aWidgetType);
      }
      else {
        // HTML

        nsCOMPtr<nsIDOMHTMLInputElement> inputElt(do_QueryInterface(content));
        if (inputElt) {
          PRBool isChecked = PR_FALSE;
          inputElt->GetChecked(&isChecked);
          if (isChecked)
            aState |= DFCS_CHECKED;
        }
        contentState = GetContentState(aFrame, aWidgetType);
        if (contentState & NS_EVENT_STATE_FOCUS)
          aFocused = PR_TRUE;
      }

      if (IsDisabled(aFrame))
        aState |= DFCS_INACTIVE;
      else if (contentState & NS_EVENT_STATE_ACTIVE && contentState & NS_EVENT_STATE_HOVER)
        aState |= DFCS_PUSHED;
      
      return NS_OK;
    }
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:     
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:      
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TOOLTIP:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS:
      // these don't use DrawFrameControl
      return NS_OK;
    case NS_THEME_DROPDOWN_BUTTON: {

      aPart = DFC_SCROLL;
      aState = DFCS_SCROLLCOMBOBOX;
      
      nsIContent* content = aFrame->GetContent();
      nsIFrame* parentFrame = aFrame->GetParent();
      nsCOMPtr<nsIMenuFrame> menuFrame(do_QueryInterface(parentFrame));
      if (menuFrame || (content && content->IsContentOfType(nsIContent::eHTML)) )
         // XUL menu lists and HTML selects get state from parent         
         aFrame = parentFrame;
         // XXX the button really shouldn't depress when clicking the 
         // parent, but the button frame is never :active for these controls..

      if (IsDisabled(aFrame))
        aState |= DFCS_INACTIVE;
      else {     
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
        if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
        aState |= DFCS_PUSHED | DFCS_FLAT;
      }

      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT: {
      PRInt32 contentState;

      aPart = DFC_SCROLL;
      switch (aWidgetType) {
        case NS_THEME_SCROLLBAR_BUTTON_UP:
          aState = DFCS_SCROLLUP;
          break;
        case NS_THEME_SCROLLBAR_BUTTON_DOWN:
          aState = DFCS_SCROLLDOWN;
          break;
        case NS_THEME_SCROLLBAR_BUTTON_LEFT:
          aState = DFCS_SCROLLLEFT;
          break;
        case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
          aState = DFCS_SCROLLRIGHT;
          break;
      }      
      
      if (IsDisabled(aFrame))
        aState |= DFCS_INACTIVE;
      else {
        contentState = GetContentState(aFrame, aWidgetType);
        if (contentState & NS_EVENT_STATE_HOVER && contentState & NS_EVENT_STATE_ACTIVE)
          aState |= DFCS_PUSHED | DFCS_FLAT;      
      }

      return NS_OK;
    }
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON: {
      PRInt32 contentState;

      aPart = DFC_SCROLL;
      switch (aWidgetType) {
        case NS_THEME_SPINNER_UP_BUTTON:
          aState = DFCS_SCROLLUP;
          break;
        case NS_THEME_SPINNER_DOWN_BUTTON:
          aState = DFCS_SCROLLDOWN;
          break;
      }      
      
      if (IsDisabled(aFrame))
        aState |= DFCS_INACTIVE;
      else {
        contentState = GetContentState(aFrame, aWidgetType);
        if (contentState & NS_EVENT_STATE_HOVER && contentState & NS_EVENT_STATE_ACTIVE)
          aState |= DFCS_PUSHED;
      }

      return NS_OK;    
    }
    case NS_THEME_RESIZER:    
      aPart = DFC_SCROLL;
      aState = DFCS_SCROLLSIZEGRIP;
      return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

// Draw classic Windows tab
// (no system API for this, but DrawEdge can draw all the parts of a tab)
static void DrawTab(HDC hdc, const RECT& R, PRInt32 aPosition, PRBool aSelected,
                    PRBool aDrawLeft, PRBool aDrawRight)
{
  PRInt32 leftFlag, topFlag, rightFlag, lightFlag, shadeFlag;  
  RECT topRect, sideRect, bottomRect, lightRect, shadeRect;
  PRInt32 selectedOffset, lOffset, rOffset;

  selectedOffset = aSelected ? 1 : 0;
  lOffset = aDrawLeft ? 2 : 0;
  rOffset = aDrawRight ? 2 : 0;

  // Get info for tab orientation/position (Left, Top, Right, Bottom)
  switch (aPosition) {
    case BF_LEFT:
      leftFlag = BF_TOP; topFlag = BF_LEFT;
      rightFlag = BF_BOTTOM;
      lightFlag = BF_DIAGONAL_ENDTOPRIGHT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMRIGHT;

      ::SetRect(&topRect, R.left, R.top+lOffset, R.right, R.bottom-rOffset);
      ::SetRect(&sideRect, R.left+2, R.top, R.right-2+selectedOffset, R.bottom);
      ::SetRect(&bottomRect, R.right-2, R.top, R.right, R.bottom);
      ::SetRect(&lightRect, R.left, R.top, R.left+3, R.top+3);
      ::SetRect(&shadeRect, R.left+1, R.bottom-2, R.left+2, R.bottom-1);
      break;
    case BF_TOP:    
      leftFlag = BF_LEFT; topFlag = BF_TOP;
      rightFlag = BF_RIGHT;
      lightFlag = BF_DIAGONAL_ENDTOPRIGHT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMRIGHT;

      ::SetRect(&topRect, R.left+lOffset, R.top, R.right-rOffset, R.bottom);
      ::SetRect(&sideRect, R.left, R.top+2, R.right, R.bottom-1+selectedOffset);
      ::SetRect(&bottomRect, R.left, R.bottom-1, R.right, R.bottom);
      ::SetRect(&lightRect, R.left, R.top, R.left+3, R.top+3);      
      ::SetRect(&shadeRect, R.right-2, R.top+1, R.right-1, R.top+2);      
      break;
    case BF_RIGHT:    
      leftFlag = BF_TOP; topFlag = BF_RIGHT;
      rightFlag = BF_BOTTOM;
      lightFlag = BF_DIAGONAL_ENDTOPLEFT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMLEFT;

      ::SetRect(&topRect, R.left, R.top+lOffset, R.right, R.bottom-rOffset);
      ::SetRect(&sideRect, R.left+2-selectedOffset, R.top, R.right-2, R.bottom);
      ::SetRect(&bottomRect, R.left, R.top, R.left+2, R.bottom);
      ::SetRect(&lightRect, R.right-3, R.top, R.right-1, R.top+2);
      ::SetRect(&shadeRect, R.right-2, R.bottom-3, R.right, R.bottom-1);
      break;
    case BF_BOTTOM:    
      leftFlag = BF_LEFT; topFlag = BF_BOTTOM;
      rightFlag = BF_RIGHT;
      lightFlag = BF_DIAGONAL_ENDTOPLEFT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMLEFT;

      ::SetRect(&topRect, R.left+lOffset, R.top, R.right-rOffset, R.bottom);
      ::SetRect(&sideRect, R.left, R.top+2-selectedOffset, R.right, R.bottom-2);
      ::SetRect(&bottomRect, R.left, R.top, R.right, R.top+2);
      ::SetRect(&lightRect, R.left, R.bottom-3, R.left+2, R.bottom-1);
      ::SetRect(&shadeRect, R.right-2, R.bottom-3, R.right, R.bottom-1);
      break;
  }

  // Background
  ::FillRect(hdc, &R, (HBRUSH) (COLOR_3DFACE+1) );

  // Tab "Top"
  ::DrawEdge(hdc, &topRect, EDGE_RAISED, BF_SOFT | topFlag);

  // Tab "Bottom"
  if (!aSelected)
    ::DrawEdge(hdc, &bottomRect, EDGE_RAISED, BF_SOFT | topFlag);

  // Tab "Sides"
  if (!aDrawLeft)
    leftFlag = 0;
  if (!aDrawRight)
    rightFlag = 0;
  ::DrawEdge(hdc, &sideRect, EDGE_RAISED, BF_SOFT | leftFlag | rightFlag);

  // Tab Diagonal Corners
  if (aDrawLeft)
    ::DrawEdge(hdc, &lightRect, EDGE_RAISED, BF_SOFT | lightFlag);

  if (aDrawRight)
    ::DrawEdge(hdc, &shadeRect, EDGE_RAISED, BF_SOFT | shadeFlag);
}

nsresult nsNativeThemeWin::ClassicDrawWidgetBackground(nsIRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aClipRect)
{         
  PRInt32 part, state;
  PRBool focused;
  nsresult rv;
  rv = ClassicGetThemePartAndState(aFrame, aWidgetType, part, state, focused);
  if (NS_FAILED(rv))
    return rv;

  nsTransform2D* transformMatrix;
  aContext->GetCurrentTransform(transformMatrix);
  RECT widgetRect;
	nsRect tr(aRect);
	transformMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
  GetNativeRect(tr, widgetRect); 

  HDC hdc = NS_STATIC_CAST(nsRenderingContextWin*, aContext)->mDC;

  switch (aWidgetType) { 
    // Draw button
    case NS_THEME_BUTTON: {
      if (focused) {
        // draw dark button focus border first
        HBRUSH brush;        
        brush = ::GetSysColorBrush(COLOR_3DDKSHADOW);
        if (brush)
          ::FrameRect(hdc, &widgetRect, brush);
        InflateRect(&widgetRect, -1, -1);
      }
      // fall-through...
    }
    // Draw controls supported by DrawFrameControl
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_RESIZER: { 
      PRInt32 oldTA;
      // setup DC to make DrawFrameControl draw correctly
      oldTA = ::SetTextAlign(hdc, TA_TOP | TA_LEFT | TA_NOUPDATECP);
      ::DrawFrameControl(hdc, &widgetRect, part, state);
      ::SetTextAlign(hdc, oldTA);

      // Draw focus rectangles for HTML checkboxes and radio buttons
      // XXX it'd be nice to draw these outside of the frame
      if (focused && (aWidgetType == NS_THEME_CHECKBOX || aWidgetType == NS_THEME_RADIO)) {
        // setup DC to make DrawFocusRect draw correctly
        ::SetBrushOrgEx(hdc, widgetRect.left, widgetRect.top, NULL);
        PRInt32 oldColor;
        oldColor = ::SetTextColor(hdc, 0);
        // draw focus rectangle
        ::DrawFocusRect(hdc, &widgetRect);
        ::SetTextColor(hdc, oldColor);
      }

      return NS_OK;
    }
    // Draw controls with 2px 3D inset border
    case NS_THEME_TEXTFIELD:
    case NS_THEME_LISTBOX:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXTFIELD: {
      // Draw inset edge
      ::DrawEdge(hdc, &widgetRect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);

      // Fill in background
      if (IsDisabled(aFrame) ||
          (aFrame->GetContent()->IsContentOfType(nsIContent::eXUL) &&
           IsReadOnly(aFrame)))
        ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_BTNFACE+1));
      else
        ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_WINDOW+1));
      return NS_OK;
    }
    case NS_THEME_TREEVIEW: {
      // Draw inset edge
      ::DrawEdge(hdc, &widgetRect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);

      // Fill in window color background
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_WINDOW+1));
      
      return NS_OK;
    }
    // Draw ToolTip background
    case NS_THEME_TOOLTIP:
      HBRUSH brush;
      brush = ::GetSysColorBrush(COLOR_3DDKSHADOW);
      if (brush)
        ::FrameRect(hdc, &widgetRect, brush);
      InflateRect(&widgetRect, -1, -1);
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_INFOBK+1));
      return NS_OK;
    // Draw 3D face background controls
    case NS_THEME_TAB_PANEL:    
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_RESIZER_PANEL: {
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_BTNFACE+1));
      return NS_OK;
    }
    // Draw 3D inset statusbar panel
    case NS_THEME_STATUSBAR_PANEL: {
      if (aFrame->GetNextSibling())
        widgetRect.right -= 2; // space between sibling status panels

      ::DrawEdge(hdc, &widgetRect, BDR_SUNKENOUTER, BF_RECT | BF_MIDDLE);
      return NS_OK;  
    }
    // Draw scrollbar thumb
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
      ::DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_RECT | BF_MIDDLE);
      return NS_OK;
    // Draw scrollbar track background
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL: {

      // Windows fills in the scrollbar track differently 
      // depending on whether these are equal
      DWORD color3D, colorScrollbar, colorWindow;

      color3D = ::GetSysColor(COLOR_3DFACE);      
      colorWindow = ::GetSysColor(COLOR_WINDOW);
      colorScrollbar = ::GetSysColor(COLOR_SCROLLBAR);
      
      if ((color3D != colorScrollbar) && (colorWindow != colorScrollbar))
        // Use solid brush
        ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_SCROLLBAR+1));
      else
      {
        // Use checkerboard pattern brush
        HBRUSH brush, oldBrush = NULL;
        HBITMAP patBmp = NULL;
        COLORREF oldBackColor, oldForeColor;
        static WORD patBits[8] = {
          0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55
        };
        
        patBmp = ::CreateBitmap(8, 8, 1, 1, patBits);
        if (patBmp) {
          brush = (HBRUSH) ::CreatePatternBrush(patBmp);
          if (brush) {        
            oldForeColor = ::SetTextColor(hdc, ::GetSysColor(COLOR_3DHILIGHT));
            oldBackColor = ::SetBkColor(hdc, color3D);

            ::UnrealizeObject(brush);
            ::SetBrushOrgEx(hdc, widgetRect.left, widgetRect.top, NULL);
            oldBrush = (HBRUSH) ::SelectObject(hdc, brush);

            ::FillRect(hdc, &widgetRect, brush);

            ::SetTextColor(hdc, oldForeColor);
            ::SetBkColor(hdc, oldBackColor);
            ::SelectObject(hdc, oldBrush);
            ::DeleteObject(brush);          
          }
          else
            ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_SCROLLBAR+1));
          
          ::DeleteObject(patBmp);
        }
      }
      // XXX should invert the part of the track being clicked here
      // but the track is never :active
      return NS_OK;
    }
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_HIGHLIGHT+1));
      return NS_OK;
    // Draw Tab
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE: {
      DrawTab(hdc, widgetRect,
        IsBottomTab(aFrame) ? BF_BOTTOM : BF_TOP, 
        IsSelectedTab(aFrame),
        aWidgetType != NS_THEME_TAB_RIGHT_EDGE,
        aWidgetType != NS_THEME_TAB_LEFT_EDGE);      

      return NS_OK;
    }
    case NS_THEME_TAB_PANELS:
      ::DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_SOFT | BF_MIDDLE |
          BF_LEFT | BF_RIGHT | BF_BOTTOM);
      return NS_OK;

  }
  return NS_ERROR_FAILURE;
}


///////////////////////////////////////////
// Creation Routine
///////////////////////////////////////////
NS_METHOD NS_NewNativeTheme(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsNativeThemeWin* theme = new nsNativeThemeWin();
  if (!theme)
    return NS_ERROR_OUT_OF_MEMORY;
  return theme->QueryInterface(aIID, aResult);
}
