/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
struct nsColor;

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
#define NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAIULABLE  \
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

/**
 * Conts need for Print Preview
 */
#ifdef NS_PRINT_PREVIEW
const PRUint8 kUseAltDCFor_NONE        = 0x00; // Do not use the AltDC for anything
const PRUint8 kUseAltDCFor_FONTMETRICS = 0x01; // Use it for only getting the font metrics
const PRUint8 kUseAltDCFor_CREATE_RC   = 0x02; // Use when creating RenderingContexts
const PRUint8 kUseAltDCFor_SURFACE_DIM = 0x04; // Use it for getting the Surface Dimensions
#endif

#define NS_IDEVICE_CONTEXT_IID   \
{ 0x5931c580, 0xb917, 0x11d1,    \
{ 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

//a cross platform way of specifying a native palette handle
typedef void * nsPalette;

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
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDEVICE_CONTEXT_IID)

  /**
   * Initialize the device context from a widget
   * @param aWidget a native widget to initialize the device context from
   * @return error status
   */
  NS_IMETHOD  Init(nsNativeWidget aWidget) = 0;

  /**
   * Create a rendering context and initialize it from an nsIView
   * @param aView view to initialize context from
   * @param aContext out parameter for new rendering context
   * @return error status
   */
  NS_IMETHOD  CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext) = 0;

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
   * Initialize a rendering context from a widget. This method is only for use
   * when a rendering context was obtained directly from a factory rather than
   * through one of the Create* methods above.
   * @param aContext rendering context to initialize
   * @param aWindow widget to initialize context from
   * @return error status
   */
  NS_IMETHOD  InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWindow) = 0;

  /**
   * Query the device to see if it supports native widgets. If not, then
   * nsIWidget->Create() calls should be avoided.
   * @param aSupportsWidgets out paramater. If PR_TRUE, then native widgets
   *        can be created.
   * @return error status
   */
  NS_IMETHOD  SupportsNativeWidgets(PRBool &aSupportsWidgets) = 0;

  /**
   * Obtain the size of a device unit relative to a Twip. A twip is 1/20 of
   * a point (which is 1/72 of an inch).
   * @param aDevUnitsToTwips out parameter for conversion value
   * @return error status
   */
  NS_IMETHOD  GetDevUnitsToTwips(float &aDevUnitsToTwips) const = 0;

  /**
   * Obtain the size of a Twip relative to a device unit.
   * @param aTwipsToDevUnits out parameter for conversion value
   * @return error status
   */
  NS_IMETHOD  GetTwipsToDevUnits(float &aTwipsToDevUnits) const = 0;

  /**
   * Set the scale factor to convert units used by the application
   * to device units. Typically, an application will query the device
   * for twips to device units scale and then set the scale
   * to convert from whatever unit the application wants to use
   * to device units. From that point on, all other parts of the
   * app can use the Get* methods below to figure out how
   * to convert device units <-> app units.
   * @param aAppUnits scale value to convert from application defined
   *        units to device units.
   * @return error status
   */
  NS_IMETHOD  SetAppUnitsToDevUnits(float aAppUnits) = 0;

  /**
   * Set the scale factor to convert device units to units
   * used by the application. This should generally be
   * 1.0f / the value passed into SetAppUnitsToDevUnits().
   * @param aDevUnits scale value to convert from device units to
   *        application defined units
   * @return error status
   */
  NS_IMETHOD  SetDevUnitsToAppUnits(float aDevUnits) = 0;

  /**
   * Get the scale factor to convert from application defined
   * units to device units.
   * @param aAppUnits out paramater for scale value
   * @return error status
   */
  NS_IMETHOD  GetAppUnitsToDevUnits(float &aAppUnits) const = 0;

  /**
   * Get the scale factor to convert from device units to
   * application defined units.
   * @param aDevUnits out paramater for scale value
   * @return error status
   */
  NS_IMETHOD  GetDevUnitsToAppUnits(float &aDevUnits) const = 0;

  /**
   * Get the value used to scale a "standard" pixel to a pixel
   * of the same physical size for this device. a standard pixel
   * is defined as a pixel on display 0. this is used to make
   * sure that entities defined in pixel dimensions maintain a
   * constant relative size when displayed from one output
   * device to another.
   * @param aScale out parameter for scale value
   * @return error status
   */
  NS_IMETHOD  GetCanonicalPixelScale(float &aScale) const = 0;

  /**
   * Get the width of a vertical scroll bar and the height
   * of a horizontal scrollbar in application units.
   * @param aWidth out parameter for width
   * @param aHeight out parameter for height
   * @return error status
   */
  NS_IMETHOD  GetScrollBarDimensions(float &aWidth, float &aHeight) const = 0;

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

  //get and set the document zoom value used for display-time
  //scaling. default is 1.0 (no zoom)
  NS_IMETHOD  SetZoom(float aZoom) = 0;
  NS_IMETHOD  GetZoom(float &aZoom) const = 0;

  //get and set the text zoom value used for display-time
  //scaling. default is 1.0 (no zoom)
  NS_IMETHOD  SetTextZoom(float aTextZoom) = 0;
  NS_IMETHOD  GetTextZoom(float &aTextZoom) const = 0;

  //get a low level drawing surface for rendering. the rendering context
  //that is passed in is used to create the drawing surface if there isn't
  //already one in the device context. the drawing surface is then cached
  //in the device context for re-use.
  NS_IMETHOD  GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface) = 0;

  //functions for handling gamma correction of output device
  NS_IMETHOD  GetGamma(float &aGamms) = 0;
  NS_IMETHOD  SetGamma(float aGamma) = 0;

  //XXX the return from this really needs to be ref counted somehow. MMP
  NS_IMETHOD  GetGammaTable(PRUint8 *&aGammaTable) = 0;

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
   * Returns Platform specific pixel value for an RGB value
   */
  NS_IMETHOD ConvertPixel(nscolor aColor, PRUint32 & aPixel) = 0;

  /**
   * Get the size of the displayable area of the output device
   * in app units.
   * @param aWidth out parameter for width
   * @param aHeight out parameter for height
   * @return error status
   */
  NS_IMETHOD GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight) = 0;

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
   * Returns a new nsIDeviceContext suitable for the device context
   * specification passed in.
   * @param aDevice a device context specification. this is a platform
   *        specific structure that only a platform specific device
   *        context can interpret.
   * @param aContext out parameter for new device context. nsnull on
   *        failure to create new device context.
   * @return error status
   */
  NS_IMETHOD GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                 nsIDeviceContext *&aContext) = 0;

  //XXX need to work out re-entrancy issues for these APIs... MMP
  /**
   * Inform the output device that output of a document is beginning
   * Used for print related device contexts. Must be matched 1:1 with
   * EndDocument().
   * XXX needs to take parameters so that feedback can be given to the
   * app regarding pagination progress and aborting print operations?
   * @return error status
   */
  NS_IMETHOD BeginDocument(PRUnichar * aTitle) = 0;

  /**
   * Inform the output device that output of a document is ending.
   * Used for print related device contexts. Must be matched 1:1 with
   * BeginDocument()
   * @return error status
   */
  NS_IMETHOD EndDocument(void) = 0;

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

#ifdef NS_PRINT_PREVIEW
  /**
   * Set an Alternative Device Context where some of the calls
   * are deferred to it
   */
  NS_IMETHOD SetAltDevice(nsIDeviceContext* aAltDC) = 0;

  /**
   * Get the Alternate Device Context
   */
  NS_IMETHOD GetAltDevice(nsIDeviceContext** aAltDC) = 0;

  /**
   * Turn on/off which types of information is retrived 
   * via the alt device context
   */
  NS_IMETHOD SetUseAltDC(PRUint8 aValue, PRBool aOn) = 0;
#endif

};

#endif /* nsIDeviceContext_h___ */
