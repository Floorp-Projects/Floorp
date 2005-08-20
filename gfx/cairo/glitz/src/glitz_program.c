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

#include <stdio.h>

#define EXPAND_NONE ""
#define EXPAND_2D   "2D"
#define EXPAND_RECT "RECT"
#define EXPAND_NA   "NA"

#define EXPAND_X_IN_SOLID_OP \
  { "", "", "MUL result.color, color, fragment.color.a;" }

#define EXPAND_SOLID_IN_X_OP \
  { "", "", "MUL result.color, fragment.color, color.a;" }
      
#define EXPAND_SRC_DECL "TEMP src;"
#define EXPAND_SRC_2D_IN_OP \
  { "TXP src, fragment.texcoord[1], texture[1], 2D;", \
    "DP4 color.a, color, fragment.color;", \
    "MUL result.color, src, color.a;" }
      
#define EXPAND_SRC_RECT_IN_OP \
  { "TXP src, fragment.texcoord[1], texture[1], RECT;", \
    "DP4 color.a, color, fragment.color;", \
    "MUL result.color, src, color.a;" }

#define EXPAND_MASK_DECL "TEMP mask;"
#define EXPAND_MASK_2D_IN_OP \
  { "TXP mask, fragment.texcoord[0], texture[0], 2D;", \
    "DP4 mask.a, mask, fragment.color;", \
    "MUL result.color, color, mask.a;" }

#define EXPAND_MASK_RECT_IN_OP \
  { "TXP mask, fragment.texcoord[0], texture[0], RECT;", \
    "DP4 mask.a, mask, fragment.color;", \
    "MUL result.color, color, mask.a;" }

#define EXPAND_IN_NA \
  { "NA", "NA", "NA" }

typedef struct _glitz_program_expand_t glitz_program_expand_t;

typedef struct _glitz_inop {
    char *fetch;
    char *dot_product;
    char *mult;
} glitz_in_op_t;

static const struct _glitz_program_expand_t {
  char          *texture;
  char          *declarations;
  glitz_in_op_t in;
} _program_expand_map[GLITZ_TEXTURE_LAST][GLITZ_TEXTURE_LAST][2] = {
  {
    /* [GLITZ_TEXTURE_NONE][GLITZ_TEXTURE_NONE] */
    {
      { EXPAND_NA, EXPAND_NA, EXPAND_IN_NA },
      { EXPAND_NA, EXPAND_NA, EXPAND_IN_NA }
    },
    
    /* [GLITZ_TEXTURE_NONE][GLITZ_TEXTURE_2D] */
    {
      { EXPAND_NA,   EXPAND_NA,   EXPAND_IN_NA         },
      { EXPAND_2D,   EXPAND_NONE, EXPAND_SOLID_IN_X_OP }
    },
    
    /* [GLITZ_TEXTURE_NONE][GLITZ_TEXTURE_RECT] */
    {
      { EXPAND_NA,   EXPAND_NA,   EXPAND_IN_NA         },
      { EXPAND_RECT, EXPAND_NONE, EXPAND_SOLID_IN_X_OP }
    }
  }, {
    
    /* [GLITZ_TEXTURE_2D][GLITZ_TEXTURE_NONE] */
    {
      { EXPAND_2D, EXPAND_NONE, EXPAND_X_IN_SOLID_OP },
      { EXPAND_NA, EXPAND_NA,   EXPAND_IN_NA         }
    },
    
    /* [GLITZ_TEXTURE_2D][GLITZ_TEXTURE_2D] */
    {
      { EXPAND_2D, EXPAND_MASK_DECL, EXPAND_MASK_2D_IN_OP },
      { EXPAND_2D, EXPAND_SRC_DECL,  EXPAND_SRC_2D_IN_OP  }
    },
    
    /* [GLITZ_TEXTURE_2D][GLITZ_TEXTURE_RECT] */
    {
      { EXPAND_2D,   EXPAND_MASK_DECL, EXPAND_MASK_RECT_IN_OP },
      { EXPAND_RECT, EXPAND_SRC_DECL,  EXPAND_SRC_2D_IN_OP    }
    }
  }, {
      
    /* [GLITZ_TEXTURE_RECT][GLITZ_TEXTURE_NONE] */
    {
      { EXPAND_RECT, EXPAND_NONE, EXPAND_X_IN_SOLID_OP },
      { EXPAND_NA,   EXPAND_NA,   EXPAND_IN_NA         }
    },
    
    /* [GLITZ_TEXTURE_RECT][GLITZ_TEXTURE_2D] */
    {
      { EXPAND_RECT, EXPAND_MASK_DECL, EXPAND_MASK_2D_IN_OP },
      { EXPAND_2D,   EXPAND_SRC_DECL,  EXPAND_SRC_2D_IN_OP  }
    },
    
    /* [GLITZ_TEXTURE_RECT][GLITZ_TEXTURE_RECT] */
    {
      { EXPAND_RECT, EXPAND_MASK_DECL, EXPAND_MASK_RECT_IN_OP },
      { EXPAND_RECT, EXPAND_SRC_DECL,  EXPAND_SRC_RECT_IN_OP  }
    }   
  }
};

/*
 * general convolution filter.
 */
static const char *_convolution_header[] = {
  "PARAM p[%d] = { program.local[0..%d] };",
  "ATTRIB pos = fragment.texcoord[%s];",
  "TEMP color, in, coord, position;",

  /* extra declarations */
  "%s"
  
  /* perspective divide */
  "RCP position.w, pos.w;",
  "MUL position, pos, position.w;", NULL
};

static const char *_convolution_sample_first[] = {
  "MOV coord, 0.0;",
  "ADD coord.x, position.x, p[0].x;",
  "ADD coord.y, position.y, p[0].y;",
  "TEX in, coord, texture[%s], %s;",
  "MUL color, in, p[0].z;", NULL
};

static const char *_convolution_sample[] = {
  "ADD coord.x, position.x, p[%d].x;",
  "ADD coord.y, position.y, p[%d].y;",
  "TEX in, coord, texture[%s], %s;",
  "MAD color, in, p[%d].z, color;", NULL
};


/*
 * gradient filters.
 */
static const char *_gradient_header[] = {
  "PARAM gradient = program.local[0];",
  "PARAM stops[%d] = { program.local[1..%d] };",
  "ATTRIB pos = fragment.texcoord[%s];",
  "TEMP color, second_color, stop0, stop1, position;",

  /* extra declarations */
  "%s",

  /* perspective divide */
  "RCP position.w, pos.w;",
  "MUL position, pos, position.w;", NULL
};

/*
 * linear gradient filter.
 *
 * gradient.x = start offset
 * gradient.y = 1 / length
 * gradient.z = cos (angle)
 * gradient.w = -sin (angle)
 */
static const char *_linear_gradient_calculations[] = {
  "MUL position.x, gradient.z, position.x;",
  "MAD position.x, gradient.w, position.y, position.x;",
  "SUB position.z, position.x, gradient.x;",
  "MUL position.z, position.z, gradient.y;", NULL
};

/*
 * radial gradient filter.
 *
 * gradient.x = center point X coordinate
 * gradient.y = center point Y coordinate
 * gradient.z = radius0
 * gradient.w = 1 / (radius1 - radius0)
 */
static const char *_radial_gradient_calculations[] = {
  "SUB position, position, gradient;",
  "MUL position.x, position.x, position.x;",
  "MAD position.x, position.y, position.y, position.x;",
  "RSQ position.y, position.x;",
  "RCP position.x, position.y;",
  "MUL position.x, position.x, position.x;",
  "MUL position.x, position.x, position.y;",
  "SUB position.x, position.x, gradient.z;",
  "MUL position.z, position.x, gradient.w;", NULL
};

static const char *_gradient_fill_repeat[] = {
  "FRC position.z, position.z;", NULL
};

static const char *_gradient_fill_reflect[] = {
  "FLR position.w, position.z;",
  "MUL position.w, position.w, 0.5;",
  "FLR position.w, position.w;",
  "MUL position.y, position.w, 2.0;",
  "FLR position.x, position.z;",
  "SUB position.y, position.x, position.y;",
  "FRC position.x, position.z;",
  "SUB position.x, position.x, position.y;",
  "ABS position.z, position.x;", NULL
};

static const char *_gradient_init_stops[] = {
  "MOV stop0, stops[0];",
  "MOV stop1, stops[%d];", NULL
};

static const char *_gradient_lower_stop[] = {
  "SLT position.x, stops[%d].z, position.z;",
  "CMP stop0, -position.x, stops[%d], stop0;", NULL
};

static const char *_gradient_higher_stop[] = {
  "SLT position.x, position.z, stops[%d].z;",
  "CMP stop1, -position.x, stops[%d], stop1;", NULL
};

static const char *_gradient_fetch_and_interpolate[] = {
  "TEX color, stop0, texture[%s], %s;",
  "TEX second_color, stop1, texture[%s], %s;",

  /* normalize gradient offset to color stop span */
  "SUB position.z, position.z, stop0.z;",
  "MUL_SAT position.z, position.z, stop0.w;",

  /* linear interpolation */
  "LRP color, position.z, second_color, color;", NULL
};

static struct _glitz_program_query {
  glitz_gl_enum_t query;
  glitz_gl_enum_t max_query;
  glitz_gl_int_t min;
}  _program_limits[] = {
  { GLITZ_GL_PROGRAM_INSTRUCTIONS,
    GLITZ_GL_MAX_PROGRAM_INSTRUCTIONS, 1 },
  { GLITZ_GL_PROGRAM_NATIVE_INSTRUCTIONS,
    GLITZ_GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS, 1 },
  { GLITZ_GL_PROGRAM_PARAMETERS,
    GLITZ_GL_MAX_PROGRAM_PARAMETERS, 1 },
  { GLITZ_GL_PROGRAM_NATIVE_PARAMETERS,
    GLITZ_GL_MAX_PROGRAM_NATIVE_PARAMETERS, 1 },
  { GLITZ_GL_PROGRAM_ALU_INSTRUCTIONS,
    GLITZ_GL_MAX_PROGRAM_ALU_INSTRUCTIONS, 0 },
  { GLITZ_GL_PROGRAM_TEX_INSTRUCTIONS,
    GLITZ_GL_MAX_PROGRAM_TEX_INSTRUCTIONS, 1 },
  { GLITZ_GL_PROGRAM_TEX_INDIRECTIONS,
    GLITZ_GL_MAX_PROGRAM_TEX_INDIRECTIONS, 0 },
  { GLITZ_GL_PROGRAM_NATIVE_ALU_INSTRUCTIONS,
    GLITZ_GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS, 0 },
  { GLITZ_GL_PROGRAM_NATIVE_TEX_INSTRUCTIONS,
    GLITZ_GL_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS, 0 },
  { GLITZ_GL_PROGRAM_NATIVE_TEX_INDIRECTIONS,
    GLITZ_GL_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS, 0 },
};

static glitz_bool_t
_glitz_program_under_limits (glitz_gl_proc_address_list_t *gl)
{
  int i, n_limits;

  n_limits = sizeof (_program_limits) / (sizeof (struct _glitz_program_query));

  for (i = 0; i < n_limits; i++) {
    glitz_gl_int_t value, max;

    gl->get_program_iv (GLITZ_GL_FRAGMENT_PROGRAM,
                        _program_limits[i].query, &value);
    gl->get_program_iv (GLITZ_GL_FRAGMENT_PROGRAM,
                        _program_limits[i].max_query, &max);

    if (value < _program_limits[i].min)
      return 0;
    
    if (value >= max)
      return 0;
  }

  return 1;
}

static glitz_gl_int_t
_glitz_compile_arb_fragment_program (glitz_gl_proc_address_list_t *gl,
                                     char *program_string,
                                     int n_parameters)
{
  glitz_gl_int_t error, pid = -1;
  glitz_gl_uint_t program;

  /* clear error flags */
  while (gl->get_error () != GLITZ_GL_NO_ERROR);

  gl->gen_programs (1, &program);
  gl->bind_program (GLITZ_GL_FRAGMENT_PROGRAM, program);
  gl->program_string (GLITZ_GL_FRAGMENT_PROGRAM,
                      GLITZ_GL_PROGRAM_FORMAT_ASCII,
                      strlen (program_string),
                      program_string);
  if (gl->get_error () == GLITZ_GL_NO_ERROR) {
    gl->get_integer_v (GLITZ_GL_PROGRAM_ERROR_POSITION, &error);
    
    if (error == -1) {
      glitz_gl_int_t value;
      
      gl->get_program_iv (GLITZ_GL_FRAGMENT_PROGRAM,
                          GLITZ_GL_PROGRAM_UNDER_NATIVE_LIMITS,
                          &value);
        
      if (value == GLITZ_GL_TRUE) {
        gl->get_program_iv (GLITZ_GL_FRAGMENT_PROGRAM,
                            GLITZ_GL_MAX_PROGRAM_LOCAL_PARAMETERS,
                            &value);
        
        if (value >= n_parameters) {
          if (_glitz_program_under_limits (gl))
            pid = program;
        }
      }
    }
  }
  
  if (pid == -1) {
    gl->bind_program (GLITZ_GL_FRAGMENT_PROGRAM, 0);
    gl->delete_programs (1, &program);
  }
  
  return pid;
}

static void
_string_array_to_char_array (char *dst, const char *src[])
{
  int i, n;
  
  for (i = 0; src[i]; i++) {
    n = strlen (src[i]);
    memcpy (dst, src[i], n);
    dst += n;
  }
  *dst = '\0';
}

/* these should be more than enough */
#define CONVOLUTION_BASE_SIZE   2048
#define CONVOLUTION_SAMPLE_SIZE 256

#define GRADIENT_BASE_SIZE 2048
#define GRADIENT_STOP_SIZE 256

static glitz_gl_uint_t
_glitz_create_fragment_program (glitz_composite_op_t         *op,
                                int                          fp_type,
                                int                          id,
                                const glitz_program_expand_t *expand)
{
  char buffer[1024], *program = NULL, *tex, *p = NULL;
  char *texture_type, *extra_declarations;
  const glitz_in_op_t *in;
  glitz_gl_uint_t fp;
  int i;

  switch (op->type) {
  case GLITZ_COMBINE_TYPE_ARGBF:
  case GLITZ_COMBINE_TYPE_ARGBF_SOLID:
  case GLITZ_COMBINE_TYPE_ARGBF_SOLIDC:
    i = 0;
    tex = "0";
    break;
  case GLITZ_COMBINE_TYPE_ARGB_ARGBF:
  case GLITZ_COMBINE_TYPE_SOLID_ARGBF:
    i = 1;
    tex = "0";
    break;
  case GLITZ_COMBINE_TYPE_ARGBF_ARGB:
  case GLITZ_COMBINE_TYPE_ARGBF_ARGBC:
    i = 0;
    tex = "1";
    break;
  default:
    return 0;
  }

  texture_type       = expand[i].texture;
  extra_declarations = expand[i].declarations;
  in                 = &expand[i].in;

  switch (fp_type) {
  case GLITZ_FP_CONVOLUTION:
    program = malloc (CONVOLUTION_BASE_SIZE + CONVOLUTION_SAMPLE_SIZE * id);
    if (program == NULL)
      return 0;

    p = program;
    
    p += sprintf (p, "!!ARBfp1.0");
    
    _string_array_to_char_array (buffer, _convolution_header);
    p += sprintf (p, buffer, id, id - 1, tex, extra_declarations);
    
    _string_array_to_char_array (buffer, _convolution_sample_first);
    p += sprintf (p, buffer, tex, texture_type);
    
    _string_array_to_char_array (buffer, _convolution_sample);
    for (i = 1; i < id; i++)
      p += sprintf (p, buffer, i, i, tex, texture_type, i);
    
    break;
  case GLITZ_FP_LINEAR_GRADIENT_TRANSPARENT:
  case GLITZ_FP_RADIAL_GRADIENT_TRANSPARENT:
    id += 2;
    /* fall-through */
  case GLITZ_FP_LINEAR_GRADIENT_NEAREST:
  case GLITZ_FP_LINEAR_GRADIENT_REPEAT:
  case GLITZ_FP_LINEAR_GRADIENT_REFLECT:
  case GLITZ_FP_RADIAL_GRADIENT_NEAREST:
  case GLITZ_FP_RADIAL_GRADIENT_REPEAT:
  case GLITZ_FP_RADIAL_GRADIENT_REFLECT:
    program = malloc (GRADIENT_BASE_SIZE + GRADIENT_STOP_SIZE * id);
    if (program == NULL)
      return 0;

    p = program;
    
    p += sprintf (p, "!!ARBfp1.0");
    
    _string_array_to_char_array (buffer, _gradient_header);
    p += sprintf (p, buffer, id, id, tex, extra_declarations);
    
    switch (fp_type) {
    case GLITZ_FP_LINEAR_GRADIENT_TRANSPARENT:
    case GLITZ_FP_LINEAR_GRADIENT_NEAREST:
    case GLITZ_FP_LINEAR_GRADIENT_REPEAT:
    case GLITZ_FP_LINEAR_GRADIENT_REFLECT:
      _string_array_to_char_array (buffer, _linear_gradient_calculations);
      break;
    default:
      _string_array_to_char_array (buffer, _radial_gradient_calculations);
      break;
    }

    p += sprintf (p, buffer);
    
    switch (fp_type) {
    case GLITZ_FP_LINEAR_GRADIENT_REPEAT:
    case GLITZ_FP_RADIAL_GRADIENT_REPEAT:
      _string_array_to_char_array (buffer, _gradient_fill_repeat);
      p += sprintf (p, buffer);
      break;
    case GLITZ_FP_LINEAR_GRADIENT_REFLECT:
    case GLITZ_FP_RADIAL_GRADIENT_REFLECT:
      _string_array_to_char_array (buffer, _gradient_fill_reflect);
      p += sprintf (p, buffer);
      break;
    default:
      break;
    }
    
    _string_array_to_char_array (buffer, _gradient_init_stops);
    p += sprintf (p, buffer, id - 1);

    _string_array_to_char_array (buffer, _gradient_lower_stop);
    for (i = 1; i < (id - 1); i++)
      p += sprintf (p, buffer, i, i);
    
    _string_array_to_char_array (buffer, _gradient_higher_stop);
    for (i = 1; i < (id - 1); i++)
      p += sprintf (p, buffer, id - i - 1, id - i - 1);
    
    _string_array_to_char_array (buffer, _gradient_fetch_and_interpolate);
    p += sprintf (p, buffer, tex, texture_type, tex, texture_type);
    
    id++;
    break;
  default:
    return 0;
  }

  if (program == NULL)
    return 0;
  
  p += sprintf (p, "%s", in->fetch);
  if (op->per_component)
      p += sprintf (p, "%s", in->dot_product);
  p += sprintf (p, "%s", in->mult);
  sprintf (p, "END");

  fp = _glitz_compile_arb_fragment_program (op->gl, program, id);
  
  free (program);

  return fp;
}
                      
void
glitz_program_map_init (glitz_program_map_t *map)
{
  memset (map, 0, sizeof (glitz_program_map_t));
}

void
glitz_program_map_fini (glitz_gl_proc_address_list_t *gl,
                        glitz_program_map_t          *map)
{
  glitz_gl_uint_t program;
  int i, j, k, x, y;
    
  for (i = 0; i < GLITZ_COMBINE_TYPES; i++) {
    for (j = 0; j < GLITZ_FP_TYPES; j++) {
      for (x = 0; x < GLITZ_TEXTURE_LAST; x++) {
        for (y = 0; y < GLITZ_TEXTURE_LAST; y++) {
          glitz_program_t *p = &map->filters[i][j].fp[x][y];
          
          if (p->name) {
            for (k = 0; k < p->size; k++)
              if (p->name[k] > 0) {
                program = p->name[k];
                gl->delete_programs (1, &program);
              }
            
            free (p->name);
          }
        }
      }
    }
  }
}

#define TEXTURE_INDEX(surface) \
  ((surface)? \
   (((surface)->texture.target == GLITZ_GL_TEXTURE_2D)? \
     GLITZ_TEXTURE_2D: \
     GLITZ_TEXTURE_RECT \
   ) : \
   GLITZ_TEXTURE_NONE \
  )

glitz_gl_uint_t
glitz_get_fragment_program (glitz_composite_op_t *op,
                            int                  fp_type,
                            int                  id)
{
  glitz_program_map_t *map;
  glitz_program_t *program;
  int t0 = TEXTURE_INDEX (op->src);
  int t1 = TEXTURE_INDEX (op->mask);

  map = op->dst->drawable->backend->program_map;
  program = &map->filters[op->type][fp_type].fp[t0][t1];
  
  if (program->size < id) {
    int old_size;
    
    program->name = realloc (program->name,
                             id * sizeof (glitz_gl_uint_t));
    if (program->name == NULL) {
      glitz_surface_status_add (op->dst, GLITZ_STATUS_NO_MEMORY_MASK);
      return 0;
    }
    old_size = program->size;
    program->size = id;
    memset (program->name + old_size, 0,
            (program->size - old_size) * sizeof (glitz_gl_uint_t));
  }
  
  if (program->name[id - 1] == 0) {
    glitz_surface_push_current (op->dst, GLITZ_CONTEXT_CURRENT);
    
    program->name[id - 1] =
      _glitz_create_fragment_program (op, fp_type, id,
                                      _program_expand_map[t0][t1]);
    
    glitz_surface_pop_current (op->dst);
  }

  if (program->name[id - 1] > 0)
    return program->name[id - 1];
  else
    return 0;
}
