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
#include "gfxTypes.h"
#include "nsStringFwd.h"

class nsIView;
class nsFontMetrics;
class nsIWidget;
class nsIDeviceContextSpec;
class nsIAtom;
class nsRect;
class gfxUserFontSet;
class nsRenderingContext;
struct nsFont;

#define NS_IDEVICE_CONTEXT_IID   \
{ 0x30a9d22f, 0x8e51, 0x40af, \
  { 0xa1, 0xf5, 0x48, 0xe3, 0x00, 0xaa, 0xa9, 0x27 } }

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
   * @param aWidget a widget to initialize the device context from
   * @return error status
   */
  NS_IMETHOD  Init(nsIWidget* aWidget) = 0;

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
  NS_IMETHOD  CreateRenderingContext(nsIView *aView, nsRenderingContext *&aContext) = 0;

  /**
   * Create a rendering context and initialize it from an nsIWidget
   * @param aWidget widget to initialize context from
   * @param aContext out parameter for new rendering context
   * @return error status
   */
  NS_IMETHOD  CreateRenderingContext(nsIWidget *aWidget, nsRenderingContext *&aContext) = 0;

  /**
   * Create a rendering context and initialize it.
   * @param aContext out parameter for new rendering context
   * @return error status
   */
  NS_IMETHOD  CreateRenderingContext(nsRenderingContext *&aContext) = 0;

  /**
   * Create an uninitalised rendering context.
   * @param aContext out parameter for new rendering context
   * @return error status
   */
  NS_IMETHOD  CreateRenderingContextInstance(nsRenderingContext *&aContext) = 0;

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
   * Convert app units to CSS pixels which is used in gfx/thebes.
   */
  static gfxFloat AppUnitsToGfxCSSPixels(nscoord aAppUnits)
  { return gfxFloat(aAppUnits) / AppUnitsPerCSSPixel(); }

  /**
   * Gets the number of app units in one device pixel; this number is usually
   * a factor of AppUnitsPerCSSPixel(), although that is not guaranteed.
   */
  PRInt32 AppUnitsPerDevPixel() const { return mAppUnitsPerDevPixel; }

  /**
   * Convert device pixels which is used for gfx/thebes to nearest (rounded)
   * app units
   */
  nscoord GfxUnitsToAppUnits(gfxFloat aGfxUnits) const
  { return nscoord(NS_round(aGfxUnits * AppUnitsPerDevPixel())); }

  /**
   * Convert app units to device pixels which is used for gfx/thebes.
   */
  gfxFloat AppUnitsToGfxUnits(nscoord aAppUnits) const
  { return gfxFloat(aAppUnits) / AppUnitsPerDevPixel(); }

  /**
   * Gets the number of app units in one physical inch; this is the
   * device's DPI times AppUnitsPerDevPixel().
   */
  PRInt32 AppUnitsPerPhysicalInch() const { return mAppUnitsPerPhysicalInch; }

  /**
   * Gets the number of app units in one CSS inch; this is the
   * 96 times AppUnitsPerCSSPixel.
   */
  static PRInt32 AppUnitsPerCSSInch() { return 96 * AppUnitsPerCSSPixel(); }

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
   * Get the nsFontMetrics that describe the properties of
   * an nsFont.
   * @param aFont font description to obtain metrics for
   * @param aLanguage the language of the document
   * @param aMetrics out parameter for font metrics
   * @param aUserFontSet user font set
   * @return error status
   */
  NS_IMETHOD  GetMetricsFor(const nsFont& aFont, nsIAtom* aLanguage,
                            gfxUserFontSet* aUserFontSet,
                            nsFontMetrics*& aMetrics) = 0;

  /**
   * Get the nsFontMetrics that describe the properties of
   * an nsFont.
   * @param aFont font description to obtain metrics for
   * @param aMetrics out parameter for font metrics
   * @param aUserFontSet user font set
   * @return error status
   */
  NS_IMETHOD  GetMetricsFor(const nsFont& aFont, gfxUserFontSet* aUserFontSet,
                            nsFontMetrics*& aMetrics) = 0;

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
  NS_IMETHOD FontMetricsDeleted(const nsFontMetrics* aFontMetrics) = 0;

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
   *         (whether AppUnitsPerDevPixel() or AppUnitsPerPhysicalInch() changed)
  */
  virtual PRBool CheckDPIChange() = 0;

  /**
   * Set the pixel scaling factor: all lengths are multiplied by this factor
   * when we convert them to device pixels. Returns whether the ratio of 
   * app units to dev pixels changed because of the scale factor.
   */
  virtual PRBool SetPixelScale(float aScale) = 0;

  /**
   * Get the pixel scaling factor; defaults to 1.0, but can be changed with
   * SetPixelScale.
   */
  float GetPixelScale() const { return mPixelScale; }

  /**
   * Get the unscaled ratio of app units to dev pixels; useful if something
   * needs to be converted from to unscaled pixels
   */
  PRInt32 UnscaledAppUnitsPerDevPixel() const { return mAppUnitsPerDevNotScaledPixel; }

protected:
  PRInt32 mAppUnitsPerDevPixel;
  PRInt32 mAppUnitsPerPhysicalInch;
  PRInt32 mAppUnitsPerDevNotScaledPixel;
  float  mPixelScale;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDeviceContext, NS_IDEVICE_CONTEXT_IID)

#endif /* nsIDeviceContext_h___ */
