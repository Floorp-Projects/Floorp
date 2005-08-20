/*
 * Copyright © 2004 David Reveman
 * 
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * David Reveman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * David Reveman makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DAVID REVEMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DAVID REVEMAN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, 
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "glitzint.h"

static void
_glitz_combine_argb_argb (glitz_composite_op_t *op)
{
  glitz_set_operator (op->gl, op->render_op);
  
  op->gl->active_texture (GLITZ_GL_TEXTURE0);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                     GLITZ_GL_REPLACE);  
  op->gl->color_4us (0x0, 0x0, 0x0, 0xffff);
  
  op->gl->active_texture (GLITZ_GL_TEXTURE1);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                     GLITZ_GL_COMBINE);
  
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_COMBINE_RGB,
                     GLITZ_GL_MODULATE);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE0_RGB,
                     GLITZ_GL_TEXTURE);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE1_RGB,
                     GLITZ_GL_PREVIOUS);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND0_RGB,
                     GLITZ_GL_SRC_COLOR);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND1_RGB,
                     GLITZ_GL_SRC_ALPHA);

  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_COMBINE_ALPHA,
                     GLITZ_GL_MODULATE);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE0_ALPHA,
                     GLITZ_GL_TEXTURE);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE1_ALPHA,
                     GLITZ_GL_PREVIOUS);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND0_ALPHA,
                     GLITZ_GL_SRC_ALPHA);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND1_ALPHA,
                     GLITZ_GL_SRC_ALPHA);
}

static void
_glitz_combine_argb_argbc (glitz_composite_op_t *op)
{
  if (op->count == 0) {
    glitz_set_operator (op->gl, op->render_op);
    
    op->gl->active_texture (GLITZ_GL_TEXTURE0);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                       GLITZ_GL_COMBINE);
    
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_COMBINE_RGB,
                       GLITZ_GL_INTERPOLATE);

    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE0_RGB,
                       GLITZ_GL_TEXTURE);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE1_RGB,
                       GLITZ_GL_PRIMARY_COLOR);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE2_RGB,
                       GLITZ_GL_PRIMARY_COLOR);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND0_RGB,
                       GLITZ_GL_SRC_COLOR);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND1_RGB,
                       GLITZ_GL_SRC_COLOR);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND2_RGB,
                       GLITZ_GL_SRC_ALPHA);

    /* we don't care about the alpha channel, so lets do something (simple?) */
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_COMBINE_ALPHA,
                       GLITZ_GL_REPLACE);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE0_ALPHA,
                       GLITZ_GL_PRIMARY_COLOR);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND0_ALPHA,
                       GLITZ_GL_SRC_ALPHA);

    
    op->gl->active_texture (GLITZ_GL_TEXTURE1);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                       GLITZ_GL_COMBINE);
  
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_COMBINE_RGB,
                       GLITZ_GL_DOT3_RGBA);
    
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE0_RGB,
                       GLITZ_GL_PREVIOUS);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE1_RGB,
                       GLITZ_GL_PRIMARY_COLOR);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND0_RGB,
                       GLITZ_GL_SRC_COLOR);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND1_RGB,
                       GLITZ_GL_SRC_COLOR);
    
    op->gl->active_texture (GLITZ_GL_TEXTURE2);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                       GLITZ_GL_MODULATE);
  }
    
  if (op->alpha_mask.red) {
    op->gl->color_4f (1.0f, 0.5f, 0.5f, 0.5f);
  } else if (op->alpha_mask.green) {
    op->gl->color_4f (0.5f, 1.0f, 0.5f, 0.5f);
  } else if (op->alpha_mask.blue) {
    op->gl->color_4f (0.5f, 0.5f, 1.0f, 0.5f);
  } else {
    op->gl->active_texture (GLITZ_GL_TEXTURE0);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                       GLITZ_GL_REPLACE);  
    op->gl->color_4us (0x0, 0x0, 0x0, 0xffff);
    
    op->gl->active_texture (GLITZ_GL_TEXTURE1);
    glitz_texture_unbind (op->gl, &op->src->texture);
    
    op->gl->active_texture (GLITZ_GL_TEXTURE2);
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                       GLITZ_GL_MODULATE);
  }
}

static void
_glitz_combine_argb_argbf (glitz_composite_op_t *op)
{
  glitz_set_operator (op->gl, op->render_op);
  glitz_filter_enable (op->mask, op);
  
  op->gl->color_4us (op->alpha_mask.red,
                     op->alpha_mask.green,
                     op->alpha_mask.blue,
                     op->alpha_mask.alpha);
}

static void
_glitz_combine_argb_solid (glitz_composite_op_t *op)
{
  glitz_set_operator (op->gl, op->render_op);
  
  if (op->alpha_mask.alpha != 0xffff) {
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                       GLITZ_GL_MODULATE);
    op->gl->color_4us (op->alpha_mask.alpha,
                       op->alpha_mask.alpha,
                       op->alpha_mask.alpha,
                       op->alpha_mask.alpha);
  } else {
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                       GLITZ_GL_REPLACE);
    op->gl->color_4us (0x0, 0x0, 0x0, 0xffff);
  }
}

static void
_glitz_combine_argb_solidc (glitz_composite_op_t *op)
{
  unsigned short alpha;

  if (op->count == 0)
    glitz_set_operator (op->gl, op->render_op);
  
  if (op->alpha_mask.red)
    alpha = op->alpha_mask.red;
  else if (op->alpha_mask.green)
    alpha = op->alpha_mask.green;
  else if (op->alpha_mask.blue)
    alpha = op->alpha_mask.blue;
  else
    alpha = op->alpha_mask.alpha;
    
  if (alpha != 0xffff) {
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                       GLITZ_GL_MODULATE);
    op->gl->color_4us (alpha, alpha, alpha, alpha);
  } else {
    op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                       GLITZ_GL_REPLACE);
    op->gl->color_4us (0x0, 0x0, 0x0, 0xffff);
  }
}

static void
_glitz_combine_argbf_solid (glitz_composite_op_t *op)
{
  if (op->count == 0) {
    glitz_set_operator (op->gl, op->render_op);
    glitz_filter_enable (op->src, op);
  }

  op->gl->color_4us (0x0, 0x0, 0x0, op->alpha_mask.alpha);
}

static void
_glitz_combine_argbf_argbc (glitz_composite_op_t *op)
{
  if (op->count == 0) {
    glitz_set_operator (op->gl, op->render_op);
    glitz_filter_enable (op->src, op);
  }
  
  op->gl->color_4us (op->alpha_mask.red,
                     op->alpha_mask.green,
                     op->alpha_mask.blue,
                     op->alpha_mask.alpha);
}

static void
_glitz_combine_argbf_solidc (glitz_composite_op_t *op)
{
  unsigned short alpha;
  
  if (op->count == 0) {
    glitz_set_operator (op->gl, op->render_op);
    glitz_filter_enable (op->src, op);
  }
  
  if (op->alpha_mask.red)
    alpha = op->alpha_mask.red;
  else if (op->alpha_mask.green)
    alpha = op->alpha_mask.green;
  else if (op->alpha_mask.blue)
    alpha = op->alpha_mask.blue;
  else
    alpha = op->alpha_mask.alpha;
  
  op->gl->color_4us (0x0, 0x0, 0x0, alpha);
}

static void
_glitz_combine_solid_solid (glitz_composite_op_t *op)
{
  glitz_set_operator (op->gl, op->render_op);
  
  op->gl->color_4us (SHORT_MULT (op->solid->red, op->alpha_mask.alpha),
                     SHORT_MULT (op->solid->green, op->alpha_mask.alpha),
                     SHORT_MULT (op->solid->blue, op->alpha_mask.alpha),
                     SHORT_MULT (op->solid->alpha, op->alpha_mask.alpha));
}

static void
_glitz_combine_solid_argb (glitz_composite_op_t *op)
{
  glitz_set_operator (op->gl, op->render_op);
  
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                     GLITZ_GL_COMBINE);
  
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_COMBINE_RGB,
                     GLITZ_GL_MODULATE);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE0_RGB,
                     GLITZ_GL_TEXTURE);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE1_RGB,
                     GLITZ_GL_PRIMARY_COLOR);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND0_RGB,
                     GLITZ_GL_SRC_ALPHA);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND1_RGB,
                     GLITZ_GL_SRC_COLOR);

  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_COMBINE_ALPHA,
                     GLITZ_GL_MODULATE);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE0_ALPHA,
                     GLITZ_GL_TEXTURE);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_SOURCE1_ALPHA,
                     GLITZ_GL_PRIMARY_COLOR);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND0_ALPHA,
                     GLITZ_GL_SRC_ALPHA);
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_OPERAND1_ALPHA,
                     GLITZ_GL_SRC_ALPHA);

  op->gl->color_4us (SHORT_MULT (op->solid->red, op->alpha_mask.alpha),
                     SHORT_MULT (op->solid->green, op->alpha_mask.alpha),
                     SHORT_MULT (op->solid->blue, op->alpha_mask.alpha),
                     SHORT_MULT (op->solid->alpha, op->alpha_mask.alpha));
}

/* This only works with the OVER operator. */
static void
_glitz_combine_solid_argbc (glitz_composite_op_t *op)
{
  glitz_color_t solid;

  solid.red = SHORT_MULT (op->solid->red, op->alpha_mask.alpha);
  solid.green = SHORT_MULT (op->solid->green, op->alpha_mask.alpha);
  solid.blue = SHORT_MULT (op->solid->blue, op->alpha_mask.alpha);
  solid.alpha = SHORT_MULT (op->solid->alpha, op->alpha_mask.alpha);

  op->gl->enable (GLITZ_GL_BLEND);
  op->gl->blend_func (GLITZ_GL_CONSTANT_COLOR, GLITZ_GL_ONE_MINUS_SRC_COLOR);
  
  if (solid.alpha > 0)
    op->gl->blend_color ((glitz_gl_clampf_t) solid.red / solid.alpha,
                         (glitz_gl_clampf_t) solid.green / solid.alpha,
                         (glitz_gl_clampf_t) solid.blue / solid.alpha,
                         1.0f);
  else
    op->gl->blend_color (1.0f, 1.0f, 1.0f, 1.0f);
  
  op->gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                     GLITZ_GL_MODULATE);
  op->gl->color_4us (solid.alpha,
                     solid.alpha,
                     solid.alpha,
                     solid.alpha);
}

static void
_glitz_combine_solid_argbf (glitz_composite_op_t *op)
{
  glitz_set_operator (op->gl, op->render_op);
  glitz_filter_enable (op->mask, op);

  op->gl->color_4us (SHORT_MULT (op->solid->red, op->alpha_mask.alpha),
                     SHORT_MULT (op->solid->green, op->alpha_mask.alpha),
                     SHORT_MULT (op->solid->blue, op->alpha_mask.alpha),
                     SHORT_MULT (op->solid->alpha, op->alpha_mask.alpha));
}

/* This only works with the OVER operator. */
static void
_glitz_combine_solid_solidc (glitz_composite_op_t *op)
{
  op->gl->enable (GLITZ_GL_BLEND);
  op->gl->blend_func (GLITZ_GL_CONSTANT_COLOR, GLITZ_GL_ONE_MINUS_SRC_COLOR);

  if (op->solid->alpha > 0)
    op->gl->blend_color ((glitz_gl_clampf_t) op->solid->red / op->solid->alpha,
                         (glitz_gl_clampf_t)
                         op->solid->green / op->solid->alpha,
                         (glitz_gl_clampf_t)
                         op->solid->blue / op->solid->alpha,
                         1.0f);
  else
    op->gl->blend_color (1.0f, 1.0f, 1.0f, 1.0f);

  op->gl->color_4us (SHORT_MULT (op->alpha_mask.red, op->solid->alpha),
                     SHORT_MULT (op->alpha_mask.green, op->solid->alpha),
                     SHORT_MULT (op->alpha_mask.blue, op->solid->alpha),
                     SHORT_MULT (op->alpha_mask.alpha, op->solid->alpha));
}

static glitz_combine_t
_glitz_combine_map[GLITZ_SURFACE_TYPES][GLITZ_SURFACE_TYPES] = {
  {
    { GLITZ_COMBINE_TYPE_NA, NULL, 0, 0 },
    { GLITZ_COMBINE_TYPE_NA, NULL, 0, 0 },
    { GLITZ_COMBINE_TYPE_NA, NULL, 0, 0 },
    { GLITZ_COMBINE_TYPE_NA, NULL, 0, 0 },
    { GLITZ_COMBINE_TYPE_NA, NULL, 0, 0 },
    { GLITZ_COMBINE_TYPE_NA, NULL, 0, 0 }
  }, {
    { GLITZ_COMBINE_TYPE_ARGB,        _glitz_combine_argb_solid,  1, 0 },
    { GLITZ_COMBINE_TYPE_ARGB_ARGB,   _glitz_combine_argb_argb,   2, 0 },
    { GLITZ_COMBINE_TYPE_ARGB_ARGBC,  _glitz_combine_argb_argbc,  3, 0 },
    { GLITZ_COMBINE_TYPE_ARGB_ARGBF,  _glitz_combine_argb_argbf,  2, 2 },
    { GLITZ_COMBINE_TYPE_ARGB_SOLID,  _glitz_combine_argb_solid,  1, 0 },
    { GLITZ_COMBINE_TYPE_ARGB_SOLIDC, _glitz_combine_argb_solidc, 1, 0 }
  }, {
    { GLITZ_COMBINE_TYPE_ARGB,        _glitz_combine_argb_solid,  1, 0 },
    { GLITZ_COMBINE_TYPE_ARGB_ARGB,   _glitz_combine_argb_argb,   2, 0 },
    { GLITZ_COMBINE_TYPE_ARGB_ARGBC,  _glitz_combine_argb_argbc,  3, 0 },
    { GLITZ_COMBINE_TYPE_ARGB_ARGBF,  _glitz_combine_argb_argbf,  2, 2 },
    { GLITZ_COMBINE_TYPE_ARGB_SOLID,  _glitz_combine_argb_solid,  1, 0 },
    { GLITZ_COMBINE_TYPE_ARGB_SOLIDC, _glitz_combine_argb_solidc, 1, 0 }
  }, {
    { GLITZ_COMBINE_TYPE_ARGBF,        _glitz_combine_argbf_solid,  1, 1 },
    { GLITZ_COMBINE_TYPE_ARGBF_ARGB,   _glitz_combine_argbf_solid,  2, 1 },
    { GLITZ_COMBINE_TYPE_ARGBF_ARGBC,  _glitz_combine_argbf_argbc,  2, 1 },
    { GLITZ_COMBINE_TYPE_NA,           NULL,                        0, 0 },
    { GLITZ_COMBINE_TYPE_ARGBF_SOLID,  _glitz_combine_argbf_solid,  1, 1 },
    { GLITZ_COMBINE_TYPE_ARGBF_SOLIDC, _glitz_combine_argbf_solidc, 1, 1 }
  }, {
    { GLITZ_COMBINE_TYPE_SOLID,        _glitz_combine_solid_solid,  0, 0 },
    { GLITZ_COMBINE_TYPE_SOLID_ARGB,   _glitz_combine_solid_argb,   1, 0 },
    { GLITZ_COMBINE_TYPE_SOLID_ARGBC,  _glitz_combine_solid_argbc,  1, 0 },
    { GLITZ_COMBINE_TYPE_SOLID_ARGBF,  _glitz_combine_solid_argbf,  1, 2 },
    { GLITZ_COMBINE_TYPE_SOLID_SOLID,  _glitz_combine_solid_solid,  0, 0 },
    { GLITZ_COMBINE_TYPE_SOLID_SOLIDC, _glitz_combine_solid_solidc, 1, 0 }
  }, {
    { GLITZ_COMBINE_TYPE_SOLID,        _glitz_combine_solid_solid,  0, 0 },
    { GLITZ_COMBINE_TYPE_SOLID_ARGB,   _glitz_combine_solid_argb,   1, 0 },
    { GLITZ_COMBINE_TYPE_SOLID_ARGBC,  _glitz_combine_solid_argbc,  1, 0 },
    { GLITZ_COMBINE_TYPE_SOLID_ARGBF,  _glitz_combine_solid_argbf,  1, 2 },
    { GLITZ_COMBINE_TYPE_SOLID_SOLID,  _glitz_combine_solid_solid,  0, 0 },
    { GLITZ_COMBINE_TYPE_SOLID_SOLIDC, _glitz_combine_solid_solidc, 1, 0 }
  }
};

#define SURFACE_WRAP(surface, feature_mask) \
  (SURFACE_REPEAT (surface)? \
   (TEXTURE_REPEATABLE (&(surface)->texture) && \
    ( \
     (!SURFACE_MIRRORED (surface)) || \
     ((feature_mask) & GLITZ_FEATURE_TEXTURE_MIRRORED_REPEAT_MASK) \
     ) \
    ) \
   : \
   ((SURFACE_PAD (surface))? \
    (TEXTURE_PADABLE (&(surface)->texture)) \
    : \
    ( \
     (!SURFACE_PROJECTIVE_TRANSFORM (surface)) || \
     ((feature_mask) & GLITZ_FEATURE_TEXTURE_BORDER_CLAMP_MASK) \
    ) \
   ) \
  )
     
static glitz_surface_type_t
_glitz_get_surface_type (glitz_surface_t *surface,
                         unsigned long feature_mask)
{
  if (surface == NULL)
    return GLITZ_SURFACE_TYPE_NULL;

  if (SURFACE_SOLID (surface) &&
      (SURFACE_REPEAT (surface) || SURFACE_PAD (surface))) {
    if (SURFACE_COMPONENT_ALPHA (surface))
      return GLITZ_SURFACE_TYPE_SOLIDC;
    else
      return GLITZ_SURFACE_TYPE_SOLID;
  }

  if (SURFACE_WRAP (surface, feature_mask)) {
    if (SURFACE_FRAGMENT_FILTER (surface)) {
      if (SURFACE_COMPONENT_ALPHA (surface))
        return GLITZ_SURFACE_TYPE_NA;

      if (feature_mask & GLITZ_FEATURE_FRAGMENT_PROGRAM_MASK)
        return GLITZ_SURFACE_TYPE_ARGBF;
    
    } else if (SURFACE_COMPONENT_ALPHA (surface)) {
      return GLITZ_SURFACE_TYPE_ARGBC;
    } else
      return GLITZ_SURFACE_TYPE_ARGB;
  }
    
  return GLITZ_SURFACE_TYPE_NA;
}

static glitz_color_t _default_alpha_mask = {
  0xffff, 0xffff, 0xffff, 0xffff
};

void
glitz_composite_op_init (glitz_composite_op_t *op,
                         glitz_operator_t render_op,
                         glitz_surface_t *src,
                         glitz_surface_t *mask,
                         glitz_surface_t *dst)
{
  glitz_surface_type_t src_type;
  glitz_surface_type_t mask_type;
  glitz_combine_t *combine;
  unsigned long feature_mask;

  op->render_op = render_op;
  op->type = GLITZ_COMBINE_TYPE_NA;
  op->combine = NULL;
  op->alpha_mask = _default_alpha_mask;
  op->src = src;
  op->mask = mask;
  op->dst = dst;
  op->count = 0;
  op->solid = NULL;
  op->per_component = 0;
  op->fp = 0;

  if (dst->attached)
  {
      op->gl = &dst->attached->backend->gl;
      feature_mask = dst->attached->backend->feature_mask;
  }
  else
  {
      op->gl = &dst->drawable->backend->gl;
      feature_mask = dst->drawable->backend->feature_mask;
  }
  
  src_type = _glitz_get_surface_type (src, feature_mask);
  if (src_type < 1)
    return;

  mask_type = _glitz_get_surface_type (mask, feature_mask);
  if (mask_type < 0)
    return;

  if (src_type == GLITZ_SURFACE_TYPE_SOLIDC)
    src_type = GLITZ_SURFACE_TYPE_SOLID;

  /* We can't do solid IN argbc OP dest, unless OP is OVER.
     But we can do argb IN argbc OP dest, so lets just not use the
     source as a solid color if this is the case. I need to figure out
     a better way to handle special cases like this. */
  if (src_type == GLITZ_SURFACE_TYPE_SOLID &&
      (mask_type == GLITZ_SURFACE_TYPE_SOLIDC ||
       mask_type == GLITZ_SURFACE_TYPE_ARGBC) &&
      render_op != GLITZ_OPERATOR_OVER)
    src_type = GLITZ_SURFACE_TYPE_ARGB;
    
  combine = &_glitz_combine_map[src_type][mask_type];
  if (!combine->type)
    return;
  
  if (dst->geometry.type == GLITZ_GEOMETRY_TYPE_BITMAP)
  {
      /* We can't do anything but solid colors with bitmaps yet. */
      if (src_type != GLITZ_SURFACE_TYPE_SOLID ||
          (mask_type != GLITZ_SURFACE_TYPE_NULL &&
           mask_type != GLITZ_SURFACE_TYPE_SOLID))
          return;
  }
  
  if (src_type == GLITZ_SURFACE_TYPE_SOLID) {
    if (SURFACE_SOLID_DAMAGE (src)) {
      glitz_surface_push_current (dst, GLITZ_ANY_CONTEXT_CURRENT);
      glitz_surface_sync_solid (src);
      glitz_surface_pop_current (dst);
    }
    op->solid = &src->solid;
    op->src = NULL;
  }
  
  if (mask_type == GLITZ_SURFACE_TYPE_SOLID) {
    if (SURFACE_SOLID_DAMAGE (mask)) {
      glitz_surface_push_current (dst, GLITZ_ANY_CONTEXT_CURRENT);
      glitz_surface_sync_solid (mask);
      glitz_surface_pop_current (dst);
    }
    op->alpha_mask = mask->solid;
    op->mask = NULL;
    op->combine = combine;
  } else if (mask_type == GLITZ_SURFACE_TYPE_SOLIDC) {
    if (SURFACE_SOLID_DAMAGE (mask)) {
      glitz_surface_push_current (dst, GLITZ_ANY_CONTEXT_CURRENT);
      glitz_surface_sync_solid (mask);
      glitz_surface_pop_current (dst);
    }
    op->alpha_mask = mask->solid;
    op->mask = NULL;
    
    if (op->src) {
        op->per_component = 4;
        op->combine = combine;
    } else if (feature_mask & GLITZ_FEATURE_BLEND_COLOR_MASK)
      op->combine = combine;
    
  } else if (mask_type != GLITZ_SURFACE_TYPE_NULL) {
    if (mask_type == GLITZ_SURFACE_TYPE_ARGBC) {
      if (op->src) {
          /* we can't do component alpha with alpha only surfaces */
        if (op->src->format->color.red_size) {
          op->per_component = 4;
          if (feature_mask & GLITZ_FEATURE_TEXTURE_ENV_COMBINE_MASK)
            op->combine = combine;
        }
      } else if (feature_mask & GLITZ_FEATURE_BLEND_COLOR_MASK)
        op->combine = combine;
    } else if (feature_mask & GLITZ_FEATURE_TEXTURE_ENV_COMBINE_MASK)
      op->combine = combine;
  } else
    op->combine = combine;

  if (!(feature_mask & GLITZ_FEATURE_MULTITEXTURE_MASK)) {
    if (op->src && op->mask)
      op->combine = NULL;
  }

  if (op->per_component &&
      (!(feature_mask & GLITZ_FEATURE_PER_COMPONENT_RENDERING_MASK)))
    op->combine = NULL;
  
  if (op->combine == combine) {
    op->type = combine->type;
    if (combine->source_shader) {
      if (combine->source_shader == 1)
        op->fp = glitz_filter_get_fragment_program (src, op);
      else
        op->fp = glitz_filter_get_fragment_program (mask, op);
      if (op->fp == 0)
        op->type = GLITZ_COMBINE_TYPE_NA;
    }
  }
}

void
glitz_composite_enable (glitz_composite_op_t *op)
{
  op->combine->enable (op);
  op->count++;
}

void
glitz_composite_disable (glitz_composite_op_t *op)
{
  if (op->fp) {
    op->gl->bind_program (GLITZ_GL_FRAGMENT_PROGRAM, 0);
    op->gl->disable (GLITZ_GL_FRAGMENT_PROGRAM);
  }
}
