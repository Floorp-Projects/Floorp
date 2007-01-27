/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfxTypes.h"

#include "gfxASurface.h"
#include "gfxColor.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "gfxMatrix.h"
#include "gfxPattern.h"

typedef struct _cairo cairo_t;

/**
 * This is the main class for doing actual drawing. It is initialized using
 * a surface and can be drawn on. It manages various state information like
 * a current transformation matrix (CTM), a current path, current color,
 * etc.
 *
 * All drawing happens by creating a path and then stroking or filling it.
 * The functions like Rectangle and Arc do not do any drawing themselves.
 * When a path is drawn (stroked or filled), it is filled/stroked with a
 * pattern set by SetPattern, SetColor or SetSource.
 */
class THEBES_API gfxContext {
    THEBES_INLINE_DECL_REFCOUNTING(gfxContext)

public:
    /**
     * Initialize this context from a surface.
     */
    gfxContext(gfxASurface *surface);
    ~gfxContext();

    /**
     * Return the surface that this gfxContext was created with
     */
    gfxASurface *OriginalSurface();

    /**
     * Return the current transparency group target, if any, along
     * with its device offsets from the top.  If no group is
     * active, returns the surface the gfxContext was created with,
     * and 0,0 in dx,dy.
     */
    already_AddRefed<gfxASurface> CurrentSurface(gfxFloat *dx, gfxFloat *dy);
    already_AddRefed<gfxASurface> CurrentSurface() {
        return CurrentSurface(NULL, NULL);
    }

    /**
     * Return the raw cairo_t object.
     * XXX this should go away at some point.
     */
    cairo_t *GetCairo() { return mCairo; }

    /**
     ** State
     **/
    // XXX document exactly what bits are saved
    void Save();
    void Restore();

    /**
     ** Paths & Drawing
     **/

    /**
     * Stroke the current path using the current settings (such as line
     * width and color).
     * A path is set up using functions such as Line, Rectangle and Arc.
     *
     * Does not consume the current path.
     */
    void Stroke();
    /**
     * Fill the current path according to the current settings.
     *
     * Does not consume the current path.
     */
    void Fill();

    /**
     * Forgets the current path.
     */
    void NewPath();

    /**
     * Closes the path, i.e. connects the last drawn point to the first one.
     *
     * Filling a path will implicitly close it.
     */
    void ClosePath();

    /**
     * Moves the pen to a new point without drawing a line.
     */
    void MoveTo(gfxPoint pt);

    /**
     * Returns the current point in the current path.
     */
    gfxPoint CurrentPoint() const;

    /**
     * Draws a line from the current point to pt.
     *
     * @see MoveTo
     */
    void LineTo(gfxPoint pt);

    /**
     * Draws a quadratic Bézier curve with control points pt1, pt2 and pt3.
     */
    void CurveTo(gfxPoint pt1, gfxPoint pt2, gfxPoint pt3);

    /**
     * Draws a clockwise arc (i.e. a circle segment).
     * @param center The center of the circle
     * @param radius The radius of the circle
     * @param angle1 Starting angle for the segment
     * @param angle2 Ending angle
     */
    void Arc(gfxPoint center, gfxFloat radius,
             gfxFloat angle1, gfxFloat angle2);

    /**
     * Draws a counter-clockwise arc (i.e. a circle segment).
     * @param center The center of the circle
     * @param radius The radius of the circle
     * @param angle1 Starting angle for the segment
     * @param angle2 Ending angle
     */

    void NegativeArc(gfxPoint center, gfxFloat radius,
                     gfxFloat angle1, gfxFloat angle2);

    // path helpers
    /**
     * Draws a line from start to end.
     */
    void Line(gfxPoint start, gfxPoint end); // XXX snapToPixels option?

    /**
     * Draws the rectangle given by rect.
     * @param snapToPixels ?
     */
    void Rectangle(gfxRect rect, PRBool snapToPixels = PR_FALSE);
    void Ellipse(gfxPoint center, gfxSize dimensions);
    void Polygon(const gfxPoint *points, PRUint32 numPoints);

    /**
     ** Transformation Matrix manipulation
     **/

    /**
     * Adds a translation to the current matrix. This translation takes place
     * before the previously set transformations.
     */
    void Translate(gfxPoint pt);

    /**
     * Adds a scale to the current matrix. This scaling takes place before the
     * previously set transformations.
     */
    void Scale(gfxFloat x, gfxFloat y);

    /**
     * Adds a rotation around the origin to the current matrix. This rotation
     * takes place before the previously set transformations.
     *
     * @param angle The angle in radians.
     */
    void Rotate(gfxFloat angle);

    /**
     * Post-multiplies 'other' onto the current CTM, i.e. this
     * matrix's transformation will take place before the previously set
     * transformations.
     */
    void Multiply(const gfxMatrix& other);

    /**
     * Replaces the current transformation matrix with matrix.
     */
    void SetMatrix(const gfxMatrix& matrix);

    /**
     * Sets the transformation matrix to the identity matrix.
     */
    void IdentityMatrix();

    /**
     * Returns the current transformation matrix.
     */
    gfxMatrix CurrentMatrix() const;

    /**
     * Converts a point from device to user coordinates using the inverse
     * transformation matrix.
     */
    gfxPoint DeviceToUser(const gfxPoint& point) const;

    /**
     * Converts a size from device to user coordinates. This does not apply
     * translation components of the matrix.
     */
    gfxSize DeviceToUser(const gfxSize& size) const;

    /**
     * Converts a rectangle from device to user coordinates; this has the
     * same effect as using DeviceToUser on both the rectangle's point and
     * size.
     */
    gfxRect DeviceToUser(const gfxRect& rect) const;

    /**
     * Converts a point from user to device coordinates using the inverse
     * transformation matrix.
     */
    gfxPoint UserToDevice(const gfxPoint& point) const;

    /**
     * Converts a size from user to device coordinates. This does not apply
     * translation components of the matrix.
     */
    gfxSize UserToDevice(const gfxSize& size) const;

    /**
     * Converts a rectangle from user to device coordinates; this has the
     * same effect as using DeviceToUser on both the rectangle's point and
     * size.
     */
    gfxRect UserToDevice(const gfxRect& rect) const;

    /**
     * Takes the given rect and tries to align it to device pixels.  If
     * this succeeds, the method will return PR_TRUE, and the rect will
     * be in device coordinates (already transformed by the CTM).  If it 
     * fails, the method will return PR_FALSE, and the rect will not be
     * changed.
     */
    PRBool UserToDevicePixelSnapped(gfxRect& rect) const;

    /**
     * Attempts to pixel snap the rectangle, add it to the current
     * path, and to set pattern as the current painting source.  This
     * should be used for drawing filled pixel-snapped rectangles (like
     * images), because the CTM at the time of the SetPattern call needs
     * to have a snapped translation, or you get smeared images.
     */
    void PixelSnappedRectangleAndSetPattern(const gfxRect& rect, gfxPattern *pattern);

    /**
     ** Painting sources
     **/

    /**
     * Uses a solid color for drawing.
     */
    void SetColor(const gfxRGBA& c);

    /**
     * Gets the current color.
     * returns PR_FALSE if there is something other than a color
     *         set as the current source (pattern, surface, etc)
     */
    PRBool GetColor(gfxRGBA& c);

    /**
     * Uses a surface for drawing. This is a shorthand for creating a
     * pattern and setting it.
     *
     * @param offset ?
     */
    void SetSource(gfxASurface *surface, gfxPoint offset = gfxPoint(0.0, 0.0));

    /**
     * Uses a pattern for drawing.
     */
    void SetPattern(gfxPattern *pattern);

    /**
     * Get the source pattern (solid color, normal pattern, surface, etc)
     */
    already_AddRefed<gfxPattern> GetPattern();

    /**
     ** Painting
     **/
    /**
     * Paints the current source surface/pattern everywhere in the current
     * clip region.
     */
    void Paint(gfxFloat alpha = 1.0);

    /**
     ** Painting with a Mask
     **/
    /**
     * Like Paint, except that it only draws the source where pattern is
     * non-transparent.
     */
    void Mask(gfxPattern *pattern);

    /**
     * Shorthand for creating a pattern and calling the pattern-taking
     * variant of Mask.
     */
    void Mask(gfxASurface *surface, gfxPoint offset = gfxPoint(0.0, 0.0));

    /**
     ** Shortcuts
     **/

    /**
     * Creates a new path with a rectangle from 0,0 to size.w,size.h
     * and calls cairo_fill.
     */
    void DrawSurface(gfxASurface *surface, gfxSize size);

    /**
     ** Line Properties
     **/

    typedef enum {
        gfxLineSolid,
        gfxLineDashed,
        gfxLineDotted
    } gfxLineType;

    void SetDash(gfxLineType ltype);
    void SetDash(gfxFloat *dashes, int ndash, gfxFloat offset);
    //void getDash() const;

    /**
     * Sets the line width that's used for line drawing.
     */
    void SetLineWidth(gfxFloat width);

    /**
     * Returns the currently set line width.
     *
     * @see SetLineWidth
     */
    gfxFloat CurrentLineWidth() const;

    enum GraphicsLineCap {
        LINE_CAP_BUTT,
        LINE_CAP_ROUND,
        LINE_CAP_SQUARE
    };
    /**
     * Sets the line caps, i.e. how line endings are drawn.
     */
    void SetLineCap(GraphicsLineCap cap);
    GraphicsLineCap CurrentLineCap() const;

    enum GraphicsLineJoin {
        LINE_JOIN_MITER,
        LINE_JOIN_ROUND,
        LINE_JOIN_BEVEL
    };
    /**
     * Sets the line join, i.e. how the connection between two lines is
     * drawn.
     */
    void SetLineJoin(GraphicsLineJoin join);
    GraphicsLineJoin CurrentLineJoin() const;

    void SetMiterLimit(gfxFloat limit);
    gfxFloat CurrentMiterLimit() const;

    /**
     ** Fill Properties
     **/

    enum FillRule {
        FILL_RULE_WINDING,
        FILL_RULE_EVEN_ODD
    };
    void SetFillRule(FillRule rule);
    FillRule CurrentFillRule() const;

    /**
     ** Operators and Rendering control
     **/

    // define enum for operators (clear, src, dst, etc)
    enum GraphicsOperator {
        OPERATOR_CLEAR,
        OPERATOR_SOURCE,

        OPERATOR_OVER,
        OPERATOR_IN,
        OPERATOR_OUT,
        OPERATOR_ATOP,

        OPERATOR_DEST,
        OPERATOR_DEST_OVER,
        OPERATOR_DEST_IN,
        OPERATOR_DEST_OUT,
        OPERATOR_DEST_ATOP,

        OPERATOR_XOR,
        OPERATOR_ADD,
        OPERATOR_SATURATE
    };
    /**
     * Sets the operator used for all further drawing. The operator affects
     * how drawing something will modify the destination. For example, the
     * OVER operator will do alpha blending of source and destination, while
     * SOURCE will replace the destination with the source.
     */
    void SetOperator(GraphicsOperator op);
    GraphicsOperator CurrentOperator() const;

    /**
     * MODE_ALIASED means that only pixels whose centers are in the drawn area
     * should be modified, and they should be modified to take the value drawn
     * at the pixel center.
     */
    enum AntialiasMode {
        MODE_ALIASED,
        MODE_COVERAGE
    };
    void SetAntialiasMode(AntialiasMode mode);
    AntialiasMode CurrentAntialiasMode() const;

    /**
     ** Clipping
     **/

    /**
     * Clips all further drawing to the current path.
     * This does not consume the current path.
     */
    void Clip();

    /**
     * Undoes any clipping. Further drawings will only be restricted by the
     * surface dimensions.
     */
    void ResetClip();

    /**
     * Helper functions that will create a rect path and call Clip().
     * Any current path will be destroyed by these functions!
     */
    void Clip(gfxRect rect); // will clip to a rect

    /**
     * This will ensure that the surface actually has its clip set.
     * Useful if you are doing native drawing.
     */
    void UpdateSurfaceClip();
    
    /**
     * Groups
     */
    void PushGroup(gfxASurface::gfxContentType content = gfxASurface::CONTENT_COLOR);
    already_AddRefed<gfxPattern> PopGroup();
    void PopGroupToSource();

    /**
     * Printing functions
     */
    // XXX look and see if the arguments here should be a separate object
    void BeginPrinting(const nsAString& aTitle, const nsAString& aPrintToFileName);
    void EndPrinting();
    void AbortPrinting();
    void BeginPage();
    void EndPage();

    /**
     ** Hit Testing - check if given point is in the current path
     **/
    PRBool PointInFill(gfxPoint pt);
    PRBool PointInStroke(gfxPoint pt);

    /**
     ** Extents - returns user space extent of current path
     **/
    gfxRect GetUserFillExtent();
    gfxRect GetUserStrokeExtent();

private:
    cairo_t *mCairo;
    nsRefPtr<gfxASurface> mSurface;
};

#endif /* GFX_CONTEXT_H */
