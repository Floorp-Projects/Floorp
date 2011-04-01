/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is thebes gfx
 *
 * The Initial Developer of the Original Code is mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#include "nsIRenderingContext.h"

#include "gfxWindowsSurface.h"

#include "nsSystemFontsWin.h"


nsresult nsSystemFontsWin::CopyLogFontToNSFont(HDC* aHDC, const LOGFONTW* ptrLogFont,
                                               nsString *aFontName,
                                               gfxFontStyle *aFontStyle) const
{
  PRUnichar name[LF_FACESIZE];
  name[0] = 0;
  memcpy(name, ptrLogFont->lfFaceName, LF_FACESIZE*sizeof(PRUnichar));
  *aFontName = name;

  // Do Style
  aFontStyle->style = FONT_STYLE_NORMAL;
  if (ptrLogFont->lfItalic)
  {
    aFontStyle->style = FONT_STYLE_ITALIC;
  }
  // XXX What about oblique?

  // Do Weight
  aFontStyle->weight = (ptrLogFont->lfWeight == FW_BOLD ? 
            FONT_WEIGHT_BOLD : FONT_WEIGHT_NORMAL);

  // FIXME: Set aFontStyle->stretch correctly!
  aFontStyle->stretch = NS_FONT_STRETCH_NORMAL;

  // XXX mPixelScale is currently hardcoded to 1 in thebes gfx...
  float mPixelScale = 1.0f;
  // Do Point Size
  //
  // The lfHeight is in pixel and it needs to be adjusted for the
  // device it will be "displayed" on
  // Screens and Printers will differe in DPI
  //
  // So this accounts for the difference in the DeviceContexts
  // The mPixelScale will be a "1" for the screen and could be
  // any value when going to a printer, for example mPixleScale is
  // 6.25 when going to a 600dpi printer.
  // round, but take into account whether it is negative
  float pixelHeight = -ptrLogFont->lfHeight;
  if (pixelHeight < 0) {
    HFONT hFont = ::CreateFontIndirectW(ptrLogFont);
    if (!hFont)
      return NS_ERROR_OUT_OF_MEMORY;
    HGDIOBJ hObject = ::SelectObject(*aHDC, hFont);
    TEXTMETRIC tm;
    ::GetTextMetrics(*aHDC, &tm);
    ::SelectObject(*aHDC, hObject);
    ::DeleteObject(hFont);
    pixelHeight = tm.tmAscent;
  }

  pixelHeight *= mPixelScale;

  // we have problem on Simplified Chinese system because the system
  // report the default font size is 8 points. but if we use 8, the text
  // display very ugly. force it to be at 9 points (12 pixels) on that
  // system (cp936), but leave other sizes alone.
  if ((pixelHeight < 12) && 
      (936 == ::GetACP())) 
    pixelHeight = 12;

  aFontStyle->size = pixelHeight;
  return NS_OK;
}

nsresult nsSystemFontsWin::GetSysFontInfo(HDC aHDC, nsSystemFontID anID,
                                          nsString *aFontName,
                                          gfxFontStyle *aFontStyle) const
{
  HGDIOBJ hGDI;

  LOGFONTW logFont;
  LOGFONTW* ptrLogFont = NULL;

#ifdef WINCE
  hGDI = ::GetStockObject(SYSTEM_FONT);
  if (hGDI == NULL)
    return NS_ERROR_UNEXPECTED;
  
  if (::GetObjectW(hGDI, sizeof(logFont), &logFont) > 0)
    ptrLogFont = &logFont;
#else

  NONCLIENTMETRICSW ncm;

  BOOL status;
  if (anID == eSystemFont_Icon) 
  {
    status = ::SystemParametersInfoW(SPI_GETICONTITLELOGFONT,
                                     sizeof(logFont),
                                     (PVOID)&logFont,
                                     0);
  }
  else
  {
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    status = ::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, 
                                     sizeof(ncm),  
                                     (PVOID)&ncm, 
                                     0);
  }

  if (!status)
  {
    return NS_ERROR_FAILURE;
  }

  switch (anID)
  {
    // Caption in CSS is NOT the same as Caption on Windows
    //case eSystemFont_Caption: 
    //  ptrLogFont = &ncm.lfCaptionFont;
    //  break;

    case eSystemFont_Icon: 
      ptrLogFont = &logFont;
      break;

    case eSystemFont_Menu: 
      ptrLogFont = &ncm.lfMenuFont;
      break;

    case eSystemFont_MessageBox: 
      ptrLogFont = &ncm.lfMessageFont;
      break;

    case eSystemFont_SmallCaption: 
      ptrLogFont = &ncm.lfSmCaptionFont;
      break;

    case eSystemFont_StatusBar: 
    case eSystemFont_Tooltips: 
      ptrLogFont = &ncm.lfStatusFont;
      break;

    case eSystemFont_Widget:

    case eSystemFont_Window:      // css3
    case eSystemFont_Document:
    case eSystemFont_Workspace:
    case eSystemFont_Desktop:
    case eSystemFont_Info:
    case eSystemFont_Dialog:
    case eSystemFont_Button:
    case eSystemFont_PullDownMenu:
    case eSystemFont_List:
    case eSystemFont_Field:
    case eSystemFont_Caption: 
      hGDI = ::GetStockObject(DEFAULT_GUI_FONT);
      if (hGDI != NULL)
      {
        if (::GetObjectW(hGDI, sizeof(logFont), &logFont) > 0)
        { 
          ptrLogFont = &logFont;
        }
      }
      break;
  } // switch 

#endif // WINCE

  if (nsnull == ptrLogFont)
  {
    return NS_ERROR_FAILURE;
  }

  aFontStyle->systemFont = PR_TRUE;

  return CopyLogFontToNSFont(&aHDC, ptrLogFont, aFontName, aFontStyle);
}

nsresult nsSystemFontsWin::GetSystemFont(nsSystemFontID anID,
                                         nsString *aFontName,
                                         gfxFontStyle *aFontStyle) const
{
  nsresult status = NS_OK;

  switch (anID) {
    case eSystemFont_Caption: 
    case eSystemFont_Icon: 
    case eSystemFont_Menu: 
    case eSystemFont_MessageBox: 
    case eSystemFont_SmallCaption: 
    case eSystemFont_StatusBar: 
    case eSystemFont_Tooltips: 
    case eSystemFont_Widget:

    case eSystemFont_Window:      // css3
    case eSystemFont_Document:
    case eSystemFont_Workspace:
    case eSystemFont_Desktop:
    case eSystemFont_Info:
    case eSystemFont_Dialog:
    case eSystemFont_Button:
    case eSystemFont_PullDownMenu:
    case eSystemFont_List:
    case eSystemFont_Field:
    {
      HWND hwnd = nsnull;
      HDC tdc = GetDC(hwnd);

      status = GetSysFontInfo(tdc, anID, aFontName, aFontStyle);

      ReleaseDC(hwnd, tdc);

      break;
    }
  }

  return status;
}

nsSystemFontsWin::nsSystemFontsWin()
{

}

