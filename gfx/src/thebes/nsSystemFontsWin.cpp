
#include "nsIRenderingContext.h"

#include "gfxWindowsSurface.h"

#include "nsSystemFontsWin.h"


nsresult nsSystemFontsWin::CopyLogFontToNSFont(HDC* aHDC, const LOGFONT* ptrLogFont,
					       nsFont* aFont, PRBool aIsWide) const
{
  PRUnichar name[LF_FACESIZE];
  name[0] = 0;
  if (aIsWide)
    memcpy(name, ptrLogFont->lfFaceName, LF_FACESIZE*2);
  else {
    MultiByteToWideChar(CP_ACP, 0, ptrLogFont->lfFaceName,
      strlen(ptrLogFont->lfFaceName) + 1, name, sizeof(name)/sizeof(name[0]));
  }
  aFont->name = name;

  // Do Style
  aFont->style = NS_FONT_STYLE_NORMAL;
  if (ptrLogFont->lfItalic)
  {
    aFont->style = NS_FONT_STYLE_ITALIC;
  }
  // XXX What about oblique?

  aFont->variant = NS_FONT_VARIANT_NORMAL;

  // Do Weight
  aFont->weight = (ptrLogFont->lfWeight == FW_BOLD ? 
            NS_FONT_WEIGHT_BOLD : NS_FONT_WEIGHT_NORMAL);

  // Do decorations
  aFont->decorations = NS_FONT_DECORATION_NONE;
  if (ptrLogFont->lfUnderline)
  {
    aFont->decorations |= NS_FONT_DECORATION_UNDERLINE;
  }
  if (ptrLogFont->lfStrikeOut)
  {
    aFont->decorations |= NS_FONT_DECORATION_LINE_THROUGH;
  }


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
    HFONT hFont = ::CreateFontIndirect(ptrLogFont);
    if (!hFont)
      return NS_ERROR_OUT_OF_MEMORY;
    HGDIOBJ hObject = ::SelectObject(*aHDC, hFont);
    TEXTMETRIC tm;
    ::GetTextMetrics(*aHDC, &tm);
    ::SelectObject(*aHDC, hObject);
    ::DeleteObject(hFont);
    pixelHeight = tm.tmAscent;
  }

  float pointSize = pixelHeight * mPixelScale * 72 / ::GetDeviceCaps(*aHDC, LOGPIXELSY);

  // we have problem on Simplified Chinese system because the system report
  // the default font size is 8. but if we use 8, the text display very
  // Ugly. force it to be at 9 on that system (cp936), but leave other sizes alone.
  if ((pointSize < 9) && 
      (936 == ::GetACP())) 
    pointSize = 9;

  aFont->size = NSFloatPointsToTwips(pointSize);
  return NS_OK;
}

nsresult nsSystemFontsWin::GetSysFontInfo(HDC aHDC, nsSystemFontID anID, nsFont* aFont) const
{
  HGDIOBJ hGDI;

  LOGFONT logFont;
  LOGFONT* ptrLogFont = NULL;

#ifdef WINCE
  hGDI = ::GetStockObject(SYSTEM_FONT);
  if (hGDI == NULL)
    return NS_ERROR_UNEXPECTED;
  
  if (::GetObject(hGDI, sizeof(logFont), &logFont) > 0)
    ptrLogFont = &logFont;
#else

  NONCLIENTMETRICS ncm;

  BOOL status;
  if (anID == eSystemFont_Icon) 
  {
    status = ::SystemParametersInfo(SPI_GETICONTITLELOGFONT,
                                  sizeof(logFont),
                                  (PVOID)&logFont,
                                  0);
  }
  else
  {
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    status = ::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 
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
        if (::GetObject(hGDI, sizeof(logFont), &logFont) > 0)
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

  aFont->systemFont = PR_TRUE;

  return CopyLogFontToNSFont(&aHDC, ptrLogFont, aFont);
}

nsresult nsSystemFontsWin::GetSystemFont(nsSystemFontID anID, nsFont *aFont) const
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

      status = GetSysFontInfo(tdc, anID, aFont);

      ReleaseDC(hwnd, tdc);

      break;
    }
  }

  return status;
}

nsSystemFontsWin::nsSystemFontsWin(float aPixelsToTwips)
{

}

