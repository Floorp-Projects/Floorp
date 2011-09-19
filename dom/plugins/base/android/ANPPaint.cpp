/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Android NPAPI support code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@mozilla.com>
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

#include <stdlib.h>
#include <assert.h>
#include <android/log.h>
#include "ANPBase.h"

#define LOG(args...)  
//__android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_paint_##name

ANPPaint*
anp_paint_newPaint()
{
  LOG("%s", __PRETTY_FUNCTION__);

  ANPPaintPrivate* p = (ANPPaintPrivate*) calloc(1, sizeof(ANPPaintPrivate));
  return (ANPPaint*) p;
}

void
anp_paint_deletePaint(ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  free((void*)p);
}


ANPPaintFlags
anp_paint_getFlags(const ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return kAntiAlias_ANPPaintFlag;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  return p->flags;
}

void
anp_paint_setFlags(ANPPaint* paint, ANPPaintFlags flags)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;  
  p->flags = flags;
}


ANPColor
anp_paint_getColor(const ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return ANP_MAKE_COLOR(1, 255, 255, 255);

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  return p->color;
}

void
anp_paint_setColor(ANPPaint* paint, ANPColor color)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  p->color = color;
}


ANPPaintStyle
anp_paint_getStyle(const ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return kFill_ANPPaintStyle;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  return p->style;
}

void
anp_paint_setStyle(ANPPaint* paint, ANPPaintStyle style)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  p->style = style;
}

float
anp_paint_getStrokeWidth(const ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return 0;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  return p->strokeWidth;
}

float
anp_paint_getStrokeMiter(const ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return 0;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  return p->strokeMiter;
}

ANPPaintCap
anp_paint_getStrokeCap(const ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return kButt_ANPPaintCap;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  return p->paintCap;
}

ANPPaintJoin
anp_paint_getStrokeJoin(const ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return kMiter_ANPPaintJoin;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  return p->paintJoin;
}

void
anp_paint_setStrokeWidth(ANPPaint* paint, float width)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  p->strokeWidth = width;
}

void
anp_paint_setStrokeMiter(ANPPaint* paint, float miter)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  p->strokeMiter = miter;
}

void
anp_paint_setStrokeCap(ANPPaint* paint, ANPPaintCap cap)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  p->paintCap = cap;
}

void
anp_paint_setStrokeJoin(ANPPaint* paint, ANPPaintJoin join)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  p->paintJoin = join;
}


ANPTextEncoding
anp_paint_getTextEncoding(const ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return kUTF8_ANPTextEncoding;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  return p->textEncoding;
}

ANPPaintAlign
anp_paint_getTextAlign(const ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return kLeft_ANPPaintAlign;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  return p->paintAlign;
}

float
anp_paint_getTextSize(const ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return 0;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  return p->textSize;
}

float
anp_paint_getTextScaleX(const ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return 0;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  return p->textScaleX;
}

float
anp_paint_getTextSkewX(const ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return 0;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  return p->textSkewX;
}

void
anp_paint_setTextEncoding(ANPPaint* paint, ANPTextEncoding encoding)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  p->textEncoding = encoding;
}

void
anp_paint_setTextAlign(ANPPaint* paint, ANPPaintAlign align)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  p->paintAlign = align;  
}

void
anp_paint_setTextSize(ANPPaint* paint, float size)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  p->textSize = size;
}

void
anp_paint_setTextScaleX(ANPPaint* paint, float scale)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  p->textScaleX = scale;
}

void
anp_paint_setTextSkewX(ANPPaint* paint, float skew)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  p->textSkewX = skew;
}


/** Return the typeface in paint, or null if there is none. This does not
    modify the owner count of the returned typeface.
*/
ANPTypeface*
anp_paint_getTypeface(const ANPPaint* paint)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return NULL;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  return &p->typeface;
}


/** Set the paint's typeface. If the paint already had a non-null typeface,
    its owner count is decremented. If the new typeface is non-null, its
    owner count is incremented.
*/
void
anp_paint_setTypeface(ANPPaint* paint, ANPTypeface* typeface)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
}

/** Return the width of the text. If bounds is not null, return the bounds
    of the text in that rectangle.
*/
float
anp_paint_measureText(ANPPaint* paint, const void* text, uint32_t byteLength,
                      ANPRectF* bounds)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return 0;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;

  LOG("%s is not impl.", __PRETTY_FUNCTION__);
  return 0;
}


/** Return the number of unichars specifed by the text.
    If widths is not null, returns the array of advance widths for each
    unichar.
    If bounds is not null, returns the array of bounds for each unichar.
*/
int
anp_paint_getTextWidths(ANPPaint* paint, const void* text, uint32_t byteLength,
                        float widths[], ANPRectF bounds[])
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return 0;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
  return 0;
}


/** Return in metrics the spacing values for text, respecting the paint's
    typeface and pointsize, and return the spacing between lines
    (descent - ascent + leading). If metrics is NULL, it will be ignored.
*/
float
anp_paint_getFontMetrics(ANPPaint* paint, ANPFontMetrics* metrics)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!paint)
    return 0;

  ANPPaintPrivate* p = (ANPPaintPrivate*) paint;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
  return 0;
}


void InitPaintInterface(ANPPaintInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, newPaint);
  ASSIGN(i, deletePaint);
  ASSIGN(i, getFlags);
  ASSIGN(i, setFlags);
  ASSIGN(i, getColor);
  ASSIGN(i, setColor);
  ASSIGN(i, getStyle);
  ASSIGN(i, setStyle);
  ASSIGN(i, getStrokeWidth);
  ASSIGN(i, getStrokeMiter);
  ASSIGN(i, getStrokeCap);
  ASSIGN(i, getStrokeJoin);
  ASSIGN(i, setStrokeWidth);
  ASSIGN(i, setStrokeMiter);
  ASSIGN(i, setStrokeCap);
  ASSIGN(i, setStrokeJoin);
  ASSIGN(i, getTextEncoding);
  ASSIGN(i, getTextAlign);
  ASSIGN(i, getTextSize);
  ASSIGN(i, getTextScaleX);
  ASSIGN(i, getTextSkewX);
  ASSIGN(i, setTextEncoding);
  ASSIGN(i, setTextAlign);
  ASSIGN(i, setTextSize);
  ASSIGN(i, setTextScaleX);
  ASSIGN(i, setTextSkewX);
  ASSIGN(i, getTypeface);
  ASSIGN(i, setTypeface);
  ASSIGN(i, measureText);
  ASSIGN(i, getTextWidths);
  ASSIGN(i, getFontMetrics);
}
