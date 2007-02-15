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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#ifndef nsIDeviceContext_h___
#define nsIDeviceContext_h___

#include "nsISupports.h"
#include "nsCoord.h"
#include "nsRect.h"
#include "nsIWidget.h"
#include "nsIRenderingContext.h"

class nsIView;
class nsIFontMetrics;
class nsIWidget;
class nsIDeviceContextSpec;
class nsIAtom;

struct nsFont;

//a cross platform way of specifying a native device context
typedef void * nsNativeDeviceContext;

/* error codes for printer device contexts */
#define NS_ERROR_GFX_PRINTER_BASE (1) /* adjustable :-) */
/* Unix: print command (lp/lpr) not found */
#define NS_ERROR_GFX_PRINTER_CMD_NOT_FOUND          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+1)
/* Unix: print command returned an error */  
#define NS_ERROR_GFX_PRINTER_CMD_FAILURE            \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+2)
/* no printer available (e.g. cannot find _any_ printer) */
#define NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE  \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+3)
/* _specified_ (by name) printer not found */
#define NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND         \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+4)
/* access to printer denied */
#define NS_ERROR_GFX_PRINTER_ACCESS_DENIED          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+5)
/* invalid printer attribute (for example: unsupported paper size etc.) */
#define NS_ERROR_GFX_PRINTER_INVALID_ATTRIBUTE      \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+6)
/* printer not "ready" (offline ?) */
#define NS_ERROR_GFX_PRINTER_PRINTER_NOT_READY     \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+8)
/* printer out of paper */
#define NS_ERROR_GFX_PRINTER_OUT_OF_PAPER           \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+9)
/* generic printer I/O error */
#define NS_ERROR_GFX_PRINTER_PRINTER_IO_ERROR       \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+10)
/* print-to-file: could not open output file */
#define NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE    \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+11)
/* print-to-file: I/O error while printing to file */
#define NS_ERROR_GFX_PRINTER_FILE_IO_ERROR          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+12)
/* print preview: needs at least one printer */
#define NS_ERROR_GFX_PRINTER_PRINTPREVIEW          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+13)
/* print: starting document */
#define NS_ERROR_GFX_PRINTER_STARTDOC          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+14)
/* print: ending document */
#define NS_ERROR_GFX_PRINTER_ENDDOC          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+15)
/* print: starting page */
#define NS_ERROR_GFX_PRINTER_STARTPAGE          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+16)
/* print: ending page */
#define NS_ERROR_GFX_PRINTER_ENDPAGE          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+17)
/* print: print while in print preview */
#define NS_ERROR_GFX_PRINTER_PRINT_WHILE_PREVIEW          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+18)
/* requested page size not supported by printer */
#define NS_ERROR_GFX_PRINTER_PAPER_SIZE_NOT_SUPPORTED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+19)
/* requested page orientation not supported */
#define NS_ERROR_GFX_PRINTER_ORIENTATION_NOT_SUPPORTED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+20)
/* requested colorspace not supported (like printing "color" on a "grayscale"-only printer) */
#define NS_ERROR_GFX_PRINTER_COLORSPACE_NOT_SUPPORTED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+21)
/* too many copies requested */
#define NS_ERROR_GFX_PRINTER_TOO_MANY_COPIES \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+22)
/* driver configuration error */
#define NS_ERROR_GFX_PRINTER_DRIVER_CONFIGURATION_ERROR \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+23)
/* Xprint module specific: Xprt server broken */
#define NS_ERROR_GFX_PRINTER_XPRINT_BROKEN_XPRT \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+24)
/* The document is still being loaded, can't Print Preview */
#define NS_ERROR_GFX_PRINTER_DOC_IS_BUSY_PP \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+25)
/* The document was asked to be destroyed while we were preparing printing */
#define NS_ERROR_GFX_PRINTER_DOC_WAS_DESTORYED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+26)
/* Cannot Print or Print Preview XUL Documents */
#define NS_ERROR_GFX_PRINTER_NO_XUL \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+27)
/* The toolkit no longer supports the Print Dialog (for embedders) */
#define NS_ERROR_GFX_NO_PRINTDIALOG_IN_TOOLKIT \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+28)
/* The was wasn't any Print Prompt service registered (this shouldn't happen) */
#define NS_ERROR_GFX_NO_PRINTROMPTSERVICE \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+29)
/* Xprint module specific: No Xprint servers found */
#define NS_ERROR_GFX_PRINTER_XPRINT_NO_XPRINT_SERVERS_FOUND \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+30)
/* requested plex mode not supported by printer */
#define NS_ERROR_GFX_PRINTER_PLEX_NOT_SUPPORTED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+31)
/* The document is still being loaded */
#define NS_ERROR_GFX_PRINTER_DOC_IS_BUSY \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+32)
/* Printing is not implemented */
#define NS_ERROR_GFX_PRINTING_NOT_IMPLEMENTED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+33)
/* Cannot load the matching print module */
#define NS_ERROR_GFX_COULD_NOT_LOAD_PRINT_MODULE \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+34)   
/* requested resolution/quality mode not supported by printer */
#define NS_ERROR_GFX_PRINTER_RESOLUTION_NOT_SUPPORTED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+35)
      
/**
 * Conts need for Print Preview
 */
#ifdef NS_PRINT_PREVIEW
const PRUint8 kUseAltDCFor_NONE            = 0x00; // Do not use the AltDC for anything
const PRUint8 kUseAltDCFor_FONTMETRICS     = 0x01; // Use it for only getting the font metrics
const PRUint8 kUseAltDCFor_CREATERC_REFLOW = 0x02; // Use when creating RenderingContexts for Reflow
const PRUint8 kUseAltDCFor_CREATERC_PAINT  = 0x04; // Use when creating RenderingContexts for Painting
const PRUint8 kUseAltDCFor_SURFACE_DIM     = 0x08; // Use it for getting the Surface Dimensions
#endif

#define NS_IDEVICE_CONTEXT_IID   \
{ 0xb05ae6b9, 0x280c, 0x4b16, \
 { 0xb1, 0x36, 0x86, 0x7c, 0x48, 0xd2, 0x15, 0x54 } }

//a cross platform way of specifying a native palette handle
typedef void * nsPalette;

  //structure used to return information about a device's palette capabilities
  struct nsPaletteInfo {
     PRPackedBool  isPaletteDevice;
     PRUint16      sizePalette;  // number of entries in the palette
     PRUint16      numReserved;  // number of reserved palette entries
     nsPalette     palette;      // native palette handle
  };

  typedef enum {
    eSystemFont_Caption,         // css2
    eSystemFont_Icon,
    eSystemFont_Menu,
    eSystemFont_MessageBox,
    eSystemFont_SmallCaption,
    eSystemFont_StatusBar,

    eSystemFont_Window,          // css3
    eSystemFont_Document,
    eSystemFont_Workspace,
    eSystemFont_Desktop,
    eSystemFont_Info,
    eSystemFont_Dialog,
    eSystemFont_Button,
    eSystemFont_PullDownMenu,
    eSystemFont_List,
    eSystemFont_Field,

    eSystemFont_Tooltips,        // moz
    eSystemFont_Widget
  } nsSystemFontID;

class nsIDeviceContext : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDEVICE_CONTEXT_IID)

  /**
   * Initialize the device context from a widget
   * @param aWidget a native widget to initialize the device context from
   * @return error status
   */
  NS_IMETHOD  Init(nsNativeWidget aWidget) = 0;

  /**
   * Initialize the device context from a device context spec
   * @param aDevSpec the specification of the printng device (platform-specific)
   * @return error status
   */
  NS_IMETHOD  InitForPrinting(nsIDeviceContextSpec* aDevSpec) = 0;

  /**
   * Create a rendering context and initialize it from an nsIView
   * @param aView view to initialize context from
   * @param aContext out parameter for new rendering context
   * @return error status
   */
  NS_IMETHOD  CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext) = 0;

  /**
   * Create a rendering context and initialize it from an nsIDrawingSurface*
   * @param nsIDrawingSurface* widget to initialize context from
   * @param aContext out parameter for new rendering context
   * @return error status
   */
  NS_IMETHOD  CreateRenderingContext(nsIDrawingSurface* aSurface, nsIRenderingContext *&aContext) = 0;

  /**
   * Create a rendering context and initialize it from an nsIWidget
   * @param aWidget widget to initialize context from
   * @param aContext out parameter for new rendering context
   * @return error status
   */
  NS_IMETHOD  CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext) = 0;

  /**
   * Create a rendering context and initialize it. This API should *only* be called
   * on device contexts whose SupportsNativeWidgets() method return PR_FALSE.
   * @param aContext out parameter for new rendering context
   * @return error status
   */
  NS_IMETHOD  CreateRenderingContext(nsIRenderingContext *&aContext) = 0;

  /**
   * Create an uninitalised rendering context.
   * @param aContext out parameter for new rendering context
   * @return error status
   */
  NS_IMETHOD  CreateRenderingContextInstance(nsIRenderingContext *&aContext) = 0;

  /**
   * Query the device to see if it supports native widgets. If not, then
   * nsIWidget->Create() calls should be avoided.
   * @param aSupportsWidgets out paramater. If PR_TRUE, then native widgets
   *        can be created.
   * @return error status
   */
  NS_IMETHOD  SupportsNativeWidgets(PRBool &aSupportsWidgets) = 0;

  /**
   * We are in the process of creating the native widget for aWidget.
   * Do any device-specific processing required to initialize the
   * native widget for this device. A pointer to some platform-specific data is
   * returned in aOut.
   *
   * GTK2 calls this to get the required visual for the window.
   */
  NS_IMETHOD PrepareNativeWidget(nsIWidget* aWidget, void** aOut) = 0;

  /**
   * Gets the number of app units in one CSS pixel; this number is global,
   * not unique to each device context.
   */
  static PRInt32 AppUnitsPerCSSPixel() { return 60; }

  /**
   * Gets the number of app units in one device pixel; this number is usually
   * a factor of AppUnitsPerCSSPixel(), although that is not guaranteed.
   */
  PRInt32 AppUnitsPerDevPixel() const { return mAppUnitsPerDevPixel; }

  /**
   * Gets the number of app units in one inch; this is the device's DPI
   * times AppUnitsPerDevPixel().
   */
  PRInt32 AppUnitsPerInch() const { return mAppUnitsPerInch; }

  /**
   * Fill in an nsFont based on the ID of a system font.  This function
   * may or may not fill in the size, so the size should be set to a
   * reasonable default before calling.
   *
   * @param aID    The system font ID.
   * @param aInfo  The font structure to be filled in.
   * @return error status
   */
  NS_IMETHOD  GetSystemFont(nsSystemFontID aID, nsFont *aFont) const = 0;

  /**
   * Get the nsIFontMetrics that describe the properties of
   * an nsFont.
   * @param aFont font description to obtain metrics for
   * @param aLangGroup the language group of the document
   * @param aMetrics out parameter for font metrics
   * @return error status
   */
  NS_IMETHOD  GetMetricsFor(const nsFont& aFont, nsIAtom* aLangGroup,
                            nsIFontMetrics*& aMetrics) = 0;

  /**
   * Get the nsIFontMetrics that describe the properties of
   * an nsFont.
   * @param aFont font description to obtain metrics for
   * @param aMetrics out parameter for font metrics
   * @return error status
   */
  NS_IMETHOD  GetMetricsFor(const nsFont& aFont, nsIFontMetrics*& aMetrics) = 0;

  /**
   * Check to see if a particular named font exists.
   * @param aFontName character string of font face name
   * @return NS_OK if font is available, else font is unavailable
   */
  NS_IMETHOD CheckFontExistence(const nsString& aFaceName) = 0;
  NS_IMETHOD FirstExistingFont(const nsFont& aFont, nsString& aFaceName) = 0;

  NS_IMETHOD GetLocalFontName(const nsString& aFaceName, nsString& aLocalName,
                              PRBool& aAliased) = 0;

  /**
   * Notification when a font metrics instance created for this device is
   * about to be deleted
   */
  NS_IMETHOD FontMetricsDeleted(const nsIFontMetrics* aFontMetrics) = 0;

  /**
   * Attempt to free up resoruces by flushing out any fonts no longer
   * referenced by anything other than the font cache itself.
   * @return error status
   */
  NS_IMETHOD FlushFontCache(void) = 0;

  /**
   * Return the bit depth of the device.
   */
  NS_IMETHOD GetDepth(PRUint32& aDepth) = 0;

  /**
   * Returns information about the device's palette capabilities.
   */
  NS_IMETHOD GetPaletteInfo(nsPaletteInfo& aPaletteInfo) = 0;

  /**
   * Get the size of the displayable area of the output device
   * in app units.
   * @param aWidth out parameter for width
   * @param aHeight out parameter for height
   * @return error status
   */
  NS_IMETHOD GetDeviceSurfaceDimensions(nscoord &aWidth, nscoord &aHeight) = 0;

  /**
   * Get the size of the content area of the output device in app units.
   * This corresponds on a screen device, for instance, to the entire screen.
   * @param aRect out parameter for full rect. Position (x,y) will be (0,0) or
   *              relative to the primary monitor if this is not the primary.
   * @return error status
   */
  NS_IMETHOD GetRect ( nsRect &aRect ) = 0;

  /**
   * Get the size of the content area of the output device in app units.
   * This corresponds on a screen device, for instance, to the area reported
   * by GetDeviceSurfaceDimensions, minus the taskbar (Windows) or menubar
   * (Macintosh).
   * @param aRect out parameter for client rect. Position (x,y) will be (0,0)
   *              adjusted for any upper/left non-client space if present or
   *              relative to the primary monitor if this is not the primary.
   * @return error status
   */
  NS_IMETHOD GetClientRect(nsRect &aRect) = 0;

  /**
   * This is enables the DeviceContext to anything it needs to do for Printing
   * before Reflow and BeginDocument is where work can be done after reflow.
   * @param aTitle - itle of Document
   * @param aPrintToFileName - name of file to print to, if NULL then don't print to file
   *
   * @return error status
   */
  NS_IMETHOD PrepareDocument(PRUnichar * aTitle, 
                             PRUnichar*  aPrintToFileName) = 0;

  //XXX need to work out re-entrancy issues for these APIs... MMP
  /**
   * Inform the output device that output of a document is beginning
   * Used for print related device contexts. Must be matched 1:1 with
   * EndDocument().
   * XXX needs to take parameters so that feedback can be given to the
   * app regarding pagination progress and aborting print operations?
   *
   * @param aTitle - itle of Document
   * @param aPrintToFileName - name of file to print to, if NULL then don't print to file
   * @param aStartPage - starting page number (must be greater than zero)
   * @param aEndPage - ending page number     (must be less than or equal to number of pages)
   *
   * @return error status
   */
  NS_IMETHOD BeginDocument(PRUnichar*  aTitle, 
                           PRUnichar*  aPrintToFileName,
                           PRInt32     aStartPage, 
                           PRInt32     aEndPage) = 0;

  /**
   * Inform the output device that output of a document is ending.
   * Used for print related device contexts. Must be matched 1:1 with
   * BeginDocument()
   * @return error status
   */
  NS_IMETHOD EndDocument(void) = 0;

  /**
   * Inform the output device that output of a document is being aborted.
   * Must be matched 1:1 with BeginDocument()
   * @return error status
   */
  NS_IMETHOD AbortDocument(void) = 0;

  /**
   * Inform the output device that output of a page is beginning
   * Used for print related device contexts. Must be matched 1:1 with
   * EndPage() and within a BeginDocument()/EndDocument() pair.
   * @return error status
   */
  NS_IMETHOD BeginPage(void) = 0;

  /**
   * Inform the output device that output of a page is ending
   * Used for print related device contexts. Must be matched 1:1 with
   * BeginPage() and within a BeginDocument()/EndDocument() pair.
   * @return error status
   */
  NS_IMETHOD EndPage(void) = 0;

  /**
   * Clear cached system fonts (refresh from theme when
   * requested).  This method is effectively static,
   * and can be called on a new DeviceContext instance
   * without any initialization.  Only really used by
   * Gtk native theme stuff.
   */
  NS_IMETHOD ClearCachedSystemFonts() = 0;

  /**
   * Check to see if the DPI has changed
   * @return whether there was actually a change in the DPI
   *         (whether AppUnitsPerDevPixel() or AppUnitsPerInch() changed)
  */
  virtual PRBool CheckDPIChange() = 0;

protected:
  PRInt32 mAppUnitsPerDevPixel;
  PRInt32 mAppUnitsPerInch;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDeviceContext, NS_IDEVICE_CONTEXT_IID)

#endif /* nsIDeviceContext_h___ */
