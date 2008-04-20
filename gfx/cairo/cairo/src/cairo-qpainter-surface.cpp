/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2008 Mozilla Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 *
 * Contributor(s):
 *      Vladimir Vukicevic <vladimir@mozilla.com>
 */

/* Get INT16_MIN etc. as per C99 */
#define __STDC_LIMIT_MACROS

#include "cairoint.h"

#include "cairo-qpainter.h"

#include <QtGui/QPainter>
#include <QtGui/QPaintEngine>
#include <QtGui/QPaintDevice>
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <QtGui/QBrush>
#include <QtGui/QPen>
#include <QtGui/QWidget>
#include <QtGui/QX11Info>

#ifdef CAIRO_HAS_XLIB_XRENDER_SURFACE
#include "cairo-xlib.h"
#include "cairo-xlib-xrender.h"
// I hate X
#undef Status
#undef CursorShape
#undef Bool
#endif

#include <sys/time.h>

#if 0
#define D(x)  x
#else
#define D(x) do { } while(0)
#endif

#if 0
#define DP(x) if (g_dump_path) { x; }
#else
#define DP(x) do { } while(0)
#endif

#ifndef CAIRO_INT_STATUS_SUCCESS
#define CAIRO_INT_STATUS_SUCCESS ((cairo_int_status_t) CAIRO_STATUS_SUCCESS)
#endif

typedef struct {
    cairo_surface_t base;

    QPainter *p;

    /* The pixmap/image constructors will store their objects here */
    QPixmap *pixmap;
    QImage *image;

    QRect window;

    bool has_clipping;
    QRect clip_bounds;

    cairo_image_surface_t *image_equiv;

#if defined(Q_WS_X11) && defined(CAIRO_HAS_XLIB_XRENDER_SURFACE)
    /* temporary, so that we can share the xlib surface's glyphs code */
    cairo_surface_t *xlib_equiv;
    bool xlib_has_clipping;
    QRect xlib_clip_bounds;
    int xlib_clip_serial;
    QPoint redir_offset;
#endif

    cairo_bool_t supports_porter_duff;
} cairo_qpainter_surface_t;

/* Will be set to TRUE if we ever try to create a QPixmap and end
 * up with one without an alpha channel.
 */
static cairo_bool_t _qpixmaps_have_no_alpha = FALSE;

static cairo_int_status_t
_cairo_qpainter_surface_paint (void *abstract_surface,
                               cairo_operator_t op,
                               cairo_pattern_t *source);

/* some debug timing stuff */
static int g_dump_path = 1;
static struct timeval timer_start_val;
#if 0
static void tstart() {
    gettimeofday(&timer_start_val, NULL);
}

static void tend(const char *what = NULL) {
    struct timeval timer_stop;
    gettimeofday(&timer_stop, NULL);

    double ms_start = timer_start_val.tv_sec * 1000.0 + timer_start_val.tv_usec / 1000.0;
    double ms_end = timer_stop.tv_sec * 1000.0 + timer_stop.tv_usec / 1000.0;

    if (ms_end - ms_start > 1.0)
	fprintf (stderr, "******* %s elapsed: %f ms\n", what ? what : "(timer)", ms_end - ms_start);
}
#else
static void tstart() { }
static void tend(const char *what = NULL) { }
#endif

/**
 ** Helper methods
 **/
static const char *
_opstr (cairo_operator_t op)
{
    const char *ops[] = {
        "CLEAR",
        "SOURCE",
        "OVER",
        "IN",
        "OUT",
        "ATOP",
        "DEST",
        "DEST_OVER",
        "DEST_IN",
        "DEST_OUT",
        "DEST_ATOP",
        "XOR",
        "ADD",
        "SATURATE"
    };

    if (op < CAIRO_OPERATOR_CLEAR || op > CAIRO_OPERATOR_SATURATE)
        return "(\?\?\?)";

    return ops[op];
}

static QPainter::CompositionMode
_qpainter_compositionmode_from_cairo_op (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
        return QPainter::CompositionMode_Clear;

    case CAIRO_OPERATOR_SOURCE:
        return QPainter::CompositionMode_Source;
    case CAIRO_OPERATOR_OVER:
        return QPainter::CompositionMode_SourceOver;
    case CAIRO_OPERATOR_IN:
        return QPainter::CompositionMode_SourceIn;
    case CAIRO_OPERATOR_OUT:
        return QPainter::CompositionMode_SourceOut;
    case CAIRO_OPERATOR_ATOP:
        return QPainter::CompositionMode_SourceAtop;

    case CAIRO_OPERATOR_DEST:
        return QPainter::CompositionMode_Destination;
    case CAIRO_OPERATOR_DEST_OVER:
        return QPainter::CompositionMode_DestinationOver;
    case CAIRO_OPERATOR_DEST_IN:
        return QPainter::CompositionMode_DestinationIn;
    case CAIRO_OPERATOR_DEST_OUT:
        return QPainter::CompositionMode_DestinationOut;
    case CAIRO_OPERATOR_DEST_ATOP:
        return QPainter::CompositionMode_DestinationAtop;

    case CAIRO_OPERATOR_XOR:
        return QPainter::CompositionMode_Xor;
    case CAIRO_OPERATOR_ADD:
        return QPainter::CompositionMode_SourceOver; // XXX?
    case CAIRO_OPERATOR_SATURATE:
        return QPainter::CompositionMode_SourceOver; // XXX?
    }

    return QPainter::CompositionMode_Source;
}

static cairo_format_t
_cairo_format_from_qimage_format (QImage::Format fmt)
{
    switch (fmt) {
    case QImage::Format_ARGB32_Premultiplied:
        return CAIRO_FORMAT_ARGB32;
    case QImage::Format_RGB32:
        return CAIRO_FORMAT_RGB24;
    case QImage::Format_Indexed8: // XXX not quite
        return CAIRO_FORMAT_A8;
#ifdef WORDS_BIGENDIAN
    case QImage::Format_Mono:
#else
    case QImage::Format_MonoLSB:
#endif
        return CAIRO_FORMAT_A1;
    default:
        return CAIRO_FORMAT_A1;
    }

    return CAIRO_FORMAT_A1;
}

static QImage::Format
_qimage_format_from_cairo_format (cairo_format_t fmt)
{
    switch (fmt) {
    case CAIRO_FORMAT_ARGB32:
        return QImage::Format_ARGB32_Premultiplied;
    case CAIRO_FORMAT_RGB24:
        return QImage::Format_RGB32;
    case CAIRO_FORMAT_A8:
        return QImage::Format_Indexed8; // XXX not quite
    case CAIRO_FORMAT_A1:
#ifdef WORDS_BIGENDIAN
        return QImage::Format_Mono; // XXX think we need to choose between this and LSB
#else
        return QImage::Format_MonoLSB;
#endif
    }

    return QImage::Format_Mono;
}

static inline QMatrix
_qmatrix_from_cairo_matrix (const cairo_matrix_t& m)
{
    return QMatrix(m.xx, m.xy, m.yx, m.yy, m.x0, m.y0);
}

static inline void
_qmatrix_from_cairo_matrix (const cairo_matrix_t& m, QMatrix& qm)
{
    qm.setMatrix(m.xx, m.xy, m.yx, m.yy, m.x0, m.y0);
}

/** Path conversion **/
typedef struct _qpainter_path_transform {
    QPainterPath *path;
    cairo_matrix_t *ctm_inverse;
} qpainter_path_data;

/* cairo path -> execute in context */
static cairo_status_t
_cairo_path_to_qpainterpath_move_to (void *closure, cairo_point_t *point)
{
    qpainter_path_data *pdata = (qpainter_path_data *)closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (pdata->ctm_inverse)
        cairo_matrix_transform_point (pdata->ctm_inverse, &x, &y);

    DP(fprintf(stderr, "moveto %4.2f %4.2f\n", x, y));

    pdata->path->moveTo(x, y);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_qpainterpath_line_to (void *closure, cairo_point_t *point)
{
    qpainter_path_data *pdata = (qpainter_path_data *)closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (pdata->ctm_inverse)
        cairo_matrix_transform_point (pdata->ctm_inverse, &x, &y);

    pdata->path->lineTo(x, y);

    if (pdata->path->isEmpty())
        pdata->path->moveTo(x, y);
    else
        pdata->path->lineTo(x, y);

    DP(fprintf(stderr, "lineto %4.2f %4.2f\n", x, y));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_qpainterpath_curve_to (void *closure, cairo_point_t *p0, cairo_point_t *p1, cairo_point_t *p2)
{
    qpainter_path_data *pdata = (qpainter_path_data *)closure;
    double x0 = _cairo_fixed_to_double (p0->x);
    double y0 = _cairo_fixed_to_double (p0->y);
    double x1 = _cairo_fixed_to_double (p1->x);
    double y1 = _cairo_fixed_to_double (p1->y);
    double x2 = _cairo_fixed_to_double (p2->x);
    double y2 = _cairo_fixed_to_double (p2->y);

    if (pdata->ctm_inverse) {
        cairo_matrix_transform_point (pdata->ctm_inverse, &x0, &y0);
        cairo_matrix_transform_point (pdata->ctm_inverse, &x1, &y1);
        cairo_matrix_transform_point (pdata->ctm_inverse, &x2, &y2);
    }

    pdata->path->cubicTo (x0, y0, x1, y1, x2, y2);

    DP(fprintf(stderr, "curveto %f %f %f %f %f %f\n", x0, y0, x1, y1, x2, y2));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_qpainterpath_close_path (void *closure)
{
    qpainter_path_data *pdata = (qpainter_path_data *)closure;

    pdata->path->closeSubpath();

    DP(fprintf(stderr, "closepath\n"));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartz_cairo_path_to_qpainterpath (cairo_path_fixed_t *path,
                                          QPainterPath *qpath,
                                          cairo_fill_rule_t fill_rule,
                                          cairo_matrix_t *ctm_inverse = NULL)
{
    qpainter_path_data pdata = { qpath, ctm_inverse };

    qpath->setFillRule (fill_rule == CAIRO_FILL_RULE_WINDING ? Qt::WindingFill : Qt::OddEvenFill);

    return _cairo_path_fixed_interpret (path,
                                        CAIRO_DIRECTION_FORWARD,
                                        _cairo_path_to_qpainterpath_move_to,
                                        _cairo_path_to_qpainterpath_line_to,
                                        _cairo_path_to_qpainterpath_curve_to,
                                        _cairo_path_to_qpainterpath_close_path,
                                        &pdata);
}

static cairo_status_t
_cairo_quartz_cairo_path_to_qpainterpath (cairo_path_fixed_t *path,
        QPainterPath *qpath,
        cairo_matrix_t *ctm_inverse)
{
    return _cairo_quartz_cairo_path_to_qpainterpath (path, qpath,
            CAIRO_FILL_RULE_WINDING,
            ctm_inverse);
}

/**
 ** Surface backend methods
 **/
static cairo_surface_t *
_cairo_qpainter_surface_create_similar (void *abstract_surface,
                                        cairo_content_t content,
                                        int width,
                                        int height)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;

    cairo_bool_t do_image = qs->image != NULL;

    D(fprintf(stderr, "q[%p] create_similar: %d %d [%d] -> ", abstract_surface, width, height, content));

    if (!do_image && (!_qpixmaps_have_no_alpha || content == CAIRO_CONTENT_COLOR)) {
	cairo_surface_t *result =
	    cairo_qpainter_surface_create_with_qpixmap (width, height);

	if (cairo_surface_get_content (result) == content) {
	    D(fprintf(stderr, "qpixmap\n"));
	    return result;
	}

	fprintf (stderr, " pixmaps have no alpha!\n");
	_qpixmaps_have_no_alpha = TRUE;
	cairo_surface_destroy (result);
    }

    D(fprintf (stderr, "qimage\n"));
    return cairo_qpainter_surface_create_with_qimage
	(_cairo_format_from_content (content), width, height);
}

static cairo_status_t
_cairo_qpainter_surface_finish (void *abstract_surface)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] finish\n", abstract_surface));

    /* Only delete p if we created it */
    if (qs->image || qs->pixmap)
        delete qs->p;
    else
        qs->p->restore();

    if (qs->image_equiv)
        cairo_surface_destroy ((cairo_surface_t*) qs->image_equiv);

#if defined(Q_WS_X11) && defined(CAIRO_HAS_XLIB_XRENDER_SURFACE)
    if (qs->xlib_equiv)
        cairo_surface_destroy (qs->xlib_equiv);
#endif

    if (qs->image)
        delete qs->image;

    if (qs->pixmap)
        delete qs->pixmap;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_qpainter_surface_acquire_source_image (void *abstract_surface,
                                              cairo_image_surface_t **image_out,
                                              void **image_extra)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] acquire_source_image\n", abstract_surface));

    *image_extra = NULL;

    if (qs->image_equiv) {
        *image_out = (cairo_image_surface_t*)
                     cairo_surface_reference ((cairo_surface_t*) qs->image_equiv);

        return CAIRO_STATUS_SUCCESS;
    }

    if (qs->pixmap) {
        QImage *qimg = new QImage(qs->pixmap->toImage());

        *image_out = (cairo_image_surface_t*)
                     cairo_image_surface_create_for_data (qimg->bits(),
                                                          _cairo_format_from_qimage_format (qimg->format()),
                                                          qimg->width(), qimg->height(),
                                                          qimg->bytesPerLine());
        *image_extra = qimg;

        return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_STATUS_NO_MEMORY;
}

static void
_cairo_qpainter_surface_release_source_image (void *abstract_surface,
                                              cairo_image_surface_t *image,
                                              void *image_extra)
{
    //cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] release_source_image\n", abstract_surface));

    cairo_surface_destroy ((cairo_surface_t*) image);

    if (image_extra) {
        QImage *qimg = (QImage *) image_extra;
        delete qimg;
    }
}

static cairo_status_t
_cairo_qpainter_surface_acquire_dest_image (void *abstract_surface,
                                            cairo_rectangle_int_t *interest_rect,
                                            cairo_image_surface_t **image_out,
                                            cairo_rectangle_int_t *image_rect,
                                            void **image_extra)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;
    QImage *qimg = NULL;

    D(fprintf(stderr, "q[%p] acquire_dest_image\n", abstract_surface));

    *image_extra = NULL;

    if (qs->image_equiv) {
        *image_out = (cairo_image_surface_t*)
                     cairo_surface_reference ((cairo_surface_t*) qs->image_equiv);

        image_rect->x = qs->window.x();
        image_rect->y = qs->window.y();
        image_rect->width = qs->window.width();
        image_rect->height = qs->window.height();

        return CAIRO_STATUS_SUCCESS;
    }

    if (qs->pixmap) {
        qimg = new QImage(qs->pixmap->toImage());
    } else {
        // Try to figure out what kind of QPaintDevice we have, and
        // how we can grab an image from it
        QPaintDevice *pd = qs->p->device();
	if (!pd)
	    return CAIRO_STATUS_NO_MEMORY;

        if (pd->devType() == QInternal::Image) {
            qimg = new QImage(((QImage*) pd)->copy());
        } else if (pd->devType() == QInternal::Pixmap) {
            qimg = new QImage(((QPixmap*) pd)->toImage());
        } else if (pd->devType() == QInternal::Widget) {
            qimg = new QImage(QPixmap::grabWindow(((QWidget*)pd)->winId()).toImage());
        }
    }

    if (qimg == NULL)
        return CAIRO_STATUS_NO_MEMORY;

    *image_out = (cairo_image_surface_t*)
                 cairo_image_surface_create_for_data (qimg->bits(),
                                                      _cairo_format_from_qimage_format (qimg->format()),
                                                      qimg->width(), qimg->height(),
                                                      qimg->bytesPerLine());
    *image_extra = qimg;

    image_rect->x = qs->window.x();
    image_rect->y = qs->window.y();
    image_rect->width = qs->window.width();
    image_rect->height = qs->window.height();

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_qpainter_surface_release_dest_image (void *abstract_surface,
        cairo_rectangle_int_t *interest_rect,
        cairo_image_surface_t *image,
        cairo_rectangle_int_t *image_rect,
        void *image_extra)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;
    D(fprintf(stderr, "q[%p] release_dest_image\n", abstract_surface));

    cairo_surface_destroy ((cairo_surface_t*) image);

    if (image_extra) {
        QImage *qimg = (QImage*) image_extra;

        // XXX should I be using setBackgroundMode here instead of setCompositionMode?
        if (qs->supports_porter_duff)
            qs->p->setCompositionMode (QPainter::CompositionMode_Source);

        qs->p->drawImage (image_rect->x, image_rect->y, *qimg);

        if (qs->supports_porter_duff)
            qs->p->setCompositionMode (QPainter::CompositionMode_SourceOver);

        delete qimg;
    }
}

static cairo_status_t
_cairo_qpainter_surface_clone_similar (void *abstract_surface,
                                       cairo_surface_t *src,
                                       int              src_x,
                                       int              src_y,
                                       int              width,
                                       int              height,
                                       cairo_surface_t **clone_out)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;
    cairo_surface_t *new_surf = NULL;

    // For non-image targets, always try to create a QPixmap first
    if (qs->image == NULL && (!_qpixmaps_have_no_alpha || src->content == CAIRO_CONTENT_COLOR))
    {
        new_surf = cairo_qpainter_surface_create_with_qpixmap
                   (width, height);
	if (cairo_surface_get_content (new_surf) != src->content) {
	    cairo_surface_destroy (new_surf);
	    _qpixmaps_have_no_alpha = TRUE;
	    new_surf = NULL;
	}
    }

    if (new_surf == NULL) {
        new_surf = cairo_qpainter_surface_create_with_qimage
                   (_cairo_format_from_content (src->content), width, height);
    }

    if (new_surf->status)
        return new_surf->status;

    cairo_pattern_union_t upat;
    _cairo_pattern_init_for_surface (&upat.surface, src);

    cairo_matrix_t tx;
    cairo_matrix_init_translate (&tx, -src_x, -src_y);
    cairo_pattern_set_matrix (&upat.base, &tx);

    cairo_int_status_t status =
        _cairo_qpainter_surface_paint (new_surf, CAIRO_OPERATOR_SOURCE, &upat.base);

    _cairo_pattern_fini (&upat.base);

    if (status) {
        cairo_surface_destroy (new_surf);
        new_surf = NULL;
    }

    *clone_out = new_surf;
    return (cairo_status_t) status;
}

static cairo_int_status_t
_cairo_qpainter_surface_get_extents (void *abstract_surface,
                                     cairo_rectangle_int_t *extents)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;

    extents->x = qs->window.x();
    extents->y = qs->window.y();
    extents->width = qs->window.width();
    extents->height = qs->window.height();

    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_qpainter_surface_intersect_clip_path (void *abstract_surface,
        cairo_path_fixed_t *path,
        cairo_fill_rule_t fill_rule,
        double tolerance,
        cairo_antialias_t antialias)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] intersect_clip_path %s\n", abstract_surface, path ? "(path)" : "(clear)"));

    if (!qs->p)
        return CAIRO_INT_STATUS_SUCCESS;

    if (path == NULL) {
	//fprintf (stderr, "clip clear\n");
        // How the clip path is reset depends on whether we own p or not
        if (qs->pixmap || qs->image) {
            // we own p
            qs->p->setClipping (false);
        } else {
            qs->p->restore ();
            qs->p->save ();
        }

	qs->clip_bounds.setRect(0, 0, 0, 0);
	qs->has_clipping = false;

        return CAIRO_INT_STATUS_SUCCESS;
    }

    // Qt will implicity enable clipping, and will use ReplaceClip
    // instead of IntersectClip if clipping was disabled before

    // Note: Qt is really bad at dealing with clip paths.  It doesn't
    // seem to usefully recognize rectangular paths, instead going down
    // extremely slow paths whenever a clip path is set.  So,
    // we do a bunch of work here to try to get rectangles or regions
    // down to Qt for clipping.

    tstart();

    QRect clip_bounds;

    // First check if it's an integer-aligned single rectangle
    cairo_box_t box;
    if (_cairo_path_fixed_is_box (path, &box) &&
	_cairo_fixed_is_integer (box.p1.x) &&
	_cairo_fixed_is_integer (box.p1.y) &&
	_cairo_fixed_is_integer (box.p2.x) &&
	_cairo_fixed_is_integer (box.p2.y))
    {
	QRect r(_cairo_fixed_integer_part(box.p1.x),
		_cairo_fixed_integer_part(box.p1.y),
		_cairo_fixed_integer_part(box.p2.x - box.p1.x),
		_cairo_fixed_integer_part(box.p2.y - box.p1.y));

	r = r.normalized();

	DP(fprintf (stderr, "clip rect: %d %d %d %d\n", r.x(), r.y(), r.width(), r.height()));

	clip_bounds = r;

	qs->p->setClipRect (r, Qt::IntersectClip);
    } else {
	// Then if it's not an integer-aligned rectangle, check
	// if we can extract a region (a set of rectangles) out.
	// We use cairo to convert the path to traps.

	cairo_traps_t traps;
	cairo_int_status_t status;
	cairo_region_t region;

	_cairo_traps_init (&traps);
	status = (cairo_int_status_t)
	    _cairo_path_fixed_fill_to_traps (path, fill_rule, tolerance, &traps);
	if (status) {
	    _cairo_traps_fini (&traps);
	    return status;
	}

	status = _cairo_traps_extract_region (&traps, &region);
	_cairo_traps_fini (&traps);

	if (status == CAIRO_INT_STATUS_SUCCESS) {
	    cairo_box_int_t *boxes;
	    int n_boxes;

	    QRegion qr;

	    _cairo_region_get_boxes (&region, &n_boxes, &boxes);

	    for (int i = 0; i < n_boxes; i++) {
		QRect r(boxes[i].p1.x,
			boxes[i].p1.y,
			boxes[i].p2.x - boxes[i].p1.x,
			boxes[i].p2.y - boxes[i].p1.y);

		if (i == 0)
		    clip_bounds = r;
		else
		    clip_bounds = clip_bounds.united(r);

		qr = qr.unite(r);
	    }

	    _cairo_region_boxes_fini (&region, boxes);
	    _cairo_region_fini (&region);

	    qs->p->setClipRegion (qr, Qt::IntersectClip);
	} else {
	    // We weren't able to extract a region from the traps.
	    // Just hand the path down to QPainter.
	    QPainterPath qpath;

	    if (_cairo_quartz_cairo_path_to_qpainterpath (path, &qpath, fill_rule) != CAIRO_STATUS_SUCCESS)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	    clip_bounds = qpath.boundingRect().toAlignedRect();

	    // XXX Antialiasing is ignored
	    qs->p->setClipPath (qpath, Qt::IntersectClip);
	}
    }

    clip_bounds = qs->p->worldTransform().mapRect(clip_bounds);

    if (qs->has_clipping) {
	qs->clip_bounds = qs->clip_bounds.intersect(clip_bounds);
    } else {
	qs->clip_bounds = clip_bounds;
	qs->has_clipping = true;
    }

    tend("clip");

    return CAIRO_INT_STATUS_SUCCESS;
}

/**
 ** Brush conversion
 **/

QBrush *
_qbrush_from_pattern (cairo_pattern_t *pattern)
{
    QBrush *brush = NULL;

    if (pattern->type == CAIRO_PATTERN_TYPE_SOLID) {
        cairo_solid_pattern_t *solid = (cairo_solid_pattern_t*) pattern;
        QColor color;
        color.setRgbF(solid->color.red,
                      solid->color.green,
                      solid->color.blue,
                      solid->color.alpha);

        brush = new QBrush(color);
    } else if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
        cairo_surface_pattern_t *spattern = (cairo_surface_pattern_t*) pattern;
        cairo_surface_t *surface = spattern->surface;

        if (surface->type == CAIRO_SURFACE_TYPE_QPAINTER) {
            cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t*) surface;

            if (qs->image) {
                brush = new QBrush(*qs->image);
            } else if (qs->pixmap) {
                brush = new QBrush(*qs->pixmap);
            } else {
                // do something smart
                brush = new QBrush(0xff0000ff);
            }
        } else if (surface->type == CAIRO_SURFACE_TYPE_IMAGE) {
            cairo_image_surface_t *isurf = (cairo_image_surface_t*) surface;
            brush = new QBrush (QImage ((const uchar *) isurf->data,
                                        isurf->width,
                                        isurf->height,
                                        isurf->stride,
                                        _qimage_format_from_cairo_format (isurf->format)));
        } else {
            brush = new QBrush(0x0000ffff);
        }

    } else if (pattern->type == CAIRO_PATTERN_TYPE_LINEAR ||
               pattern->type == CAIRO_PATTERN_TYPE_RADIAL)
    {
        brush = new QBrush(0x00ff00ff);
    }

    if (brush &&
            pattern->type != CAIRO_PATTERN_TYPE_SOLID &&
            !_cairo_matrix_is_identity(&pattern->matrix))
    {
        cairo_matrix_t pm = pattern->matrix;
        if (cairo_matrix_invert (&pm) == CAIRO_STATUS_SUCCESS)
            brush->setMatrix(_qmatrix_from_cairo_matrix (pm));
    }

    return brush;
}

QPen *
_qpen_from_pattern_and_style (cairo_pattern_t *source,
                              cairo_stroke_style_t *style)
{
    QPen *pen;
    QBrush *brush;

    brush = _qbrush_from_pattern (source);

    Qt::PenJoinStyle join = Qt::MiterJoin;
    Qt::PenCapStyle cap = Qt::SquareCap;

    switch (style->line_cap) {
    case CAIRO_LINE_CAP_BUTT:
        cap = Qt::FlatCap;
        break;
    case CAIRO_LINE_CAP_ROUND:
        cap = Qt::RoundCap;
        break;
    case CAIRO_LINE_CAP_SQUARE:
        cap = Qt::SquareCap;
        break;
    }

    switch (style->line_join) {
    case CAIRO_LINE_JOIN_MITER:
        join = Qt::MiterJoin;
        break;
    case CAIRO_LINE_JOIN_ROUND:
        join = Qt::RoundJoin;
        break;
    case CAIRO_LINE_JOIN_BEVEL:
        join = Qt::BevelJoin;
        break;
    }

    pen = new QPen (*brush, style->line_width, Qt::SolidLine, cap, join);
    pen->setMiterLimit (style->miter_limit);

    if (style->dash && style->num_dashes) {
	unsigned int odd_dash = style->num_dashes % 2;

	QVector<qreal> dashes (odd_dash ? style->num_dashes * 2 : style->num_dashes);
	for (unsigned int i = 0; i < odd_dash+1; i++) {
	    for (unsigned int j = 0; j < style->num_dashes; j++) {
		// In Qt, the dash lengths are given in units of line width, whereas
		// in cairo, they are in user-space units.  We'll always apply the CTM,
		// so all we have to do here is divide cairo's dash lengths by the line
		// width.
		dashes.append (style->dash[j] / style->line_width);
	    }
	}

	pen->setDashPattern (dashes);
	pen->setDashOffset (style->dash_offset / style->line_width);
    }

    delete brush;

    return pen;
}

/**
 ** Core drawing operations
 **/

cairo_int_status_t
_cairo_qpainter_surface_paint (void *abstract_surface,
                               cairo_operator_t op,
                               cairo_pattern_t *source)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] paint op:%s\n", abstract_surface, _opstr(op)));

    if (!qs->p)
        return CAIRO_INT_STATUS_SUCCESS;

    if (qs->supports_porter_duff)
        qs->p->setCompositionMode (_qpainter_compositionmode_from_cairo_op (op));

    if (source->type == CAIRO_PATTERN_TYPE_SURFACE &&
            source->extend == CAIRO_EXTEND_NONE &&
            ((cairo_surface_pattern_t*)source)->surface->type == CAIRO_SURFACE_TYPE_QPAINTER)
    {
        cairo_qpainter_surface_t *qsSrc = (cairo_qpainter_surface_t*) ((cairo_surface_pattern_t*)source)->surface;

        QMatrix savedMatrix = qs->p->worldMatrix();
	cairo_matrix_t m = source->matrix;
	cairo_matrix_invert (&m);
        qs->p->setWorldMatrix (_qmatrix_from_cairo_matrix (m), true);

        if (qsSrc->image) {
            qs->p->drawImage (0, 0, *qsSrc->image);
        } else if (qsSrc->pixmap) {
            qs->p->drawPixmap (0, 0, *qsSrc->pixmap);
        }

        qs->p->setWorldMatrix (savedMatrix, false);
    } else {
        QBrush *brush = _qbrush_from_pattern (source);

        qs->p->fillRect (qs->window, *brush);

        delete brush;
    }

    if (qs->supports_porter_duff)
        qs->p->setCompositionMode (QPainter::CompositionMode_SourceOver);

    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_qpainter_surface_fill (void *abstract_surface,
                              cairo_operator_t op,
                              cairo_pattern_t *source,
                              cairo_path_fixed_t *path,
                              cairo_fill_rule_t fill_rule,
                              double tolerance,
                              cairo_antialias_t antialias)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] fill op:%s\n", abstract_surface, _opstr(op)));

    if (!qs->p)
        return CAIRO_INT_STATUS_SUCCESS;

    tstart();

    QPainterPath qpath;
    if (_cairo_quartz_cairo_path_to_qpainterpath (path, &qpath, fill_rule) != CAIRO_STATUS_SUCCESS)
        return CAIRO_INT_STATUS_UNSUPPORTED;

    if (qs->supports_porter_duff)
        qs->p->setCompositionMode (_qpainter_compositionmode_from_cairo_op (op));

    // XXX Qt4.3, 4.4 misrenders some complex paths if antialiasing is
    // enabled
    //qs->p->setRenderHint (QPainter::Antialiasing, antialias == CAIRO_ANTIALIAS_NONE ? false : true);
    qs->p->setRenderHint (QPainter::SmoothPixmapTransform, source->filter != CAIRO_FILTER_FAST);

    QBrush *brush = _qbrush_from_pattern (source);

    qs->p->fillPath (qpath, *brush);

    delete brush;

    if (qs->supports_porter_duff)
        qs->p->setCompositionMode (QPainter::CompositionMode_SourceOver);

    tend("fill");

    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_qpainter_surface_stroke (void *abstract_surface,
                                cairo_operator_t op,
                                cairo_pattern_t *source,
                                cairo_path_fixed_t *path,
                                cairo_stroke_style_t *style,
                                cairo_matrix_t *ctm,
                                cairo_matrix_t *ctm_inverse,
                                double tolerance,
                                cairo_antialias_t antialias)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] stroke op:%s\n", abstract_surface, _opstr(op)));

    if (!qs->p)
        return CAIRO_INT_STATUS_SUCCESS;

    QPainterPath qpath;
    if (_cairo_quartz_cairo_path_to_qpainterpath (path, &qpath, ctm_inverse) != CAIRO_STATUS_SUCCESS)
        return CAIRO_INT_STATUS_UNSUPPORTED;

    QMatrix savedMatrix = qs->p->worldMatrix();

    if (qs->supports_porter_duff)
        qs->p->setCompositionMode (_qpainter_compositionmode_from_cairo_op (op));

    qs->p->setWorldMatrix (_qmatrix_from_cairo_matrix (*ctm), true);
    // XXX Qt4.3, 4.4 misrenders some complex paths if antialiasing is
    // enabled
    //qs->p->setRenderHint (QPainter::Antialiasing, antialias == CAIRO_ANTIALIAS_NONE ? false : true);
    qs->p->setRenderHint (QPainter::SmoothPixmapTransform, source->filter != CAIRO_FILTER_FAST);

    QPen *pen = _qpen_from_pattern_and_style (source, style);

    qs->p->setPen(*pen);
    qs->p->drawPath (qpath);
    qs->p->setPen(Qt::black);

    delete pen;

    qs->p->setWorldMatrix (savedMatrix, false);

    if (qs->supports_porter_duff)
        qs->p->setCompositionMode (QPainter::CompositionMode_SourceOver);

    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_qpainter_surface_show_glyphs (void *abstract_surface,
                                     cairo_operator_t op,
                                     cairo_pattern_t *source,
                                     cairo_glyph_t *glyphs,
                                     int num_glyphs,
                                     cairo_scaled_font_t *scaled_font)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;

#if defined(Q_WS_X11) && defined(CAIRO_HAS_XLIB_XRENDER_SURFACE)
    /* If we have an equivalent X surface, let the xlib surface handle this
     * until we figure out how to do this natively with Qt.
     */
    if (qs->xlib_equiv) {

	D(fprintf(stderr, "q[%p] show_glyphs (x11 equiv) op:%s nglyphs: %d\n", abstract_surface, _opstr(op), num_glyphs));

	for (int i = 0; i < num_glyphs; i++) {
	    glyphs[i].x -= qs->redir_offset.x();
	    glyphs[i].y -= qs->redir_offset.y();
	}

	if (qs->has_clipping != qs->xlib_has_clipping ||
	    qs->clip_bounds != qs->xlib_clip_bounds)
	{
	    _cairo_surface_reset_clip (qs->xlib_equiv);

	    if (qs->has_clipping) {
		cairo_region_t region;
		cairo_rectangle_int_t rect = {
		    qs->clip_bounds.x() - qs->redir_offset.x(),
		    qs->clip_bounds.y() - qs->redir_offset.y(),
		    qs->clip_bounds.width(),
		    qs->clip_bounds.height()
		};

		_cairo_region_init_rect (&region, &rect);

		_cairo_surface_set_clip_region (qs->xlib_equiv, &region, ++qs->xlib_clip_serial);

		_cairo_region_fini (&region);
	    }

	    qs->xlib_has_clipping = qs->has_clipping;
	    qs->xlib_clip_bounds = qs->clip_bounds;
	}

        return (cairo_int_status_t)
               _cairo_surface_show_glyphs (qs->xlib_equiv, op, source, glyphs, num_glyphs, scaled_font);
    }
#endif

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_qpainter_surface_mask (void *abstract_surface,
                              cairo_operator_t op,
                              cairo_pattern_t *source,
                              cairo_pattern_t *mask)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] mask op:%s\n", abstract_surface, _opstr(op)));

    if (!qs->p)
        return CAIRO_INT_STATUS_SUCCESS;

    if (mask->type == CAIRO_PATTERN_TYPE_SOLID) {
        cairo_solid_pattern_t *solid_mask = (cairo_solid_pattern_t *) mask;
        cairo_int_status_t result;

        qs->p->setOpacity (solid_mask->color.alpha);

        result = _cairo_qpainter_surface_paint (abstract_surface, op, source);

        qs->p->setOpacity (1.0);

        return result;
    }

    // otherwise skip for now
    return CAIRO_INT_STATUS_SUCCESS;
}

/**
 ** XXX this will go away!  it's only implemented for now so that we
 ** can get some text without show_glyphs being available.
 **/
static cairo_int_status_t
_cairo_qpainter_surface_composite (cairo_operator_t op,
                                   cairo_pattern_t *pattern,
                                   cairo_pattern_t *mask_pattern,
                                   void *abstract_surface,
                                   int src_x,
                                   int src_y,
                                   int mask_x,
                                   int mask_y,
                                   int dst_x,
                                   int dst_y,
                                   unsigned int width,
                                   unsigned int height)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t *) abstract_surface;

    if (mask_pattern)
        return CAIRO_INT_STATUS_UNSUPPORTED;

    D(fprintf(stderr, "q[%p] composite op:%s src:%p [%d %d] dst [%d %d] dim [%d %d]\n",
              abstract_surface, _opstr(op), (void*)pattern,
              src_x, src_y, dst_x, dst_y, width, height));

    if (pattern->type == CAIRO_PATTERN_TYPE_SOLID) {
        cairo_solid_pattern_t *solid = (cairo_solid_pattern_t*) pattern;

        QColor color;
        color.setRgbF(solid->color.red,
                      solid->color.green,
                      solid->color.blue,
                      solid->color.alpha);

        if (qs->supports_porter_duff)
            qs->p->setCompositionMode (_qpainter_compositionmode_from_cairo_op (op));

        qs->p->fillRect (dst_x, dst_y, width, height, color);

        if (qs->supports_porter_duff)
            qs->p->setCompositionMode (QPainter::CompositionMode_SourceOver);
    } else if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
        cairo_surface_pattern_t *spattern = (cairo_surface_pattern_t*) pattern;
        cairo_surface_t *surface = spattern->surface;

        QImage *qimg = NULL;
        QPixmap *qpixmap = NULL;
        cairo_bool_t delete_qimg = FALSE;

        if (surface->type == CAIRO_SURFACE_TYPE_IMAGE) {
            cairo_image_surface_t *isurf = (cairo_image_surface_t*) surface;
            qimg = new QImage ((const uchar *) isurf->data,
                               isurf->width,
                               isurf->height,
                               isurf->stride,
                               _qimage_format_from_cairo_format (isurf->format));
            delete_qimg = TRUE;
        }

        if (surface->type == CAIRO_SURFACE_TYPE_QPAINTER) {
            cairo_qpainter_surface_t *qsrc = (cairo_qpainter_surface_t*) surface;

            if (qsrc->image)
                qimg = qsrc->image;
            else if (qsrc->pixmap)
                qpixmap = qsrc->pixmap;
        }

        if (!qimg && !qpixmap)
            return CAIRO_INT_STATUS_UNSUPPORTED;

        QMatrix savedMatrix = qs->p->worldMatrix();
        if (!_cairo_matrix_is_identity (&pattern->matrix)) {
            cairo_matrix_t pm = pattern->matrix;
            if (cairo_matrix_invert (&pm) == CAIRO_STATUS_SUCCESS)
                qs->p->setWorldMatrix(_qmatrix_from_cairo_matrix (pm), true);
        }

        if (qs->supports_porter_duff)
            qs->p->setCompositionMode (_qpainter_compositionmode_from_cairo_op (op));

        if (qimg)
            qs->p->drawImage (dst_x, dst_y, *qimg, src_x, src_y, width, height);
        else if (qpixmap)
            qs->p->drawPixmap (dst_x, dst_y, *qpixmap, src_x, src_y, width, height);

        if (qs->supports_porter_duff)
            qs->p->setCompositionMode (QPainter::CompositionMode_SourceOver);

        if (delete_qimg)
            delete qimg;
    } else {
        return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return CAIRO_INT_STATUS_SUCCESS;
}

/**
 ** Backend struct
 **/

static const struct _cairo_surface_backend cairo_qpainter_surface_backend = {
    CAIRO_SURFACE_TYPE_QPAINTER,
    _cairo_qpainter_surface_create_similar,
    _cairo_qpainter_surface_finish,
    _cairo_qpainter_surface_acquire_source_image,
    _cairo_qpainter_surface_release_source_image,
    _cairo_qpainter_surface_acquire_dest_image,
    _cairo_qpainter_surface_release_dest_image,
    _cairo_qpainter_surface_clone_similar,

    _cairo_qpainter_surface_composite, /* composite -- XXX temporary! */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    NULL, /* set_clip_region */
    _cairo_qpainter_surface_intersect_clip_path,
    _cairo_qpainter_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    _cairo_qpainter_surface_paint,
    _cairo_qpainter_surface_mask,
    _cairo_qpainter_surface_stroke,
    _cairo_qpainter_surface_fill,
    _cairo_qpainter_surface_show_glyphs,

    NULL, /* snapshot */
    NULL, /* is_similar */
    NULL, /* reset */
    NULL  /* fill_stroke */
};

#if defined(Q_WS_X11) && defined(CAIRO_HAS_XLIB_XRENDER_SURFACE)
void
_cairo_qpainter_create_xlib_surface (cairo_qpainter_surface_t *qs)
{
    Drawable d = None;
    QX11Info xinfo;
    int width, height;

    QPaintDevice *pd = qs->p->device();
    if (!pd)
	return;

    QPoint offs;
    QPaintDevice *rpd = QPainter::redirected(pd, &offs);
    if (rpd) {
	pd = rpd;
	qs->redir_offset = offs;
    }

    if (pd->devType() == QInternal::Widget) {
	QWidget *w = (QWidget*) pd;
	d = (Drawable) w->handle();
	xinfo = w->x11Info();
	width = w->width();
	height = w->height();
    } else if (pd->devType() == QInternal::Pixmap) {
	QPixmap *pixmap = (QPixmap*) pd;
	d = (Drawable) pixmap->handle();
	xinfo = pixmap->x11Info();
	width = pixmap->width();
	height = pixmap->height();
    } else {
	return;
    }

    if (d != None) {
	qs->xlib_equiv = cairo_xlib_surface_create_with_xrender_format
	    (xinfo.display(),
	     d,
	     ScreenOfDisplay(xinfo.display(), xinfo.screen()),
	     XRenderFindVisualFormat (xinfo.display(), (Visual*) xinfo.visual()),
	     width,
	     height);
    }
}
#endif

cairo_surface_t *
cairo_qpainter_surface_create (QPainter *painter)
{
    cairo_qpainter_surface_t *qs;

    qs = (cairo_qpainter_surface_t *) malloc (sizeof(cairo_qpainter_surface_t));
    if (qs == NULL)
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    memset (qs, 0, sizeof(cairo_qpainter_surface_t));

    _cairo_surface_init (&qs->base, &cairo_qpainter_surface_backend, CAIRO_CONTENT_COLOR_ALPHA);

    qs->p = painter;
    if (qs->p->paintEngine())
	qs->supports_porter_duff = qs->p->paintEngine()->hasFeature(QPaintEngine::PorterDuff);
    else
	qs->supports_porter_duff = FALSE;

    // Save so that we can always get back to the original state
    qs->p->save();

    qs->window = painter->window();

#if defined(Q_WS_X11) && defined(CAIRO_HAS_XLIB_XRENDER_SURFACE)
    _cairo_qpainter_create_xlib_surface (qs);
#endif

    D(fprintf(stderr, "qpainter_surface_create: window: [%d %d %d %d] pd:%d\n",
              qs->window.x(), qs->window.y(), qs->window.width(), qs->window.height(),
              qs->supports_porter_duff));

    return &qs->base;
}

cairo_surface_t *
cairo_qpainter_surface_create_with_qimage (cairo_format_t format,
                                           int width,
                                           int height)
{
    cairo_qpainter_surface_t *qs;

    qs = (cairo_qpainter_surface_t *) malloc (sizeof(cairo_qpainter_surface_t));
    if (qs == NULL)
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    memset (qs, 0, sizeof(cairo_qpainter_surface_t));

    _cairo_surface_init (&qs->base, &cairo_qpainter_surface_backend, _cairo_content_from_format (format));

    QImage *image = new QImage (width, height, _qimage_format_from_cairo_format (format));

    qs->image = image;

    if (!image->isNull()) {
        qs->p = new QPainter(image);
        qs->supports_porter_duff = qs->p->paintEngine()->hasFeature(QPaintEngine::PorterDuff);
    }

    qs->image_equiv = (cairo_image_surface_t*)
                      cairo_image_surface_create_for_data (image->bits(),
                                                           format,
                                                           width, height,
                                                           image->bytesPerLine());

    qs->window = QRect(0, 0, width, height);

    D(fprintf(stderr, "qpainter_surface_create: qimage: [%d %d %d %d] pd:%d\n",
              qs->window.x(), qs->window.y(), qs->window.width(), qs->window.height(),
              qs->supports_porter_duff));

    return &qs->base;
}

cairo_surface_t *
cairo_qpainter_surface_create_with_qpixmap (int width,
                                            int height)
{
    cairo_qpainter_surface_t *qs;

    qs = (cairo_qpainter_surface_t *) malloc (sizeof(cairo_qpainter_surface_t));
    if (qs == NULL)
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    memset (qs, 0, sizeof(cairo_qpainter_surface_t));

    QPixmap *pixmap = new QPixmap (width, height);

    _cairo_surface_init (&qs->base, &cairo_qpainter_surface_backend,
                         pixmap->hasAlphaChannel() ? CAIRO_CONTENT_COLOR_ALPHA : CAIRO_CONTENT_COLOR);


    qs->pixmap = pixmap;

    if (!pixmap->isNull()) {
        qs->p = new QPainter(pixmap);
        qs->supports_porter_duff = qs->p->paintEngine()->hasFeature(QPaintEngine::PorterDuff);
    }

    qs->window = QRect(0, 0, width, height);

#if defined(Q_WS_X11) && defined(CAIRO_HAS_XLIB_XRENDER_SURFACE)
    _cairo_qpainter_create_xlib_surface (qs);
#endif

    D(fprintf(stderr, "qpainter_surface_create: qpixmap: [%d %d %d %d] pd:%d\n",
              qs->window.x(), qs->window.y(), qs->window.width(), qs->window.height(),
              qs->supports_porter_duff));

    return &qs->base;
}

QPainter *
cairo_qpainter_surface_get_qpainter (cairo_surface_t *surface)
{
    cairo_qpainter_surface_t *qs = (cairo_qpainter_surface_t*) surface;

    if (surface->type != CAIRO_SURFACE_TYPE_QPAINTER)
        return NULL;

    return qs->p;
}

/*
 * TODO:
 *
 * - Figure out why QBrush isn't working with non-repeated images
 *
 * - Correct repeat mode; right now, every surface source is EXTEND_REPEAT
 *   - implement EXTEND_NONE (?? probably need to clip to the extents of the source)
 *   - implement EXTEND_REFLECT (create temporary and copy 4x, then EXTEND_REPEAT that)
 *
 * - stroke-image failure
 *
 * - Implement mask() with non-solid masks (probably will need to use a temporary and use IN)
 *
 * - Implement gradient sources
 *
 * - Make create_similar smarter -- create QPixmaps in more circumstances
 *   (e.g. if the pixmap can have alpha)
 *
 * - Implement show_glyphs() in terms of Qt
 *
 */

void
_cairo_image_surface_write_to_ppm (cairo_image_surface_t *isurf, const char *fn)
{
    char *fmt;
    if (isurf->format == CAIRO_FORMAT_ARGB32 || isurf->format == CAIRO_FORMAT_RGB24)
        fmt = "P6";
    else if (isurf->format == CAIRO_FORMAT_A8)
        fmt = "P5";
    else
        return;

    FILE *fp = fopen(fn, "wb");
    if (!fp)
        return;

    fprintf (fp, "%s %d %d 255\n", fmt,isurf->width, isurf->height);
    for (int j = 0; j < isurf->height; j++) {
        unsigned char *row = isurf->data + isurf->stride * j;
        for (int i = 0; i < isurf->width; i++) {
            if (isurf->format == CAIRO_FORMAT_ARGB32 || isurf->format == CAIRO_FORMAT_RGB24) {
                unsigned char r = *row++;
                unsigned char g = *row++;
                unsigned char b = *row++;
                *row++;
                putc(r, fp);
                putc(g, fp);
                putc(b, fp);
            } else {
                unsigned char a = *row++;
                putc(a, fp);
            }
        }
    }

    fclose (fp);

    fprintf (stderr, "Wrote %s\n", fn);
}
