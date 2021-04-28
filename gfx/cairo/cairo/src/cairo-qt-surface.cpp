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
#include "cairo-types-private.h"
#include "cairo-clip-private.h"
#include "cairo-surface-clipper-private.h"
#include "cairo-region-private.h"

#include "cairo-ft.h"
#include "cairo-qt.h"
#include "cairo-error-private.h"

#include <memory>

#include <QtGui/QPainter>
#include <QtGui/QPaintEngine>
#include <QtGui/QPaintDevice>
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <QtGui/QBrush>
#include <QtGui/QPen>
#include <QtCore/QVarLengthArray>

#include <sys/time.h>

/* Enable workaround slow regional Qt paths */
#define ENABLE_FAST_FILL 0
#define ENABLE_FAST_CLIP 0

#if 0
#define D(x)  x
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
#else
#define D(x) do { } while(0)
#endif

#ifndef CAIRO_INT_STATUS_SUCCESS
#define CAIRO_INT_STATUS_SUCCESS ((cairo_int_status_t) CAIRO_STATUS_SUCCESS)
#endif

/* Qt::PenStyle optimization based on the assumption that dots are 1*w and dashes are 3*w. */
#define DOT_LENGTH  1.0
#define DASH_LENGTH 3.0

struct cairo_qt_surface_t {
    cairo_surface_t base;

    cairo_bool_t supports_porter_duff;

    QPainter *p;

    /* The pixmap/image constructors will store their objects here */
    QPixmap *pixmap;
    QImage *image;

    QRect window;

    cairo_surface_clipper_t clipper;

    cairo_surface_t *image_equiv;
};

/* Will be true if we ever try to create a QPixmap and end
 * up with one without an alpha channel.
 */
static cairo_bool_t _qpixmaps_have_no_alpha = FALSE;

/**
 ** Helper methods
 **/

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

    default:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
    case CAIRO_OPERATOR_MULTIPLY:
    case CAIRO_OPERATOR_SCREEN:
    case CAIRO_OPERATOR_OVERLAY:
    case CAIRO_OPERATOR_DARKEN:
    case CAIRO_OPERATOR_LIGHTEN:
    case CAIRO_OPERATOR_COLOR_DODGE:
    case CAIRO_OPERATOR_COLOR_BURN:
    case CAIRO_OPERATOR_HARD_LIGHT:
    case CAIRO_OPERATOR_SOFT_LIGHT:
    case CAIRO_OPERATOR_DIFFERENCE:
    case CAIRO_OPERATOR_EXCLUSION:
    case CAIRO_OPERATOR_HSL_HUE:
    case CAIRO_OPERATOR_HSL_SATURATION:
    case CAIRO_OPERATOR_HSL_COLOR:
    case CAIRO_OPERATOR_HSL_LUMINOSITY:
	ASSERT_NOT_REACHED;
    }
    return QPainter::CompositionMode_Source;
}

static bool
_op_is_supported (cairo_qt_surface_t *qs, cairo_operator_t op)
{
    if (qs->supports_porter_duff) {
	switch (op) {
	case CAIRO_OPERATOR_CLEAR:
	case CAIRO_OPERATOR_SOURCE:
	case CAIRO_OPERATOR_OVER:
	case CAIRO_OPERATOR_IN:
	case CAIRO_OPERATOR_OUT:
	case CAIRO_OPERATOR_ATOP:

	case CAIRO_OPERATOR_DEST:
	case CAIRO_OPERATOR_DEST_OVER:
	case CAIRO_OPERATOR_DEST_IN:
	case CAIRO_OPERATOR_DEST_OUT:
	case CAIRO_OPERATOR_DEST_ATOP:

	case CAIRO_OPERATOR_XOR:
	    return TRUE;

	default:
	    ASSERT_NOT_REACHED;
	case CAIRO_OPERATOR_ADD:
	case CAIRO_OPERATOR_SATURATE:
	case CAIRO_OPERATOR_MULTIPLY:
	case CAIRO_OPERATOR_SCREEN:
	case CAIRO_OPERATOR_OVERLAY:
	case CAIRO_OPERATOR_DARKEN:
	case CAIRO_OPERATOR_LIGHTEN:
	case CAIRO_OPERATOR_COLOR_DODGE:
	case CAIRO_OPERATOR_COLOR_BURN:
	case CAIRO_OPERATOR_HARD_LIGHT:
	case CAIRO_OPERATOR_SOFT_LIGHT:
	case CAIRO_OPERATOR_DIFFERENCE:
	case CAIRO_OPERATOR_EXCLUSION:
	case CAIRO_OPERATOR_HSL_HUE:
	case CAIRO_OPERATOR_HSL_SATURATION:
	case CAIRO_OPERATOR_HSL_COLOR:
	case CAIRO_OPERATOR_HSL_LUMINOSITY:
	    return FALSE;

	}
    } else {
	return op == CAIRO_OPERATOR_OVER;
    }
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

    case QImage::Format_Invalid:
#ifdef WORDS_BIGENDIAN
    case QImage::Format_MonoLSB:
#else
    case QImage::Format_Mono:
#endif
    case QImage::Format_ARGB32:
    case QImage::Format_RGB16:
    case QImage::Format_ARGB8565_Premultiplied:
    case QImage::Format_RGB666:
    case QImage::Format_ARGB6666_Premultiplied:
    case QImage::Format_RGB555:
    case QImage::Format_ARGB8555_Premultiplied:
    case QImage::Format_RGB888:
    case QImage::Format_RGB444:
    case QImage::Format_ARGB4444_Premultiplied:
    case QImage::NImageFormats:
    default:
	ASSERT_NOT_REACHED;
        return (cairo_format_t) -1;
    }
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
    return QMatrix(m.xx, m.yx, m.xy, m.yy, m.x0, m.y0);
}

/** Path conversion **/
typedef struct _qpainter_path_transform {
    QPainterPath path;
    const cairo_matrix_t *ctm_inverse;
} qpainter_path_data;

/* cairo path -> execute in context */
static cairo_status_t
_cairo_path_to_qpainterpath_move_to (void *closure, const cairo_point_t *point)
{
    qpainter_path_data *pdata = static_cast <qpainter_path_data *> (closure);
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (pdata->ctm_inverse)
        cairo_matrix_transform_point (pdata->ctm_inverse, &x, &y);

    pdata->path.moveTo(x, y);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_qpainterpath_line_to (void *closure, const cairo_point_t *point)
{
    qpainter_path_data *pdata = static_cast <qpainter_path_data *> (closure);
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (pdata->ctm_inverse)
        cairo_matrix_transform_point (pdata->ctm_inverse, &x, &y);

    pdata->path.lineTo(x, y);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_qpainterpath_curve_to (void *closure, const cairo_point_t *p0, const cairo_point_t *p1, const cairo_point_t *p2)
{
    qpainter_path_data *pdata = static_cast <qpainter_path_data *> (closure);
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

    pdata->path.cubicTo (x0, y0, x1, y1, x2, y2);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_qpainterpath_close_path (void *closure)
{
    qpainter_path_data *pdata = static_cast <qpainter_path_data *> (closure);

    pdata->path.closeSubpath();

    return CAIRO_STATUS_SUCCESS;
}

static inline QPainterPath
path_to_qt (cairo_path_fixed_t *path,
	    const cairo_matrix_t *ctm_inverse = NULL)
{
    qpainter_path_data data;
    cairo_status_t status;

    if (ctm_inverse && _cairo_matrix_is_identity (ctm_inverse))
	ctm_inverse = NULL;
    data.ctm_inverse = ctm_inverse;

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_path_to_qpainterpath_move_to,
					  _cairo_path_to_qpainterpath_line_to,
					  _cairo_path_to_qpainterpath_curve_to,
					  _cairo_path_to_qpainterpath_close_path,
					  &data);
    assert (status == CAIRO_STATUS_SUCCESS);

    return data.path;
}

static inline QPainterPath
path_to_qt (cairo_path_fixed_t *path,
	    cairo_fill_rule_t fill_rule,
	    cairo_matrix_t *ctm_inverse = NULL)
{
    QPainterPath qpath = path_to_qt (path, ctm_inverse);

    qpath.setFillRule (fill_rule == CAIRO_FILL_RULE_WINDING ?
			Qt::WindingFill :
			Qt::OddEvenFill);

    return qpath;
}

/**
 ** Surface backend methods
 **/
static cairo_surface_t *
_cairo_qt_surface_create_similar (void *abstract_surface,
				  cairo_content_t content,
				  int width,
				  int height)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;
    bool use_pixmap;

    D(fprintf(stderr, "q[%p] create_similar: %d %d [%d] -> ", abstract_surface, width, height, content));

    use_pixmap = qs->image == NULL;
    if (use_pixmap) {
	switch (content) {
	case CAIRO_CONTENT_ALPHA:
	    use_pixmap = FALSE;
	    break;
	case CAIRO_CONTENT_COLOR:
	    break;
	case CAIRO_CONTENT_COLOR_ALPHA:
	    use_pixmap = ! _qpixmaps_have_no_alpha;
	    break;
	}
    }

    if (use_pixmap) {
	cairo_surface_t *result =
	    cairo_qt_surface_create_with_qpixmap (content, width, height);

	/* XXX result->content is always content. ??? */
	if (result->content == content) {
	    D(fprintf(stderr, "qpixmap content: %d\n", content));
	    return result;
	}

	_qpixmaps_have_no_alpha = TRUE;
	cairo_surface_destroy (result);
    }

    D(fprintf (stderr, "qimage\n"));
    return cairo_qt_surface_create_with_qimage
	(_cairo_format_from_content (content), width, height);
}

static cairo_status_t
_cairo_qt_surface_finish (void *abstract_surface)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] finish\n", abstract_surface));

    /* Only delete p if we created it */
    if (qs->image || qs->pixmap)
        delete qs->p;
    else if (qs->p)
	qs->p->restore ();

    if (qs->image_equiv)
        cairo_surface_destroy (qs->image_equiv);

    _cairo_surface_clipper_reset (&qs->clipper);

    if (qs->image)
        delete qs->image;

    if (qs->pixmap)
        delete qs->pixmap;

    return CAIRO_STATUS_SUCCESS;
}

static void
_qimg_destroy (void *closure)
{
    QImage *qimg = (QImage *) closure;
    delete qimg;
}

static cairo_status_t
_cairo_qt_surface_acquire_source_image (void *abstract_surface,
					cairo_image_surface_t **image_out,
					void **image_extra)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] acquire_source_image\n", abstract_surface));

    *image_extra = NULL;

    if (qs->image_equiv) {
        *image_out = (cairo_image_surface_t*)
                     cairo_surface_reference (qs->image_equiv);

        return CAIRO_STATUS_SUCCESS;
    }

    if (qs->pixmap) {
        QImage *qimg = new QImage(qs->pixmap->toImage());
	cairo_surface_t *image;
	cairo_status_t status;

        image = cairo_image_surface_create_for_data (qimg->bits(),
						     _cairo_format_from_qimage_format (qimg->format()),
						     qimg->width(), qimg->height(),
						     qimg->bytesPerLine());

	status = _cairo_user_data_array_set_data (&image->user_data,
						  (const cairo_user_data_key_t *)&_qimg_destroy,
						  qimg,
						  _qimg_destroy);
	if (status) {
	    cairo_surface_destroy (image);
	    return status;
	}

	*image_out = (cairo_image_surface_t *) image;
        return CAIRO_STATUS_SUCCESS;
    }

    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
}

static void
_cairo_qt_surface_release_source_image (void *abstract_surface,
					cairo_image_surface_t *image,
					void *image_extra)
{
    //cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] release_source_image\n", abstract_surface));

    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_qt_surface_acquire_dest_image (void *abstract_surface,
				      cairo_rectangle_int_t *interest_rect,
				      cairo_image_surface_t **image_out,
				      cairo_rectangle_int_t *image_rect,
				      void **image_extra)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;
    QImage *qimg = NULL;

    D(fprintf(stderr, "q[%p] acquire_dest_image\n", abstract_surface));

    *image_extra = NULL;

    if (qs->image_equiv) {
        *image_out = (cairo_image_surface_t*)
                     cairo_surface_reference (qs->image_equiv);

        image_rect->x = qs->window.x();
        image_rect->y = qs->window.y();
        image_rect->width = qs->window.width();
        image_rect->height = qs->window.height();

        return CAIRO_STATUS_SUCCESS;
    }

    QPoint offset;

    if (qs->pixmap) {
        qimg = new QImage(qs->pixmap->toImage());
    } else {
        // Try to figure out what kind of QPaintDevice we have, and
        // how we can grab an image from it
        QPaintDevice *pd = qs->p->device();
	if (!pd)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	QPaintDevice *rpd = QPainter::redirected(pd, &offset);
	if (rpd)
	    pd = rpd;

        if (pd->devType() == QInternal::Image) {
            qimg = new QImage(((QImage*) pd)->copy());
        } else if (pd->devType() == QInternal::Pixmap) {
            qimg = new QImage(((QPixmap*) pd)->toImage());
        }
    }

    if (qimg == NULL)
        return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    *image_out = (cairo_image_surface_t*)
                 cairo_image_surface_create_for_data (qimg->bits(),
                                                      _cairo_format_from_qimage_format (qimg->format()),
                                                      qimg->width(), qimg->height(),
                                                      qimg->bytesPerLine());
    *image_extra = qimg;

    image_rect->x = qs->window.x() + offset.x();
    image_rect->y = qs->window.y() + offset.y();
    image_rect->width = qs->window.width() - offset.x();
    image_rect->height = qs->window.height() - offset.y();

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_qt_surface_release_dest_image (void *abstract_surface,
				      cairo_rectangle_int_t *interest_rect,
				      cairo_image_surface_t *image,
				      cairo_rectangle_int_t *image_rect,
				      void *image_extra)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;
    D(fprintf(stderr, "q[%p] release_dest_image\n", abstract_surface));

    cairo_surface_destroy (&image->base);

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
_cairo_qt_surface_clone_similar (void *abstract_surface,
				 cairo_surface_t *src,
				 int              src_x,
				 int              src_y,
				 int              width,
				 int              height,
				 int              *clone_offset_x,
				 int              *clone_offset_y,
				 cairo_surface_t **clone_out)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;

    if (src->backend == qs->base.backend) {
	*clone_offset_x = 0;
	*clone_offset_y = 0;
	*clone_out = cairo_surface_reference (src);
	return CAIRO_STATUS_SUCCESS;
    }

    return (cairo_status_t) CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_bool_t
_cairo_qt_surface_get_extents (void *abstract_surface,
			       cairo_rectangle_int_t *extents)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;

    extents->x = qs->window.x();
    extents->y = qs->window.y();
    extents->width = qs->window.width();
    extents->height = qs->window.height();

    return TRUE;
}

static cairo_status_t
_cairo_qt_surface_clipper_intersect_clip_path (cairo_surface_clipper_t *clipper,
					       cairo_path_fixed_t *path,
					       cairo_fill_rule_t fill_rule,
					       double tolerance,
					       cairo_antialias_t antialias)
{
    cairo_qt_surface_t *qs = cairo_container_of (clipper,
						 cairo_qt_surface_t,
						 clipper);

    if (path == NULL) {
        if (qs->pixmap || qs->image) {
            // we own p
            qs->p->setClipping (false);
        } else {
            qs->p->restore ();
            qs->p->save ();
        }
    } else {
	// XXX Antialiasing is ignored
	qs->p->setClipPath (path_to_qt (path, fill_rule), Qt::IntersectClip);
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_qt_surface_set_clip_region (cairo_qt_surface_t *qs,
				   cairo_region_t *clip_region)
{
    _cairo_surface_clipper_reset (&qs->clipper);

    if (clip_region == NULL) {
        // How the clip path is reset depends on whether we own p or not
        if (qs->pixmap || qs->image) {
            // we own p
            qs->p->setClipping (false);
        } else {
            qs->p->restore ();
            qs->p->save ();
        }
    } else {
	QRegion qr;
	int num_rects = cairo_region_num_rectangles (clip_region);
	for (int i = 0; i < num_rects; ++i) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (clip_region, i, &rect);

	    QRect r(rect.x, rect.y, rect.width, rect.height);
	    qr = qr.united(r);
	}

	qs->p->setClipRegion (qr, Qt::IntersectClip);
    }
}

static cairo_int_status_t
_cairo_qt_surface_set_clip (cairo_qt_surface_t *qs,
			    cairo_clip_t *clip)
{
    cairo_int_status_t status;

    D(fprintf(stderr, "q[%p] intersect_clip_path %s\n", qs, clip ? "(path)" : "(clear)"));

    if (clip == NULL) {
	_cairo_surface_clipper_reset (&qs->clipper);
        // How the clip path is reset depends on whether we own p or not
        if (qs->pixmap || qs->image) {
            // we own p
            qs->p->setClipping (false);
        } else {
            qs->p->restore ();
            qs->p->save ();
        }

        return CAIRO_INT_STATUS_SUCCESS;
    }

#if ENABLE_FAST_CLIP
    // Qt will implicitly enable clipping, and will use ReplaceClip
    // instead of IntersectClip if clipping was disabled before

    // Note: Qt is really bad at dealing with clip paths.  It doesn't
    // seem to usefully recognize rectangular paths, instead going down
    // extremely slow paths whenever a clip path is set.  So,
    // we do a bunch of work here to try to get rectangles or regions
    // down to Qt for clipping.

    cairo_region_t *clip_region = NULL;

    status = _cairo_clip_get_region (clip, &clip_region);
    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	// We weren't able to extract a region from the traps.
	// Just hand the path down to QPainter.
	status = (cairo_int_status_t)
	    _cairo_surface_clipper_set_clip (&qs->clipper, clip);
    } else if (status == CAIRO_INT_STATUS_SUCCESS) {
	_cairo_qt_surface_set_clip_region (qs, clip_region);
	status = CAIRO_INT_STATUS_SUCCESS;
    }
#else
    status = (cairo_int_status_t)
	_cairo_surface_clipper_set_clip (&qs->clipper, clip);
#endif

    return status;
}

/**
 ** Brush conversion
 **/

struct PatternToBrushConverter {
    PatternToBrushConverter (const cairo_pattern_t *pattern) :
	mAcquiredImageParent(0),
	mAcquiredImage(0),
	mAcquiredImageExtra(0)
    {
	if (pattern->type == CAIRO_PATTERN_TYPE_SOLID) {
	    cairo_solid_pattern_t *solid = (cairo_solid_pattern_t*) pattern;
	    QColor color;
	    color.setRgbF(solid->color.red,
			  solid->color.green,
			  solid->color.blue,
			  solid->color.alpha);

	    mBrush = QBrush(color);
	} else if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	    cairo_surface_pattern_t *spattern = (cairo_surface_pattern_t*) pattern;
	    cairo_surface_t *surface = spattern->surface;

	    if (surface->type == CAIRO_SURFACE_TYPE_QT) {
		cairo_qt_surface_t *qs = (cairo_qt_surface_t*) surface;

		if (qs->image) {
		    mBrush = QBrush(*qs->image);
		} else if (qs->pixmap) {
		    mBrush = QBrush(*qs->pixmap);
		} else {
		    // do something smart
		    mBrush = QBrush(0xff0000ff);
		}
	    } else {
		cairo_image_surface_t *isurf = NULL;

		if (surface->type == CAIRO_SURFACE_TYPE_IMAGE) {
		    isurf = (cairo_image_surface_t*) surface;
		} else {
		    void *image_extra;

		    if (_cairo_surface_acquire_source_image (surface, &isurf, &image_extra) == CAIRO_STATUS_SUCCESS) {
			mAcquiredImageParent = surface;
			mAcquiredImage = isurf;
			mAcquiredImageExtra = image_extra;
		    } else {
			isurf = NULL;
		    }
		}

		if (isurf) {
		    mBrush = QBrush (QImage ((const uchar *) isurf->data,
						 isurf->width,
						 isurf->height,
						 isurf->stride,
						 _qimage_format_from_cairo_format (isurf->format)));
		} else {
		    mBrush = QBrush(0x0000ffff);
		}
	    }
	} else if (pattern->type == CAIRO_PATTERN_TYPE_LINEAR ||
		   pattern->type == CAIRO_PATTERN_TYPE_RADIAL)
	{
	    QGradient *grad;
	    cairo_bool_t reverse_stops = FALSE;
	    cairo_bool_t emulate_reflect = FALSE;
	    double offset = 0.0;

	    cairo_extend_t extend = pattern->extend;

	    cairo_gradient_pattern_t *gpat = (cairo_gradient_pattern_t *) pattern;

	    if (pattern->type == CAIRO_PATTERN_TYPE_LINEAR) {
		cairo_linear_pattern_t *lpat = (cairo_linear_pattern_t *) pattern;
		grad = new QLinearGradient (_cairo_fixed_to_double (lpat->p1.x),
					    _cairo_fixed_to_double (lpat->p1.y),
					    _cairo_fixed_to_double (lpat->p2.x),
					    _cairo_fixed_to_double (lpat->p2.y));
	    } else if (pattern->type == CAIRO_PATTERN_TYPE_RADIAL) {
		cairo_radial_pattern_t *rpat = (cairo_radial_pattern_t *) pattern;

		/* Based on the SVG surface code */

		cairo_point_t *c0, *c1;
		cairo_fixed_t radius0, radius1;

		if (rpat->r1 < rpat->r2) {
		    c0 = &rpat->c1;
		    c1 = &rpat->c2;
		    radius0 = rpat->r1;
		    radius1 = rpat->r2;
		    reverse_stops = FALSE;
		} else {
		    c0 = &rpat->c2;
		    c1 = &rpat->c1;
		    radius0 = rpat->r2;
		    radius1 = rpat->r1;
		    reverse_stops = TRUE;
		}

		double x0 = _cairo_fixed_to_double (c0->x);
		double y0 = _cairo_fixed_to_double (c0->y);
		double r0 = _cairo_fixed_to_double (radius0);
		double x1 = _cairo_fixed_to_double (c1->x);
		double y1 = _cairo_fixed_to_double (c1->y);
		double r1 = _cairo_fixed_to_double (radius1);

		if (rpat->r1 == rpat->r2) {
		    grad = new QRadialGradient (x1, y1, r1, x1, y1);
		} else {
		    double fx = (r1 * x0 - r0 * x1) / (r1 - r0);
		    double fy = (r1 * y0 - r0 * y1) / (r1 - r0);

		    /* QPainter doesn't support the inner circle and use instead a gradient focal.
		     * That means we need to emulate the cairo behaviour by processing the
		     * cairo gradient stops.
		     * The CAIRO_EXTENT_NONE and CAIRO_EXTENT_PAD modes are quite easy to handle,
		     * it's just a matter of stop position translation and calculation of
		     * the corresponding SVG radial gradient focal.
		     * The CAIRO_EXTENT_REFLECT and CAIRO_EXTEND_REPEAT modes require to compute a new
		     * radial gradient, with an new outer circle, equal to r1 - r0 in the CAIRO_EXTEND_REPEAT
		     * case, and 2 * (r1 - r0) in the CAIRO_EXTENT_REFLECT case, and a new gradient stop
		     * list that maps to the original cairo stop list.
		     */
		    if ((extend == CAIRO_EXTEND_REFLECT || extend == CAIRO_EXTEND_REPEAT) && r0 > 0.0) {
			double r_org = r1;
			double r, x, y;

			if (extend == CAIRO_EXTEND_REFLECT) {
			    r1 = 2 * r1 - r0;
			    emulate_reflect = TRUE;
			}

			offset = fmod (r1, r1 - r0) / (r1 - r0) - 1.0;
			r = r1 - r0;

			/* New position of outer circle. */
			x = r * (x1 - fx) / r_org + fx;
			y = r * (y1 - fy) / r_org + fy;

			x1 = x;
			y1 = y;
			r1 = r;
			r0 = 0.0;
		    } else {
			offset = r0 / r1;
		    }

		    grad = new QRadialGradient (x1, y1, r1, fx, fy);

		    if (extend == CAIRO_EXTEND_NONE && r0 != 0.0)
			grad->setColorAt (r0 / r1, Qt::transparent);
		}
	    }

	    switch (extend) {
		case CAIRO_EXTEND_NONE:
		case CAIRO_EXTEND_PAD:
		    grad->setSpread(QGradient::PadSpread);

		    grad->setColorAt (0.0, Qt::transparent);
		    grad->setColorAt (1.0, Qt::transparent);
		    break;

		case CAIRO_EXTEND_REFLECT:
		    grad->setSpread(QGradient::ReflectSpread);
		    break;

		case CAIRO_EXTEND_REPEAT:
		    grad->setSpread(QGradient::RepeatSpread);
		    break;
	    }

	    for (unsigned int i = 0; i < gpat->n_stops; i++) {
		int index = i;
		if (reverse_stops)
		    index = gpat->n_stops - i - 1;

		double offset = gpat->stops[i].offset;
		QColor color;
		color.setRgbF (gpat->stops[i].color.red,
			       gpat->stops[i].color.green,
			       gpat->stops[i].color.blue,
			       gpat->stops[i].color.alpha);

		if (emulate_reflect) {
		    offset = offset / 2.0;
		    grad->setColorAt (1.0 - offset, color);
		}

		grad->setColorAt (offset, color);
	    }

	    mBrush = QBrush(*grad);

	    delete grad;
	}

	if (mBrush.style() != Qt::NoBrush  &&
            pattern->type != CAIRO_PATTERN_TYPE_SOLID &&
            ! _cairo_matrix_is_identity (&pattern->matrix))
	{
	    cairo_matrix_t pm = pattern->matrix;
	    cairo_status_t status = cairo_matrix_invert (&pm);
	    assert (status == CAIRO_STATUS_SUCCESS);
	    mBrush.setMatrix (_qmatrix_from_cairo_matrix (pm));
	}
    }

    ~PatternToBrushConverter () {
	if (mAcquiredImageParent)
	    _cairo_surface_release_source_image (mAcquiredImageParent, mAcquiredImage, mAcquiredImageExtra);
    }

    operator QBrush& () {
	return mBrush;
    }

    QBrush mBrush;

    private:
    cairo_surface_t *mAcquiredImageParent;
    cairo_image_surface_t *mAcquiredImage;
    void *mAcquiredImageExtra;
};

struct PatternToPenConverter {
    PatternToPenConverter (const cairo_pattern_t *source,
                           const cairo_stroke_style_t *style) :
        mBrushConverter(source)
    {
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

        mPen = QPen(mBrushConverter, style->line_width, Qt::SolidLine, cap, join);
        mPen.setMiterLimit (style->miter_limit);

        if (style->dash && style->num_dashes) {
            Qt::PenStyle pstyle = Qt::NoPen;

            if (style->num_dashes == 2) {
                if ((style->dash[0] == style->line_width &&
                        style->dash[1] == style->line_width && style->line_width <= 2.0) ||
                    (style->dash[0] == 0.0 &&
                        style->dash[1] == style->line_width * 2 && cap == Qt::RoundCap))
                {
                    pstyle = Qt::DotLine;
                } else if (style->dash[0] == style->line_width * DASH_LENGTH &&
                           style->dash[1] == style->line_width * DASH_LENGTH &&
                           cap == Qt::FlatCap)
                {
                    pstyle = Qt::DashLine;
                }
            }

            if (pstyle != Qt::NoPen) {
                mPen.setStyle(pstyle);
                return;
            }

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

            mPen.setDashPattern(dashes);
            mPen.setDashOffset(style->dash_offset / style->line_width);
        }
    }

    ~PatternToPenConverter() { }

    operator QPen& () {
        return mPen;
    }

    QPen mPen;
    PatternToBrushConverter mBrushConverter;
};

/**
 ** Core drawing operations
 **/

static bool
_cairo_qt_fast_fill (cairo_qt_surface_t *qs,
		     const cairo_pattern_t *source,
		     cairo_path_fixed_t *path = NULL,
		     cairo_fill_rule_t fill_rule = CAIRO_FILL_RULE_WINDING,
		     double tolerance = 0.0,
		     cairo_antialias_t antialias = CAIRO_ANTIALIAS_NONE)
{
#if ENABLE_FAST_FILL
    QImage *qsSrc_image = NULL;
    QPixmap *qsSrc_pixmap = NULL;
    std::auto_ptr<QImage> qsSrc_image_d;


    if (source->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_pattern_t *spattern = (cairo_surface_pattern_t*) source;
	if (spattern->surface->type == CAIRO_SURFACE_TYPE_QT) {
	    cairo_qt_surface_t *p = (cairo_qt_surface_t*) spattern->surface;

	    qsSrc_image = p->image;
	    qsSrc_pixmap = p->pixmap;
	} else if (spattern->surface->type == CAIRO_SURFACE_TYPE_IMAGE) {
	    cairo_image_surface_t *p = (cairo_image_surface_t*) spattern->surface;
	    qsSrc_image = new QImage((const uchar*) p->data,
				     p->width,
				     p->height,
				     p->stride,
				     _qimage_format_from_cairo_format(p->format));
	    qsSrc_image_d.reset(qsSrc_image);
	}
    }

    if (!qsSrc_image && !qsSrc_pixmap)
	return false;

    // We can only drawTiledPixmap; there's no drawTiledImage
    if (! qsSrc_pixmap &&
	(source->extend == CAIRO_EXTEND_REPEAT ||
	 source->extend == CAIRO_EXTEND_REFLECT))
    {
	return false;
    }

    QMatrix sourceMatrix = _qmatrix_from_cairo_matrix (source->matrix);

    // We can draw this faster by clipping and calling drawImage/drawPixmap.
    // Use our own clipping function so that we can get the
    // region handling to end up with the fastest possible clip.
    //
    // XXX Antialiasing will fail pretty hard here, since we can't clip with AA
    // with QPainter.
    qs->p->save();

    if (path) {
	cairo_int_status_t status;

	cairo_clip_t clip, old_clip = qs->clipper.clip;

	_cairo_clip_init_copy (&clip, &qs->clipper.clip);
	status = (cairo_int_status_t) _cairo_clip_clip (&clip,
							path,
							fill_rule,
							tolerance,
							antialias);
	if (unlikely (status)) {
	    qs->p->restore();
	    return false;
	}

	status = _cairo_qt_surface_set_clip (qs, &clip);
	if (unlikely (status)) {
	    qs->p->restore();
	    return false;
	}

	_cairo_clip_reset (&clip);
	qs->clipper.clip = old_clip;
    }

    qs->p->setWorldMatrix (sourceMatrix.inverted(), true);

    switch (source->extend) {
    case CAIRO_EXTEND_REPEAT:
    // XXX handle reflect by tiling 4 times first
    case CAIRO_EXTEND_REFLECT: {
            assert (qsSrc_pixmap);

            // Render the tiling to cover the entire destination window (because
            // it'll be clipped).  Transform the window rect by the inverse
            // of the current world transform so that the device coordinates
            // end up as the right thing.
            QRectF dest = qs->p->worldTransform().inverted().mapRect(QRectF(qs->window));
            QPointF origin = sourceMatrix.map(QPointF(0.0, 0.0));

            qs->p->drawTiledPixmap (dest, *qsSrc_pixmap, origin);
        }
        break;
    case CAIRO_EXTEND_NONE:
    case CAIRO_EXTEND_PAD: // XXX not exactly right, but good enough
    default:
        if (qsSrc_image)
            qs->p->drawImage (0, 0, *qsSrc_image);
        else if (qsSrc_pixmap)
            qs->p->drawPixmap (0, 0, *qsSrc_pixmap);
        break;
    }

    qs->p->restore();

    return true;
#else
    return false;
#endif
}

static cairo_int_status_t
_cairo_qt_surface_paint (void *abstract_surface,
			 cairo_operator_t op,
			 const cairo_pattern_t *source,
			 cairo_clip_t	       *clip)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;
    cairo_int_status_t status;

    D(fprintf(stderr, "q[%p] paint op:%s\n", abstract_surface, _opstr(op)));

    if (!qs->p)
        return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _op_is_supported (qs, op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_qt_surface_set_clip (qs, clip);
    if (unlikely (status))
	return status;

    if (qs->supports_porter_duff)
        qs->p->setCompositionMode (_qpainter_compositionmode_from_cairo_op (op));

    if (! _cairo_qt_fast_fill (qs, source)) {
	PatternToBrushConverter brush (source);
        qs->p->fillRect (qs->window, brush);
    }

    if (qs->supports_porter_duff)
        qs->p->setCompositionMode (QPainter::CompositionMode_SourceOver);

    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_qt_surface_fill (void *abstract_surface,
			cairo_operator_t op,
			const cairo_pattern_t *source,
			cairo_path_fixed_t *path,
			cairo_fill_rule_t fill_rule,
			double tolerance,
			cairo_antialias_t antialias,
			cairo_clip_t *clip)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] fill op:%s\n", abstract_surface, _opstr(op)));

    if (!qs->p)
        return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _op_is_supported (qs, op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    cairo_int_status_t status = _cairo_qt_surface_set_clip (qs, clip);
    if (unlikely (status))
	return status;

    if (qs->supports_porter_duff)
        qs->p->setCompositionMode (_qpainter_compositionmode_from_cairo_op (op));

    // XXX Qt4.3, 4.4 misrenders some complex paths if antialiasing is
    // enabled
    //qs->p->setRenderHint (QPainter::Antialiasing, antialias == CAIRO_ANTIALIAS_NONE ? false : true);
    qs->p->setRenderHint (QPainter::SmoothPixmapTransform, source->filter != CAIRO_FILTER_FAST);

    if (! _cairo_qt_fast_fill (qs, source,
			       path, fill_rule, tolerance, antialias))
    {
	PatternToBrushConverter brush(source);
	qs->p->fillPath (path_to_qt (path, fill_rule), brush);
    }

    if (qs->supports_porter_duff)
        qs->p->setCompositionMode (QPainter::CompositionMode_SourceOver);

    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_qt_surface_stroke (void *abstract_surface,
			  cairo_operator_t op,
			  const cairo_pattern_t *source,
			  cairo_path_fixed_t *path,
			  const cairo_stroke_style_t *style,
			  const cairo_matrix_t *ctm,
			  const cairo_matrix_t *ctm_inverse,
			  double tolerance,
			  cairo_antialias_t antialias,
			  cairo_clip_t *clip)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] stroke op:%s\n", abstract_surface, _opstr(op)));

    if (!qs->p)
        return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _op_is_supported (qs, op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    cairo_int_status_t int_status = _cairo_qt_surface_set_clip (qs, clip);
    if (unlikely (int_status))
	return int_status;


    QMatrix savedMatrix = qs->p->worldMatrix();

    if (qs->supports_porter_duff)
        qs->p->setCompositionMode (_qpainter_compositionmode_from_cairo_op (op));

    qs->p->setWorldMatrix (_qmatrix_from_cairo_matrix (*ctm), true);
    // XXX Qt4.3, 4.4 misrenders some complex paths if antialiasing is
    // enabled
    //qs->p->setRenderHint (QPainter::Antialiasing, antialias == CAIRO_ANTIALIAS_NONE ? false : true);
    qs->p->setRenderHint (QPainter::SmoothPixmapTransform, source->filter != CAIRO_FILTER_FAST);

    PatternToPenConverter pen(source, style);

    qs->p->setPen(pen);
    qs->p->drawPath(path_to_qt (path, ctm_inverse));
    qs->p->setPen(Qt::black);

    qs->p->setWorldMatrix (savedMatrix, false);

    if (qs->supports_porter_duff)
        qs->p->setCompositionMode (QPainter::CompositionMode_SourceOver);

    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_qt_surface_show_glyphs (void *abstract_surface,
			       cairo_operator_t op,
			       const cairo_pattern_t *source,
			       cairo_glyph_t *glyphs,
			       int num_glyphs,
			       cairo_scaled_font_t *scaled_font,
			       cairo_clip_t *clip,
			       int *remaining_glyphs)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_qt_surface_mask (void *abstract_surface,
			cairo_operator_t op,
			const cairo_pattern_t *source,
			const cairo_pattern_t *mask,
			cairo_clip_t	    *clip)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;

    D(fprintf(stderr, "q[%p] mask op:%s\n", abstract_surface, _opstr(op)));

    if (!qs->p)
        return CAIRO_INT_STATUS_UNSUPPORTED;

    if (mask->type == CAIRO_PATTERN_TYPE_SOLID) {
        cairo_solid_pattern_t *solid_mask = (cairo_solid_pattern_t *) mask;
        cairo_int_status_t result;

        qs->p->setOpacity (solid_mask->color.alpha);

        result = _cairo_qt_surface_paint (abstract_surface, op, source, clip);

        qs->p->setOpacity (1.0);

        return result;
    }

    // otherwise skip for now
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_qt_surface_composite (cairo_operator_t op,
			     const cairo_pattern_t *pattern,
			     const cairo_pattern_t *mask_pattern,
			     void *abstract_surface,
			     int src_x,
			     int src_y,
			     int mask_x,
			     int mask_y,
			     int dst_x,
			     int dst_y,
			     unsigned int width,
			     unsigned int height,
			     cairo_region_t *clip_region)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;

    if (mask_pattern)
        return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _op_is_supported (qs, op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    _cairo_qt_surface_set_clip_region (qs, clip_region);

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
        std::auto_ptr<QImage> qimg_d;

        if (surface->type == CAIRO_SURFACE_TYPE_IMAGE) {
            cairo_image_surface_t *isurf = (cairo_image_surface_t*) surface;
            qimg = new QImage ((const uchar *) isurf->data,
                               isurf->width,
                               isurf->height,
                               isurf->stride,
                               _qimage_format_from_cairo_format (isurf->format));
            qimg_d.reset(qimg);
        }

        if (surface->type == CAIRO_SURFACE_TYPE_QT) {
            cairo_qt_surface_t *qsrc = (cairo_qt_surface_t*) surface;

            if (qsrc->image)
                qimg = qsrc->image;
            else if (qsrc->pixmap)
                qpixmap = qsrc->pixmap;
        }

        if (!qimg && !qpixmap)
            return CAIRO_INT_STATUS_UNSUPPORTED;

        QMatrix savedMatrix = qs->p->worldMatrix();
        if (! _cairo_matrix_is_identity (&pattern->matrix)) {
            cairo_matrix_t pm = pattern->matrix;
	    cairo_status_t status;

            status = cairo_matrix_invert (&pm);
	    assert (status == CAIRO_STATUS_SUCCESS);
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
    } else {
        return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_qt_surface_mark_dirty (void *abstract_surface,
			      int x, int y,
			      int width, int height)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t *) abstract_surface;

    if (qs->p && !(qs->image || qs->pixmap))
	qs->p->save ();

    return CAIRO_STATUS_SUCCESS;
}

/**
 ** Backend struct
 **/

static const cairo_surface_backend_t cairo_qt_surface_backend = {
    CAIRO_SURFACE_TYPE_QT,
    _cairo_qt_surface_create_similar,
    _cairo_qt_surface_finish,
    _cairo_qt_surface_acquire_source_image,
    _cairo_qt_surface_release_source_image,
    _cairo_qt_surface_acquire_dest_image,
    _cairo_qt_surface_release_dest_image,
    _cairo_qt_surface_clone_similar,

    _cairo_qt_surface_composite,
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_qt_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    _cairo_qt_surface_mark_dirty,
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    _cairo_qt_surface_paint,
    _cairo_qt_surface_mask,
    _cairo_qt_surface_stroke,
    _cairo_qt_surface_fill,
    _cairo_qt_surface_show_glyphs,

    NULL, /* snapshot */
    NULL, /* is_similar */
    NULL, /* fill_stroke */
    NULL, /* create_solid_pattern_surface */
    NULL, /* can_repaint_solid_pattern_surface */
    NULL, /* has_show_text_glyphs */
    NULL, /* show_text_glyphs */
};

cairo_surface_t *
cairo_qt_surface_create (QPainter *painter)
{
    cairo_qt_surface_t *qs;

    qs = (cairo_qt_surface_t *) malloc (sizeof(cairo_qt_surface_t));
    if (qs == NULL)
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    memset (qs, 0, sizeof(cairo_qt_surface_t));

    _cairo_surface_init (&qs->base,
			 &cairo_qt_surface_backend,
			 NULL,
			 CAIRO_CONTENT_COLOR_ALPHA);

    _cairo_surface_clipper_init (&qs->clipper,
				 _cairo_qt_surface_clipper_intersect_clip_path);

    qs->p = painter;
    if (qs->p->paintEngine())
        qs->supports_porter_duff = qs->p->paintEngine()->hasFeature(QPaintEngine::PorterDuff);
    else
        qs->supports_porter_duff = FALSE;

    // Save so that we can always get back to the original state
    qs->p->save();

    qs->window = painter->window();

    D(fprintf(stderr, "qpainter_surface_create: window: [%d %d %d %d] pd:%d\n",
              qs->window.x(), qs->window.y(), qs->window.width(), qs->window.height(),
              qs->supports_porter_duff));

    return &qs->base;
}

cairo_surface_t *
cairo_qt_surface_create_with_qimage (cairo_format_t format,
				     int width,
				     int height)
{
    cairo_qt_surface_t *qs;

    qs = (cairo_qt_surface_t *) malloc (sizeof(cairo_qt_surface_t));
    if (qs == NULL)
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    memset (qs, 0, sizeof(cairo_qt_surface_t));

    _cairo_surface_init (&qs->base,
			 &cairo_qt_surface_backend,
			 NULL,
			 _cairo_content_from_format (format));

    _cairo_surface_clipper_init (&qs->clipper,
				 _cairo_qt_surface_clipper_intersect_clip_path);

    if (CAIRO_FORMAT_A8 == format) {
        qs->image = NULL;
        qs->image_equiv = cairo_image_surface_create(format,
                                                     width, height);
        qs->p = NULL;
        qs->supports_porter_duff = false;
        qs->window = QRect(0, 0, width, height);
        return &qs->base;
    }

    QImage *image = new QImage (width, height,
				_qimage_format_from_cairo_format (format));

    qs->image = image;

    if (!image->isNull()) {
        qs->p = new QPainter(image);
        qs->supports_porter_duff = qs->p->paintEngine()->hasFeature(QPaintEngine::PorterDuff);
    }

    qs->image_equiv = cairo_image_surface_create_for_data (image->bits(),
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
cairo_qt_surface_create_with_qpixmap (cairo_content_t content,
				      int width,
				      int height)
{
    cairo_qt_surface_t *qs;

    if ((content & CAIRO_CONTENT_COLOR) == 0)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_CONTENT));

    qs = (cairo_qt_surface_t *) malloc (sizeof(cairo_qt_surface_t));
    if (qs == NULL)
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    memset (qs, 0, sizeof(cairo_qt_surface_t));

    QPixmap *pixmap = new QPixmap (width, height);
    if (pixmap == NULL) {
	free (qs);
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    // By default, a QPixmap is opaque; however, if it's filled
    // with a color with a transparency component, it is converted
    // to a format that preserves transparency.
    if (content == CAIRO_CONTENT_COLOR_ALPHA)
	pixmap->fill(Qt::transparent);

    _cairo_surface_init (&qs->base, &cairo_qt_surface_backend, NULL, content);

    _cairo_surface_clipper_init (&qs->clipper,
				 _cairo_qt_surface_clipper_intersect_clip_path);

    qs->pixmap = pixmap;

    if (!pixmap->isNull()) {
        qs->p = new QPainter(pixmap);
        qs->supports_porter_duff = qs->p->paintEngine()->hasFeature(QPaintEngine::PorterDuff);
    }

    qs->window = QRect(0, 0, width, height);

    D(fprintf(stderr, "qpainter_surface_create: qpixmap: [%d %d %d %d] pd:%d\n",
              qs->window.x(), qs->window.y(), qs->window.width(), qs->window.height(),
              qs->supports_porter_duff));

    return &qs->base;
}

QPainter *
cairo_qt_surface_get_qpainter (cairo_surface_t *surface)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t*) surface;

    if (surface->type != CAIRO_SURFACE_TYPE_QT)
        return NULL;

    return qs->p;
}

QImage *
cairo_qt_surface_get_qimage (cairo_surface_t *surface)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t*) surface;

    if (surface->type != CAIRO_SURFACE_TYPE_QT)
        return NULL;

    return qs->image;
}

cairo_surface_t *
cairo_qt_surface_get_image (cairo_surface_t *surface)
{
    cairo_qt_surface_t *qs = (cairo_qt_surface_t*) surface;

    if (surface->type != CAIRO_SURFACE_TYPE_QT)
        return NULL;

    return qs->image_equiv;
}

/*
 * TODO:
 *
 * - Figure out why QBrush isn't working with non-repeated images
 *
 * - Correct repeat mode; right now, every surface source is ExtendMode::REPEAT
 *   - implement EXTEND_NONE (?? probably need to clip to the extents of the source)
 *   - implement ExtendMode::REFLECT (create temporary and copy 4x, then ExtendMode::REPEAT that)
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
