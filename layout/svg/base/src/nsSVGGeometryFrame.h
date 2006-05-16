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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#ifndef __NS_SVGGEOMETRYFRAME_H__
#define __NS_SVGGEOMETRYFRAME_H__

#include "nsFrame.h"
#include "nsWeakReference.h"
#include "nsISVGValueObserver.h"
#include <cairo.h>

class nsSVGPaintServerFrame;
class nsISVGRendererCanvas;

typedef nsFrame nsSVGGeometryFrameBase;

/* nsSVGGeometryFrame is a base class for SVG objects that directly
 * have geometry (circle, ellipse, line, polyline, polygon, path, and
 * glyph frames).  It knows how to convert the style information into
 * cairo context information and stores the fill/stroke paint
 * servers. */

class nsSVGGeometryFrame : public nsSVGGeometryFrameBase,
                           public nsISVGValueObserver,
                           public nsSupportsWeakReference
{
public:
  nsSVGGeometryFrame(nsStyleContext *aContext);
  ~nsSVGGeometryFrame();

  // nsIFrame interface:
  NS_IMETHOD DidSetStyleContext();

  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable(nsISVGValue* observable,
                                    nsISVGValue::modificationType aModType);

  // nsSVGGeometryFrame methods:
  NS_IMETHOD GetCanvasTM(nsIDOMSVGMatrix * *aCanvasTM) = 0;
  PRUint16 GetClipRule();
  PRBool IsClipChild(); 

  float GetStrokeWidth();

  // Check if this geometry needs to be filled or stroked.  These also
  // have the side effect of looking up the paint server if that is
  // the indicated type and storing it in a property, so need to be
  // called before SetupCairo{Fill,Stroke}.
  PRBool HasFill();
  PRBool HasStroke();

  // Setup/Cleanup a cairo context for filling a path
  nsresult SetupCairoFill(nsISVGRendererCanvas *aCanvas,
                          cairo_t *aCtx,
                          void **aClosure);
  void CleanupCairoFill(cairo_t *aCtx, void *aClosure);

  // Set up a cairo context for measuring a stroked path
  void SetupCairoStrokeGeometry(cairo_t *aCtx);

  // Setup/Cleanup a cairo context for stroking path
  nsresult SetupCairoStroke(nsISVGRendererCanvas *aCanvas,
                            cairo_t *aCtx,
                            void **aClosure);
  void CleanupCairoStroke(cairo_t *aCtx, void *aClosure);

  enum {
    UPDATEMASK_NOTHING           = 0x00000000,
    UPDATEMASK_CANVAS_TM         = 0x00000001,
    UPDATEMASK_STROKE_OPACITY    = 0x00000002,
    UPDATEMASK_STROKE_WIDTH      = 0x00000004,
    UPDATEMASK_STROKE_DASH_ARRAY = 0x00000008,
    UPDATEMASK_STROKE_DASHOFFSET = 0x00000010,
    UPDATEMASK_STROKE_LINECAP    = 0x00000020,
    UPDATEMASK_STROKE_LINEJOIN   = 0x00000040,
    UPDATEMASK_STROKE_MITERLIMIT = 0x00000080,
    UPDATEMASK_FILL_OPACITY      = 0x00000100,
    UPDATEMASK_FILL_RULE         = 0x00000200,
    UPDATEMASK_STROKE_PAINT_TYPE = 0x00000400,
    UPDATEMASK_STROKE_PAINT      = 0x00000800,
    UPDATEMASK_FILL_PAINT_TYPE   = 0x00001000,
    UPDATEMASK_FILL_PAINT        = 0x00002000,
    UPDATEMASK_ALL               = 0xFFFFFFFF
  };

protected:
  virtual nsresult UpdateGraphic(PRUint32 flags,
                                 PRBool suppressInvalidation = PR_FALSE) = 0;

  nsSVGPaintServerFrame *GetPaintServer(const nsStyleSVGPaint *aPaint);

private:
  nsresult GetStrokeDashArray(double **arr, PRUint32 *count);
  float GetStrokeDashoffset();

  nsSVGPaintServerFrame* mFillServer;
  nsSVGPaintServerFrame* mStrokeServer;
};

#endif // __NS_SVGGEOMETRYFRAME_H__
