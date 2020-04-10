/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>

#ifdef __MACH__
#  include <mach/mach.h>
#  include <mach/mach_time.h>
#else
#  include <time.h>
#endif

#ifdef NDEBUG
#  define debugf(...)
#else
#  define debugf(...) printf(__VA_ARGS__)
#endif

#ifdef _WIN32
#  define ALWAYS_INLINE __forceinline
#else
#  define ALWAYS_INLINE __attribute__((always_inline)) inline
#endif

#define UNREACHABLE __builtin_unreachable()

#define UNUSED __attribute__((unused))

#ifdef MOZILLA_CLIENT
#  define IMPLICIT __attribute__((annotate("moz_implicit")))
#else
#  define IMPLICIT
#endif

#include "gl_defs.h"
#include "glsl.h"
#include "program.h"

using namespace glsl;

struct IntRect {
  int x;
  int y;
  int width;
  int height;
};

struct VertexAttrib {
  size_t size = 0;  // in bytes
  GLenum type = 0;
  bool normalized = false;
  GLsizei stride = 0;
  GLuint offset = 0;
  bool enabled = false;
  GLuint divisor = 0;
  int vertex_array = 0;
  int vertex_buffer = 0;
  char* buf = nullptr;  // XXX: this can easily dangle
  size_t buf_size = 0;  // this will let us bounds check
};

static int bytes_for_internal_format(GLenum internal_format) {
  switch (internal_format) {
    case GL_RGBA32F:
      return 4 * 4;
    case GL_RGBA32I:
      return 4 * 4;
    case GL_RGBA8:
    case GL_BGRA8:
    case GL_RGBA:
      return 4;
    case GL_R8:
    case GL_RED:
      return 1;
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT16:
      return 2;
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32:
      return 4;
    default:
      debugf("internal format: %x\n", internal_format);
      assert(0);
      return 0;
  }
}

static inline int aligned_stride(int row_bytes) { return (row_bytes + 3) & ~3; }

static TextureFormat gl_format_to_texture_format(int type) {
  switch (type) {
    case GL_RGBA32F:
      return TextureFormat::RGBA32F;
    case GL_RGBA32I:
      return TextureFormat::RGBA32I;
    case GL_RGBA8:
      return TextureFormat::RGBA8;
    case GL_R8:
      return TextureFormat::R8;
    default:
      assert(0);
      return TextureFormat::RGBA8;
  }
}

struct Query {
  uint64_t value = 0;
};

struct Buffer {
  char* buf = nullptr;
  size_t size = 0;

  bool allocate(size_t new_size) {
    if (new_size != size) {
      char* new_buf = (char*)realloc(buf, new_size);
      assert(new_buf);
      if (new_buf) {
        buf = new_buf;
        size = new_size;
        return true;
      }
      cleanup();
    }
    return false;
  }

  void cleanup() {
    if (buf) {
      free(buf);
      buf = nullptr;
      size = 0;
    }
  }

  ~Buffer() { cleanup(); }
};

struct Framebuffer {
  GLuint color_attachment = 0;
  GLint layer = 0;
  GLuint depth_attachment = 0;
};

struct Renderbuffer {
  GLuint texture = 0;

  void on_erase();
};

TextureFilter gl_filter_to_texture_filter(int type) {
  switch (type) {
    case GL_NEAREST:
      return TextureFilter::NEAREST;
    case GL_NEAREST_MIPMAP_LINEAR:
      return TextureFilter::NEAREST;
    case GL_NEAREST_MIPMAP_NEAREST:
      return TextureFilter::NEAREST;
    case GL_LINEAR:
      return TextureFilter::LINEAR;
    case GL_LINEAR_MIPMAP_LINEAR:
      return TextureFilter::LINEAR;
    case GL_LINEAR_MIPMAP_NEAREST:
      return TextureFilter::LINEAR;
    default:
      assert(0);
      return TextureFilter::NEAREST;
  }
}

struct Texture {
  int levels = 0;
  GLenum internal_format = 0;
  int width = 0;
  int height = 0;
  int depth = 0;
  char* buf = nullptr;
  size_t buf_size = 0;
  GLenum min_filter = GL_NEAREST;
  GLenum mag_filter = GL_LINEAR;

  enum FLAGS {
    SHOULD_FREE = 1 << 1,
  };
  int flags = SHOULD_FREE;
  bool should_free() const { return bool(flags & SHOULD_FREE); }

  void set_flag(int flag, bool val) {
    if (val) {
      flags |= flag;
    } else {
      flags &= ~flag;
    }
  }
  void set_should_free(bool val) { set_flag(SHOULD_FREE, val); }

  int delay_clear = 0;
  uint32_t clear_val = 0;
  uint32_t* cleared_rows = nullptr;

  void enable_delayed_clear(uint32_t val) {
    delay_clear = height;
    clear_val = val;
    if (!cleared_rows) {
      cleared_rows = new uint32_t[(height + 31) / 32];
    }
    memset(cleared_rows, 0, ((height + 31) / 32) * sizeof(uint32_t));
    if (height & 31) {
      cleared_rows[height / 32] = ~0U << (height & 31);
    }
  }

  void disable_delayed_clear() {
    if (cleared_rows) {
      delete[] cleared_rows;
      cleared_rows = nullptr;
      delay_clear = 0;
    }
  }

  int bpp() const { return bytes_for_internal_format(internal_format); }

  size_t stride(int b = 0, int min_width = 0) const {
    return aligned_stride((b ? b : bpp()) * max(width, min_width));
  }

  size_t layer_stride(int b = 0, int min_width = 0, int min_height = 0) const {
    return stride(b ? b : bpp(), min_width) * max(height, min_height);
  }

  bool allocate(bool force = false, int min_width = 0, int min_height = 0) {
    if ((!buf || force) && should_free()) {
      size_t size =
          layer_stride(bpp(), min_width, min_height) * max(depth, 1) * levels;
      if (!buf || size > buf_size) {
        char* new_buf = (char*)realloc(buf, size + sizeof(Float));
        assert(new_buf);
        if (new_buf) {
          buf = new_buf;
          buf_size = size;
          return true;
        }
        cleanup();
      }
    }
    return false;
  }

  void cleanup() {
    if (buf && should_free()) {
      free(buf);
      buf = nullptr;
      buf_size = 0;
    }
    disable_delayed_clear();
  }

  ~Texture() { cleanup(); }
};

#define MAX_ATTRIBS 16
#define NULL_ATTRIB 15
struct VertexArray {
  VertexAttrib attribs[MAX_ATTRIBS];
  int max_attrib = -1;

  void validate();
};

struct Shader {
  GLenum type = 0;
  ProgramLoader loader = nullptr;
};

struct Program {
  ProgramImpl* impl = nullptr;
  VertexShaderImpl* vert_impl = nullptr;
  FragmentShaderImpl* frag_impl = nullptr;
  bool deleted = false;

  ~Program() {
    delete impl;
    delete vert_impl;
    delete frag_impl;
  }
};

// for GL defines to fully expand
#define CONCAT_KEY(prefix, x, y, z, w, ...) prefix##x##y##z##w
#define BLEND_KEY(...) CONCAT_KEY(BLEND_, __VA_ARGS__, 0, 0)
#define FOR_EACH_BLEND_KEY(macro)                                              \
  macro(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE)                  \
      macro(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, 0, 0)                              \
          macro(GL_ZERO, GL_ONE_MINUS_SRC_COLOR, 0, 0)                         \
              macro(GL_ZERO, GL_ONE_MINUS_SRC_COLOR, GL_ZERO, GL_ONE)          \
                  macro(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA, 0, 0) macro(          \
                      GL_ZERO, GL_SRC_COLOR, 0, 0) macro(GL_ONE, GL_ONE, 0, 0) \
                      macro(GL_ONE, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)    \
                          macro(GL_ONE, GL_ZERO, 0, 0) macro(                  \
                              GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ZERO, GL_ONE) \
                              macro(GL_CONSTANT_COLOR, GL_ONE_MINUS_SRC_COLOR, \
                                    0, 0)                                      \
                                  macro(GL_ONE, GL_ONE_MINUS_SRC1_COLOR, 0, 0)

#define DEFINE_BLEND_KEY(...) BLEND_KEY(__VA_ARGS__),
enum BlendKey : uint8_t {
  BLEND_KEY_NONE = 0,
  FOR_EACH_BLEND_KEY(DEFINE_BLEND_KEY)
};

const size_t MAX_TEXTURE_UNITS = 16;

template <typename T>
static inline bool unlink(T& binding, T n) {
  if (binding == n) {
    binding = 0;
    return true;
  }
  return false;
}

template <typename O>
struct ObjectStore {
  O** objects = nullptr;
  size_t size = 0;
  // reserve object 0 as null
  size_t first_free = 1;
  O invalid;

  ~ObjectStore() {
    if (objects) {
      for (size_t i = 0; i < size; i++) delete objects[i];
      free(objects);
    }
  }

  bool grow(size_t i) {
    size_t new_size = size ? size : 8;
    while (new_size <= i) new_size += new_size / 2;
    O** new_objects = (O**)realloc(objects, new_size * sizeof(O*));
    assert(new_objects);
    if (!new_objects) return false;
    while (size < new_size) new_objects[size++] = nullptr;
    objects = new_objects;
    return true;
  }

  void insert(size_t i, const O& o) {
    if (i >= size && !grow(i)) return;
    if (!objects[i]) objects[i] = new O(o);
  }

  size_t next_free() {
    size_t i = first_free;
    while (i < size && objects[i]) i++;
    first_free = i;
    return i;
  }

  size_t insert(const O& o = O()) {
    size_t i = next_free();
    insert(i, o);
    return i;
  }

  O& operator[](size_t i) {
    insert(i, O());
    return i < size ? *objects[i] : invalid;
  }

  O* find(size_t i) const { return i < size ? objects[i] : nullptr; }

  template <typename T> void on_erase(T* o, ...) {}
  template <typename T> void on_erase(T* o, decltype(&T::on_erase)) {
    o->on_erase();
  }

  bool erase(size_t i) {
    if (i < size && objects[i]) {
      on_erase(objects[i], nullptr);
      delete objects[i];
      objects[i] = nullptr;
      if (i < first_free) first_free = i;
      return true;
    }
    return false;
  }

  O** begin() const { return objects; }
  O** end() const { return &objects[size]; }
};

struct Context {
  ObjectStore<Query> queries;
  ObjectStore<Buffer> buffers;
  ObjectStore<Texture> textures;
  ObjectStore<VertexArray> vertex_arrays;
  ObjectStore<Framebuffer> framebuffers;
  ObjectStore<Renderbuffer> renderbuffers;
  ObjectStore<Shader> shaders;
  ObjectStore<Program> programs;

  IntRect viewport = {0, 0, 0, 0};

  bool blend = false;
  GLenum blendfunc_srgb = GL_ONE;
  GLenum blendfunc_drgb = GL_ZERO;
  GLenum blendfunc_sa = GL_ONE;
  GLenum blendfunc_da = GL_ZERO;
  GLenum blend_equation = GL_FUNC_ADD;
  V8<uint16_t> blendcolor = 0;
  BlendKey blend_key = BLEND_KEY_NONE;

  bool depthtest = false;
  bool depthmask = true;
  GLenum depthfunc = GL_LESS;

  bool scissortest = false;
  IntRect scissor = {0, 0, 0, 0};

  uint32_t clearcolor = 0;
  GLdouble cleardepth = 1;

  int unpack_row_length = 0;

  int shaded_rows = 0;
  int shaded_pixels = 0;

  struct TextureUnit {
    GLuint texture_2d_binding = 0;
    GLuint texture_3d_binding = 0;
    GLuint texture_2d_array_binding = 0;
    GLuint texture_rectangle_binding = 0;

    void unlink(GLuint n) {
      ::unlink(texture_2d_binding, n);
      ::unlink(texture_3d_binding, n);
      ::unlink(texture_2d_array_binding, n);
      ::unlink(texture_rectangle_binding, n);
    }
  };
  TextureUnit texture_units[MAX_TEXTURE_UNITS];
  int active_texture_unit = 0;

  GLuint current_program = 0;

  GLuint current_vertex_array = 0;
  bool validate_vertex_array = true;

  GLuint pixel_pack_buffer_binding = 0;
  GLuint pixel_unpack_buffer_binding = 0;
  GLuint array_buffer_binding = 0;
  GLuint element_array_buffer_binding = 0;
  GLuint time_elapsed_query = 0;
  GLuint samples_passed_query = 0;
  GLuint renderbuffer_binding = 0;
  GLuint draw_framebuffer_binding = 0;
  GLuint read_framebuffer_binding = 0;
  GLuint unknown_binding = 0;

  GLuint& get_binding(GLenum name) {
    switch (name) {
      case GL_PIXEL_PACK_BUFFER:
        return pixel_pack_buffer_binding;
      case GL_PIXEL_UNPACK_BUFFER:
        return pixel_unpack_buffer_binding;
      case GL_ARRAY_BUFFER:
        return array_buffer_binding;
      case GL_ELEMENT_ARRAY_BUFFER:
        return element_array_buffer_binding;
      case GL_TEXTURE_2D:
        return texture_units[active_texture_unit].texture_2d_binding;
      case GL_TEXTURE_2D_ARRAY:
        return texture_units[active_texture_unit].texture_2d_array_binding;
      case GL_TEXTURE_3D:
        return texture_units[active_texture_unit].texture_3d_binding;
      case GL_TEXTURE_RECTANGLE:
        return texture_units[active_texture_unit].texture_rectangle_binding;
      case GL_TIME_ELAPSED:
        return time_elapsed_query;
      case GL_SAMPLES_PASSED:
        return samples_passed_query;
      case GL_RENDERBUFFER:
        return renderbuffer_binding;
      case GL_DRAW_FRAMEBUFFER:
        return draw_framebuffer_binding;
      case GL_READ_FRAMEBUFFER:
        return read_framebuffer_binding;
      default:
        debugf("unknown binding %x\n", name);
        assert(false);
        return unknown_binding;
    }
  }

  Texture& get_texture(sampler2D, int unit) {
    return textures[texture_units[unit].texture_2d_binding];
  }

  Texture& get_texture(isampler2D, int unit) {
    return textures[texture_units[unit].texture_2d_binding];
  }

  Texture& get_texture(sampler2DArray, int unit) {
    return textures[texture_units[unit].texture_2d_array_binding];
  }

  Texture& get_texture(sampler2DRect, int unit) {
    return textures[texture_units[unit].texture_rectangle_binding];
  }
};
static Context* ctx = nullptr;
static ProgramImpl* program_impl = nullptr;
static VertexShaderImpl* vertex_shader = nullptr;
static FragmentShaderImpl* fragment_shader = nullptr;
static BlendKey blend_key = BLEND_KEY_NONE;

static void prepare_texture(Texture& t, const IntRect* skip = nullptr);

template <typename S>
static inline void init_depth(S* s, Texture& t) {
  s->depth = t.depth;
  s->height_stride = s->stride * t.height;
}

template <typename S>
static inline void init_filter(S* s, Texture& t) {
  s->filter = gl_filter_to_texture_filter(t.mag_filter);
}

template <typename S>
static inline void init_sampler(S* s, Texture& t) {
  prepare_texture(t);
  s->width = t.width;
  s->height = t.height;
  int bpp = t.bpp();
  s->stride = t.stride(bpp);
  if (bpp >= 4) s->stride /= 4;
  // Use uint32_t* for easier sampling, but need to cast to uint8_t* for formats
  // with bpp < 4.
  s->buf = (uint32_t*)t.buf;
  s->format = gl_format_to_texture_format(t.internal_format);
}

template <typename S>
S* lookup_sampler(S* s, int texture) {
  Texture& t = ctx->get_texture(s, texture);
  if (!t.buf) {
    *s = S();
  } else {
    init_sampler(s, t);
    init_filter(s, t);
  }
  return s;
}

template <typename S>
S* lookup_isampler(S* s, int texture) {
  Texture& t = ctx->get_texture(s, texture);
  if (!t.buf) {
    *s = S();
  } else {
    init_sampler(s, t);
  }
  return s;
}

template <typename S>
S* lookup_sampler_array(S* s, int texture) {
  Texture& t = ctx->get_texture(s, texture);
  if (!t.buf) {
    *s = S();
  } else {
    init_sampler(s, t);
    init_depth(s, t);
    init_filter(s, t);
  }
  return s;
}

int bytes_per_type(GLenum type) {
  switch (type) {
    case GL_INT:
      return 4;
    case GL_FLOAT:
      return 4;
    case GL_UNSIGNED_SHORT:
      return 2;
    case GL_UNSIGNED_BYTE:
      return 1;
    default:
      assert(0);
      return 0;
  }
}

template <typename S>
static inline S load_attrib_scalar(const char* src, size_t size, GLenum type,
                                   bool normalized) {
  if (sizeof(S) <= size) {
    return *reinterpret_cast<const S*>(src);
  }
  S scalar = {0};
  if (type == GL_UNSIGNED_SHORT) {
    if (normalized) {
      for (size_t i = 0; i < size / sizeof(uint16_t); i++) {
        typename ElementType<S>::ty x =
            reinterpret_cast<const uint16_t*>(src)[i];
        put_nth_component(scalar, i, x * (1.0f / 0xFFFF));
      }
    } else {
      for (size_t i = 0; i < size / sizeof(uint16_t); i++) {
        typename ElementType<S>::ty x =
            reinterpret_cast<const uint16_t*>(src)[i];
        put_nth_component(scalar, i, x);
      }
    }
  } else {
    assert(sizeof(typename ElementType<S>::ty) == bytes_per_type(type));
    memcpy(&scalar, src, size);
  }
  return scalar;
}

template <typename T>
void load_attrib(T& attrib, VertexAttrib& va, uint16_t* indices, int start,
                 int instance, int count) {
  typedef decltype(force_scalar(attrib)) scalar_type;
  if (!va.enabled) {
    attrib = T(scalar_type{0});
  } else if (va.divisor == 1) {
    char* src = (char*)va.buf + va.stride * instance + va.offset;
    assert(src + va.size <= va.buf + va.buf_size);
    attrib = T(
        load_attrib_scalar<scalar_type>(src, va.size, va.type, va.normalized));
  } else if (va.divisor == 0) {
    if (!indices) return;
    assert(sizeof(typename ElementType<T>::ty) == bytes_per_type(va.type));
    assert(count == 3 || count == 4);
    attrib = (T){
        load_attrib_scalar<scalar_type>(
            (char*)va.buf + va.stride * indices[start + 0] + va.offset, va.size,
            va.type, va.normalized),
        load_attrib_scalar<scalar_type>(
            (char*)va.buf + va.stride * indices[start + 1] + va.offset, va.size,
            va.type, va.normalized),
        load_attrib_scalar<scalar_type>(
            (char*)va.buf + va.stride * indices[start + 2] + va.offset, va.size,
            va.type, va.normalized),
        load_attrib_scalar<scalar_type>(
            (char*)va.buf + va.stride * indices[start + (count > 3 ? 3 : 2)] +
                va.offset,
            va.size, va.type, va.normalized),
    };
  } else {
    assert(false);
  }
}

template <typename T>
void load_flat_attrib(T& attrib, VertexAttrib& va, uint16_t* indices, int start,
                      int instance, int count) {
  typedef decltype(force_scalar(attrib)) scalar_type;
  if (!va.enabled) {
    attrib = T{0};
    return;
  }
  char* src = nullptr;
  if (va.divisor == 1) {
    src = (char*)va.buf + va.stride * instance + va.offset;
  } else if (va.divisor == 0) {
    if (!indices) return;
    src = (char*)va.buf + va.stride * indices[start] + va.offset;
  } else {
    assert(false);
  }
  assert(src + va.size <= va.buf + va.buf_size);
  attrib =
      T(load_attrib_scalar<scalar_type>(src, va.size, va.type, va.normalized));
}

void setup_program(GLuint program) {
  if (!program) {
    program_impl = nullptr;
    vertex_shader = nullptr;
    fragment_shader = nullptr;
    return;
  }
  Program& p = ctx->programs[program];
  assert(p.impl);
  assert(p.vert_impl);
  assert(p.frag_impl);
  program_impl = p.impl;
  vertex_shader = p.vert_impl;
  fragment_shader = p.frag_impl;
}

extern ProgramLoader load_shader(const char* name);

extern "C" {

void UseProgram(GLuint program) {
  if (ctx->current_program && program != ctx->current_program) {
    auto* p = ctx->programs.find(ctx->current_program);
    if (p && p->deleted) {
      ctx->programs.erase(ctx->current_program);
    }
  }
  ctx->current_program = program;
  setup_program(program);
}

void SetViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  ctx->viewport.x = x;
  ctx->viewport.y = y;
  ctx->viewport.width = width;
  ctx->viewport.height = height;
}

void Enable(GLenum cap) {
  switch (cap) {
    case GL_BLEND:
      ctx->blend = true;
      blend_key = ctx->blend_key;
      break;
    case GL_DEPTH_TEST:
      ctx->depthtest = true;
      break;
    case GL_SCISSOR_TEST:
      ctx->scissortest = true;
      break;
  }
}

void Disable(GLenum cap) {
  switch (cap) {
    case GL_BLEND:
      ctx->blend = false;
      blend_key = BLEND_KEY_NONE;
      break;
    case GL_DEPTH_TEST:
      ctx->depthtest = false;
      break;
    case GL_SCISSOR_TEST:
      ctx->scissortest = false;
      break;
  }
}

GLenum GetError() { return GL_NO_ERROR; }

static const char* const extensions[] = {
    "GL_ARB_blend_func_extended", "GL_ARB_copy_image",
    "GL_ARB_draw_instanced",      "GL_ARB_explicit_attrib_location",
    "GL_ARB_instanced_arrays",    "GL_ARB_invalidate_subdata",
    "GL_ARB_texture_storage",     "GL_EXT_timer_query",
};

void GetIntegerv(GLenum pname, GLint* params) {
  assert(params);
  switch (pname) {
    case GL_MAX_TEXTURE_UNITS:
    case GL_MAX_TEXTURE_IMAGE_UNITS:
      params[0] = MAX_TEXTURE_UNITS;
      break;
    case GL_MAX_TEXTURE_SIZE:
      params[0] = 1 << 15;
      break;
    case GL_MAX_ARRAY_TEXTURE_LAYERS:
      params[0] = 1 << 15;
      break;
    case GL_READ_FRAMEBUFFER_BINDING:
      params[0] = ctx->read_framebuffer_binding;
      break;
    case GL_DRAW_FRAMEBUFFER_BINDING:
      params[0] = ctx->draw_framebuffer_binding;
      break;
    case GL_PIXEL_PACK_BUFFER_BINDING:
      params[0] = ctx->pixel_pack_buffer_binding;
      break;
    case GL_PIXEL_UNPACK_BUFFER_BINDING:
      params[0] = ctx->pixel_unpack_buffer_binding;
      break;
    case GL_NUM_EXTENSIONS:
      params[0] = sizeof(extensions) / sizeof(extensions[0]);
      break;
    default:
      debugf("unhandled glGetIntegerv parameter %x\n", pname);
      assert(false);
  }
}

void GetBooleanv(GLenum pname, GLboolean* params) {
  assert(params);
  switch (pname) {
    case GL_DEPTH_WRITEMASK:
      params[0] = ctx->depthmask;
      break;
    default:
      debugf("unhandled glGetBooleanv parameter %x\n", pname);
      assert(false);
  }
}

const char* GetString(GLenum name) {
  switch (name) {
    case GL_VENDOR:
      return "Mozilla Gfx";
    case GL_RENDERER:
      return "Software WebRender";
    case GL_VERSION:
      return "3.2";
    default:
      debugf("unhandled glGetString parameter %x\n", name);
      assert(false);
      return nullptr;
  }
}

const char* GetStringi(GLenum name, GLuint index) {
  switch (name) {
    case GL_EXTENSIONS:
      if (index >= sizeof(extensions) / sizeof(extensions[0])) {
        return nullptr;
      }
      return extensions[index];
    default:
      debugf("unhandled glGetStringi parameter %x\n", name);
      assert(false);
      return nullptr;
  }
}

GLenum remap_blendfunc(GLenum rgb, GLenum a) {
  switch (a) {
    case GL_SRC_ALPHA:
      if (rgb == GL_SRC_COLOR) a = GL_SRC_COLOR;
      break;
    case GL_ONE_MINUS_SRC_ALPHA:
      if (rgb == GL_ONE_MINUS_SRC_COLOR) a = GL_ONE_MINUS_SRC_COLOR;
      break;
    case GL_DST_ALPHA:
      if (rgb == GL_DST_COLOR) a = GL_DST_COLOR;
      break;
    case GL_ONE_MINUS_DST_ALPHA:
      if (rgb == GL_ONE_MINUS_DST_COLOR) a = GL_ONE_MINUS_DST_COLOR;
      break;
    case GL_CONSTANT_ALPHA:
      if (rgb == GL_CONSTANT_COLOR) a = GL_CONSTANT_COLOR;
      break;
    case GL_ONE_MINUS_CONSTANT_ALPHA:
      if (rgb == GL_ONE_MINUS_CONSTANT_COLOR) a = GL_ONE_MINUS_CONSTANT_COLOR;
      break;
    case GL_SRC_COLOR:
      if (rgb == GL_SRC_ALPHA) a = GL_SRC_ALPHA;
      break;
    case GL_ONE_MINUS_SRC_COLOR:
      if (rgb == GL_ONE_MINUS_SRC_ALPHA) a = GL_ONE_MINUS_SRC_ALPHA;
      break;
    case GL_DST_COLOR:
      if (rgb == GL_DST_ALPHA) a = GL_DST_ALPHA;
      break;
    case GL_ONE_MINUS_DST_COLOR:
      if (rgb == GL_ONE_MINUS_DST_ALPHA) a = GL_ONE_MINUS_DST_ALPHA;
      break;
    case GL_CONSTANT_COLOR:
      if (rgb == GL_CONSTANT_ALPHA) a = GL_CONSTANT_ALPHA;
      break;
    case GL_ONE_MINUS_CONSTANT_COLOR:
      if (rgb == GL_ONE_MINUS_CONSTANT_ALPHA) a = GL_ONE_MINUS_CONSTANT_ALPHA;
      break;
    case GL_SRC1_ALPHA:
      if (rgb == GL_SRC1_COLOR) a = GL_SRC1_COLOR;
      break;
    case GL_ONE_MINUS_SRC1_ALPHA:
      if (rgb == GL_ONE_MINUS_SRC1_COLOR) a = GL_ONE_MINUS_SRC1_COLOR;
      break;
    case GL_SRC1_COLOR:
      if (rgb == GL_SRC1_ALPHA) a = GL_SRC1_ALPHA;
      break;
    case GL_ONE_MINUS_SRC1_COLOR:
      if (rgb == GL_ONE_MINUS_SRC1_ALPHA) a = GL_ONE_MINUS_SRC1_ALPHA;
      break;
  }
  return a;
}

void BlendFunc(GLenum srgb, GLenum drgb, GLenum sa, GLenum da) {
  ctx->blendfunc_srgb = srgb;
  ctx->blendfunc_drgb = drgb;
  sa = remap_blendfunc(srgb, sa);
  da = remap_blendfunc(drgb, da);
  ctx->blendfunc_sa = sa;
  ctx->blendfunc_da = da;

#define HASH_BLEND_KEY(x, y, z, w) ((x << 4) | (y) | (z << 24) | (w << 20))
  int hash = HASH_BLEND_KEY(srgb, drgb, 0, 0);
  if (srgb != sa || drgb != da) hash |= HASH_BLEND_KEY(0, 0, sa, da);
  switch (hash) {
#define MAP_BLEND_KEY(...)                   \
  case HASH_BLEND_KEY(__VA_ARGS__):          \
    ctx->blend_key = BLEND_KEY(__VA_ARGS__); \
    break;
    FOR_EACH_BLEND_KEY(MAP_BLEND_KEY)
    default:
      debugf("blendfunc: %x, %x, separate: %x, %x\n", srgb, drgb, sa, da);
      assert(false);
      break;
  }

  if (ctx->blend) {
    blend_key = ctx->blend_key;
  }
}

void BlendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
  I32 c = roundfast((Float){b, g, r, a}, 255.49f);
  ctx->blendcolor = CONVERT(c, U16).xyzwxyzw;
}

void BlendEquation(GLenum mode) {
  assert(mode == GL_FUNC_ADD);
  ctx->blend_equation = mode;
}

void DepthMask(GLboolean flag) { ctx->depthmask = flag; }

void DepthFunc(GLenum func) {
  switch (func) {
    case GL_LESS:
    case GL_LEQUAL:
      break;
    default:
      assert(false);
  }
  ctx->depthfunc = func;
}

void SetScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  ctx->scissor.x = x;
  ctx->scissor.y = y;
  ctx->scissor.width = width;
  ctx->scissor.height = height;
}

void ClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
  I32 c = roundfast((Float){b, g, r, a}, 255.49f);
  ctx->clearcolor = bit_cast<uint32_t>(CONVERT(c, U8));
}

void ClearDepth(GLdouble depth) { ctx->cleardepth = depth; }

void ActiveTexture(GLenum texture) {
  assert(texture >= GL_TEXTURE0);
  assert(texture < GL_TEXTURE0 + MAX_TEXTURE_UNITS);
  ctx->active_texture_unit =
      clamp(int(texture - GL_TEXTURE0), 0, int(MAX_TEXTURE_UNITS - 1));
}

void GenQueries(GLsizei n, GLuint* result) {
  for (int i = 0; i < n; i++) {
    Query q;
    result[i] = ctx->queries.insert(q);
  }
}

void DeleteQuery(GLuint n) {
  if (n && ctx->queries.erase(n)) {
    unlink(ctx->time_elapsed_query, n);
    unlink(ctx->samples_passed_query, n);
  }
}

void GenBuffers(int n, GLuint* result) {
  for (int i = 0; i < n; i++) {
    Buffer b;
    result[i] = ctx->buffers.insert(b);
  }
}

void DeleteBuffer(GLuint n) {
  if (n && ctx->buffers.erase(n)) {
    unlink(ctx->pixel_pack_buffer_binding, n);
    unlink(ctx->pixel_unpack_buffer_binding, n);
    unlink(ctx->array_buffer_binding, n);
    unlink(ctx->element_array_buffer_binding, n);
  }
}

void GenVertexArrays(int n, GLuint* result) {
  for (int i = 0; i < n; i++) {
    VertexArray v;
    result[i] = ctx->vertex_arrays.insert(v);
  }
}

void DeleteVertexArray(GLuint n) {
  if (n && ctx->vertex_arrays.erase(n)) {
    unlink(ctx->current_vertex_array, n);
  }
}

GLuint CreateShader(GLenum type) {
  Shader s;
  s.type = type;
  return ctx->shaders.insert(s);
}

void ShaderSourceByName(GLuint shader, char* name) {
  Shader& s = ctx->shaders[shader];
  s.loader = load_shader(name);
  if (!s.loader) {
    debugf("unknown shader %s\n", name);
  }
}

void AttachShader(GLuint program, GLuint shader) {
  Program& p = ctx->programs[program];
  Shader& s = ctx->shaders[shader];
  if (s.type == GL_VERTEX_SHADER) {
    if (!p.impl && s.loader) p.impl = s.loader();
  } else if (s.type == GL_FRAGMENT_SHADER) {
    if (!p.impl && s.loader) p.impl = s.loader();
  } else {
    assert(0);
  }
}

void DeleteShader(GLuint n) {
  if (n) ctx->shaders.erase(n);
}

GLuint CreateProgram() {
  Program p;
  return ctx->programs.insert(p);
}

void DeleteProgram(GLuint n) {
  if (!n) return;
  if (ctx->current_program == n) {
    if (auto* p = ctx->programs.find(n)) {
      p->deleted = true;
    }
  } else {
    ctx->programs.erase(n);
  }
}

void LinkProgram(GLuint program) {
  Program& p = ctx->programs[program];
  assert(p.impl);
  if (!p.vert_impl) p.vert_impl = p.impl->get_vertex_shader();
  if (!p.frag_impl) p.frag_impl = p.impl->get_fragment_shader();
}

void BindAttribLocation(GLuint program, GLuint index, char* name) {
  Program& p = ctx->programs[program];
  assert(p.impl);
  p.impl->bind_attrib(name, index);
}

GLint GetAttribLocation(GLuint program, char* name) {
  Program& p = ctx->programs[program];
  assert(p.impl);
  return p.impl->get_attrib(name);
}

GLint GetUniformLocation(GLuint program, char* name) {
  Program& p = ctx->programs[program];
  assert(p.impl);
  GLint loc = p.impl->get_uniform(name);
  // debugf("location: %d\n", loc);
  return loc;
}

static uint64_t get_time_value() {
#ifdef __MACH__
  return mach_absolute_time();
#elif defined(_WIN32)
  return uint64_t(clock()) * (1000000000ULL / CLOCKS_PER_SEC);
#else
  return ({
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    tp.tv_sec * 1000000000ULL + tp.tv_nsec;
  });
#endif
}

void BeginQuery(GLenum target, GLuint id) {
  ctx->get_binding(target) = id;
  Query& q = ctx->queries[id];
  switch (target) {
    case GL_SAMPLES_PASSED:
      q.value = 0;
      break;
    case GL_TIME_ELAPSED:
      q.value = get_time_value();
      break;
    default:
      debugf("unknown query target %x for query %d\n", target, id);
      assert(false);
  }
}

void EndQuery(GLenum target) {
  Query& q = ctx->queries[ctx->get_binding(target)];
  switch (target) {
    case GL_SAMPLES_PASSED:
      break;
    case GL_TIME_ELAPSED:
      q.value = get_time_value() - q.value;
      break;
    default:
      debugf("unknown query target %x\n", target);
      assert(false);
  }
  ctx->get_binding(target) = 0;
}

void GetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params) {
  Query& q = ctx->queries[id];
  switch (pname) {
    case GL_QUERY_RESULT:
      assert(params);
      params[0] = q.value;
      break;
    default:
      assert(false);
  }
}

void BindVertexArray(GLuint vertex_array) {
  if (vertex_array != ctx->current_vertex_array) {
    ctx->validate_vertex_array = true;
  }
  ctx->current_vertex_array = vertex_array;
}

void BindTexture(GLenum target, GLuint texture) {
  ctx->get_binding(target) = texture;
}

void BindBuffer(GLenum target, GLuint buffer) {
  ctx->get_binding(target) = buffer;
}

void BindFramebuffer(GLenum target, GLuint fb) {
  if (target == GL_FRAMEBUFFER) {
    ctx->read_framebuffer_binding = fb;
    ctx->draw_framebuffer_binding = fb;
  } else {
    assert(target == GL_READ_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER);
    ctx->get_binding(target) = fb;
  }
}

void BindRenderbuffer(GLenum target, GLuint rb) {
  ctx->get_binding(target) = rb;
}

void PixelStorei(GLenum name, GLint param) {
  if (name == GL_UNPACK_ALIGNMENT) {
    assert(param == 1);
  } else if (name == GL_UNPACK_ROW_LENGTH) {
    ctx->unpack_row_length = param;
  }
}

static GLenum remap_internal_format(GLenum format) {
  switch (format) {
    case GL_DEPTH_COMPONENT:
      return GL_DEPTH_COMPONENT16;
    case GL_RGBA:
      return GL_RGBA8;
    case GL_RED:
      return GL_R8;
    default:
      return format;
  }
}

void TexStorage3D(GLenum target, GLint levels, GLenum internal_format,
                  GLsizei width, GLsizei height, GLsizei depth) {
  Texture& t = ctx->textures[ctx->get_binding(target)];
  internal_format = remap_internal_format(internal_format);
  bool changed = false;
  if (t.width != width || t.height != height || t.depth != depth ||
      t.levels != levels || t.internal_format != internal_format) {
    changed = true;
    t.levels = levels;
    t.internal_format = internal_format;
    t.width = width;
    t.height = height;
    t.depth = depth;
  }
  t.disable_delayed_clear();
  t.allocate(changed);
}

static void set_tex_storage(Texture& t, GLint levels, GLenum internal_format,
                            GLsizei width, GLsizei height,
                            bool should_free = true, void* buf = nullptr,
                            GLsizei min_width = 0, GLsizei min_height = 0) {
  internal_format = remap_internal_format(internal_format);
  bool changed = false;
  if (t.width != width || t.height != height || t.levels != levels ||
      t.internal_format != internal_format) {
    changed = true;
    t.levels = levels;
    t.internal_format = internal_format;
    t.width = width;
    t.height = height;
  }
  if (t.should_free() != should_free || buf != nullptr) {
    if (t.should_free()) {
      t.cleanup();
    }
    t.set_should_free(should_free);
    t.buf = (char*)buf;
    t.buf_size = 0;
  }
  t.disable_delayed_clear();
  t.allocate(changed, min_width, min_height);
}

void TexStorage2D(GLenum target, GLint levels, GLenum internal_format,
                  GLsizei width, GLsizei height) {
  Texture& t = ctx->textures[ctx->get_binding(target)];
  set_tex_storage(t, levels, internal_format, width, height);
}

GLenum internal_format_for_data(GLenum format, GLenum ty) {
  if (format == GL_RED && ty == GL_UNSIGNED_BYTE) {
    return GL_R8;
  } else if ((format == GL_RGBA || format == GL_BGRA) &&
             ty == GL_UNSIGNED_BYTE) {
    return GL_RGBA8;
  } else if (format == GL_RGBA && ty == GL_FLOAT) {
    return GL_RGBA32F;
  } else if (format == GL_RGBA_INTEGER && ty == GL_INT) {
    return GL_RGBA32I;
  } else {
    debugf("unknown internal format for format %x, type %x\n", format, ty);
    assert(false);
    return 0;
  }
}

static inline void copy_bgra8_to_rgba8(uint32_t* dest, uint32_t* src,
                                       int width) {
  for (; width >= 4; width -= 4, dest += 4, src += 4) {
    U32 p = unaligned_load<U32>(src);
    U32 rb = p & 0x00FF00FF;
    unaligned_store(dest, (p & 0xFF00FF00) | (rb << 16) | (rb >> 16));
  }
  for (; width > 0; width--, dest++, src++) {
    uint32_t p = *src;
    uint32_t rb = p & 0x00FF00FF;
    *dest = (p & 0xFF00FF00) | (rb << 16) | (rb >> 16);
  }
}

static Buffer* get_pixel_pack_buffer() {
  return ctx->pixel_pack_buffer_binding
             ? &ctx->buffers[ctx->pixel_pack_buffer_binding]
             : nullptr;
}

static void* get_pixel_pack_buffer_data(void* data) {
  if (Buffer* b = get_pixel_pack_buffer()) {
    return b->buf ? b->buf + (size_t)data : nullptr;
  }
  return data;
}

static Buffer* get_pixel_unpack_buffer() {
  return ctx->pixel_unpack_buffer_binding
             ? &ctx->buffers[ctx->pixel_unpack_buffer_binding]
             : nullptr;
}

static void* get_pixel_unpack_buffer_data(void* data) {
  if (Buffer* b = get_pixel_unpack_buffer()) {
    return b->buf ? b->buf + (size_t)data : nullptr;
  }
  return data;
}

void TexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                   GLsizei width, GLsizei height, GLenum format, GLenum ty,
                   void* data) {
  data = get_pixel_unpack_buffer_data(data);
  if (!data) return;
  Texture& t = ctx->textures[ctx->get_binding(target)];
  IntRect skip = {xoffset, yoffset, width, height};
  prepare_texture(t, &skip);
  assert(xoffset + width <= t.width);
  assert(yoffset + height <= t.height);
  assert(ctx->unpack_row_length == 0 || ctx->unpack_row_length >= width);
  GLsizei row_length =
      ctx->unpack_row_length != 0 ? ctx->unpack_row_length : width;
  assert(t.internal_format == internal_format_for_data(format, ty));
  int bpp = t.bpp();
  if (!bpp || !t.buf) return;
  size_t dest_stride = t.stride(bpp);
  char* dest = t.buf + yoffset * dest_stride + xoffset * bpp;
  char* src = (char*)data;
  for (int y = 0; y < height; y++) {
    if (t.internal_format == GL_RGBA8 && format != GL_BGRA) {
      copy_bgra8_to_rgba8((uint32_t*)dest, (uint32_t*)src, width);
    } else {
      memcpy(dest, src, width * bpp);
    }
    dest += dest_stride;
    src += row_length * bpp;
  }
}

void TexImage2D(GLenum target, GLint level, GLint internal_format,
                GLsizei width, GLsizei height, GLint border, GLenum format,
                GLenum ty, void* data) {
  assert(level == 0);
  assert(border == 0);
  TexStorage2D(target, 1, internal_format, width, height);
  TexSubImage2D(target, 0, 0, 0, width, height, format, ty, data);
}

void TexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                   GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                   GLenum format, GLenum ty, void* data) {
  data = get_pixel_unpack_buffer_data(data);
  if (!data) return;
  Texture& t = ctx->textures[ctx->get_binding(target)];
  prepare_texture(t);
  assert(ctx->unpack_row_length == 0 || ctx->unpack_row_length >= width);
  GLsizei row_length =
      ctx->unpack_row_length != 0 ? ctx->unpack_row_length : width;
  if (format == GL_BGRA) {
    assert(ty == GL_UNSIGNED_BYTE);
    assert(t.internal_format == GL_RGBA8);
  } else {
    assert(t.internal_format == internal_format_for_data(format, ty));
  }
  int bpp = t.bpp();
  if (!bpp || !t.buf) return;
  char* src = (char*)data;
  assert(xoffset + width <= t.width);
  assert(yoffset + height <= t.height);
  assert(zoffset + depth <= t.depth);
  size_t dest_stride = t.stride(bpp);
  for (int z = 0; z < depth; z++) {
    char* dest = t.buf + ((zoffset + z) * t.height + yoffset) * dest_stride +
                 xoffset * bpp;
    for (int y = 0; y < height; y++) {
      if (t.internal_format == GL_RGBA8 && format != GL_BGRA) {
        copy_bgra8_to_rgba8((uint32_t*)dest, (uint32_t*)src, width);
      } else {
        memcpy(dest, src, width * bpp);
      }
      dest += dest_stride;
      src += row_length * bpp;
    }
  }
}

void TexImage3D(GLenum target, GLint level, GLint internal_format,
                GLsizei width, GLsizei height, GLsizei depth, GLint border,
                GLenum format, GLenum ty, void* data) {
  assert(level == 0);
  assert(border == 0);
  TexStorage3D(target, 1, internal_format, width, height, depth);
  TexSubImage3D(target, 0, 0, 0, 0, width, height, depth, format, ty, data);
}

void GenerateMipmap(GLenum target) {
  // TODO: support mipmaps
}

void TexParameteri(GLenum target, GLenum pname, GLint param) {
  Texture& t = ctx->textures[ctx->get_binding(target)];
  switch (pname) {
    case GL_TEXTURE_WRAP_S:
      assert(param == GL_CLAMP_TO_EDGE);
      break;
    case GL_TEXTURE_WRAP_T:
      assert(param == GL_CLAMP_TO_EDGE);
      break;
    case GL_TEXTURE_MIN_FILTER:
      t.min_filter = param;
      break;
    case GL_TEXTURE_MAG_FILTER:
      t.mag_filter = param;
      break;
    default:
      break;
  }
}

void GenTextures(int n, GLuint* result) {
  for (int i = 0; i < n; i++) {
    Texture t;
    result[i] = ctx->textures.insert(t);
  }
}

void DeleteTexture(GLuint n) {
  if (n && ctx->textures.erase(n)) {
    for (size_t i = 0; i < MAX_TEXTURE_UNITS; i++) {
      ctx->texture_units[i].unlink(n);
    }
  }
}

void GenRenderbuffers(int n, GLuint* result) {
  for (int i = 0; i < n; i++) {
    Renderbuffer r;
    result[i] = ctx->renderbuffers.insert(r);
  }
}

void Renderbuffer::on_erase() {
  for (auto* fb : ctx->framebuffers) {
    if (fb) {
      if (unlink(fb->color_attachment, texture)) {
        fb->layer = 0;
      }
      unlink(fb->depth_attachment, texture);
    }
  }
  DeleteTexture(texture);
}

void DeleteRenderbuffer(GLuint n) {
  if (n && ctx->renderbuffers.erase(n)) {
    unlink(ctx->renderbuffer_binding, n);
  }
}

void GenFramebuffers(int n, GLuint* result) {
  for (int i = 0; i < n; i++) {
    Framebuffer f;
    result[i] = ctx->framebuffers.insert(f);
  }
}

void DeleteFramebuffer(GLuint n) {
  if (n && ctx->framebuffers.erase(n)) {
    unlink(ctx->read_framebuffer_binding, n);
    unlink(ctx->draw_framebuffer_binding, n);
  }
}

void RenderbufferStorage(GLenum target, GLenum internal_format, GLsizei width,
                         GLsizei height) {
  // Just refer a renderbuffer to a texture to simplify things for now...
  Renderbuffer& r = ctx->renderbuffers[ctx->get_binding(target)];
  if (!r.texture) {
    GenTextures(1, &r.texture);
  }
  switch (internal_format) {
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32:
      // Force depth format to 16 bits...
      internal_format = GL_DEPTH_COMPONENT16;
      break;
  }
  set_tex_storage(ctx->textures[r.texture], 1, internal_format, width, height);
}

void VertexAttribPointer(GLuint index, GLint size, GLenum type, bool normalized,
                         GLsizei stride, GLuint offset) {
  // debugf("cva: %d\n", ctx->current_vertex_array);
  VertexArray& v = ctx->vertex_arrays[ctx->current_vertex_array];
  if (index >= NULL_ATTRIB) {
    assert(0);
    return;
  }
  VertexAttrib& va = v.attribs[index];
  va.size = size * bytes_per_type(type);
  va.type = type;
  va.normalized = normalized;
  va.stride = stride;
  va.offset = offset;
  // Buffer &vertex_buf = ctx->buffers[ctx->array_buffer_binding];
  va.vertex_buffer = ctx->array_buffer_binding;
  va.vertex_array = ctx->current_vertex_array;
  ctx->validate_vertex_array = true;
}

void VertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride,
                          GLuint offset) {
  // debugf("cva: %d\n", ctx->current_vertex_array);
  VertexArray& v = ctx->vertex_arrays[ctx->current_vertex_array];
  if (index >= NULL_ATTRIB) {
    assert(0);
    return;
  }
  VertexAttrib& va = v.attribs[index];
  va.size = size * bytes_per_type(type);
  va.type = type;
  va.normalized = false;
  va.stride = stride;
  va.offset = offset;
  // Buffer &vertex_buf = ctx->buffers[ctx->array_buffer_binding];
  va.vertex_buffer = ctx->array_buffer_binding;
  va.vertex_array = ctx->current_vertex_array;
  ctx->validate_vertex_array = true;
}

void EnableVertexAttribArray(GLuint index) {
  VertexArray& v = ctx->vertex_arrays[ctx->current_vertex_array];
  if (index >= NULL_ATTRIB) {
    assert(0);
    return;
  }
  VertexAttrib& va = v.attribs[index];
  if (!va.enabled) {
    ctx->validate_vertex_array = true;
  }
  va.enabled = true;
  v.max_attrib = max(v.max_attrib, (int)index);
}

void DisableVertexAttribArray(GLuint index) {
  VertexArray& v = ctx->vertex_arrays[ctx->current_vertex_array];
  if (index >= NULL_ATTRIB) {
    assert(0);
    return;
  }
  VertexAttrib& va = v.attribs[index];
  if (va.enabled) {
    ctx->validate_vertex_array = true;
  }
  va.enabled = false;
}

void VertexAttribDivisor(GLuint index, GLuint divisor) {
  VertexArray& v = ctx->vertex_arrays[ctx->current_vertex_array];
  if (index >= NULL_ATTRIB) {
    assert(0);
    return;
  }
  VertexAttrib& va = v.attribs[index];
  va.divisor = divisor;
}

void BufferData(GLenum target, GLsizeiptr size, void* data, GLenum usage) {
  Buffer& b = ctx->buffers[ctx->get_binding(target)];
  if (b.allocate(size)) {
    ctx->validate_vertex_array = true;
  }
  if (data && b.buf && size <= b.size) {
    memcpy(b.buf, data, size);
  }
}

void BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,
                   void* data) {
  Buffer& b = ctx->buffers[ctx->get_binding(target)];
  assert(offset + size <= b.size);
  if (data && b.buf && offset + size <= b.size) {
    memcpy(&b.buf[offset], data, size);
  }
}

void* MapBuffer(GLenum target, GLbitfield access) {
  Buffer& b = ctx->buffers[ctx->get_binding(target)];
  return b.buf;
}

void* MapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                     GLbitfield access) {
  Buffer& b = ctx->buffers[ctx->get_binding(target)];
  if (b.buf && offset >= 0 && length > 0 && offset + length <= b.size) {
    return b.buf + offset;
  }
  return nullptr;
}

GLboolean UnmapBuffer(GLenum target) {
  Buffer& b = ctx->buffers[ctx->get_binding(target)];
  return b.buf != nullptr;
}

void Uniform1i(GLint location, GLint V0) {
  // debugf("tex: %d\n", (int)ctx->textures.size);
  if (!program_impl->set_sampler(location, V0)) {
    vertex_shader->set_uniform_1i(location, V0);
    fragment_shader->set_uniform_1i(location, V0);
  }
}
void Uniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  vertex_shader->set_uniform_4fv(location, v);
  fragment_shader->set_uniform_4fv(location, v);
}
void UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                      const GLfloat* value) {
  vertex_shader->set_uniform_matrix4fv(location, value);
  fragment_shader->set_uniform_matrix4fv(location, value);
}

void FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
                          GLuint texture, GLint level) {
  assert(target == GL_READ_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER);
  Framebuffer& fb = ctx->framebuffers[ctx->get_binding(target)];
  if (attachment == GL_COLOR_ATTACHMENT0) {
    fb.color_attachment = texture;
    fb.layer = 0;
  } else if (attachment == GL_DEPTH_ATTACHMENT) {
    fb.depth_attachment = texture;
  } else {
    assert(0);
  }
}

void FramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture,
                             GLint level, GLint layer) {
  assert(level == 0);
  assert(target == GL_READ_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER);
  Framebuffer& fb = ctx->framebuffers[ctx->get_binding(target)];
  if (attachment == GL_COLOR_ATTACHMENT0) {
    fb.color_attachment = texture;
    fb.layer = layer;
  } else if (attachment == GL_DEPTH_ATTACHMENT) {
    assert(layer == 0);
    fb.depth_attachment = texture;
  } else {
    assert(0);
  }
}

void FramebufferRenderbuffer(GLenum target, GLenum attachment,
                             GLenum renderbuffertarget, GLuint renderbuffer) {
  assert(target == GL_READ_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER);
  assert(renderbuffertarget == GL_RENDERBUFFER);
  Framebuffer& fb = ctx->framebuffers[ctx->get_binding(target)];
  Renderbuffer& rb = ctx->renderbuffers[renderbuffer];
  if (attachment == GL_COLOR_ATTACHMENT0) {
    fb.color_attachment = rb.texture;
    fb.layer = 0;
  } else if (attachment == GL_DEPTH_ATTACHMENT) {
    fb.depth_attachment = rb.texture;
  } else {
    assert(0);
  }
}

}  // extern "C"

static inline Framebuffer* get_framebuffer(GLenum target) {
  if (target == GL_FRAMEBUFFER) {
    target = GL_DRAW_FRAMEBUFFER;
  }
  return ctx->framebuffers.find(ctx->get_binding(target));
}

template <typename T>
static inline void fill_n(T* dst, size_t n, T val) {
  for (T* end = &dst[n]; dst < end; dst++) *dst = val;
}

#if USE_SSE2
template <>
inline void fill_n<uint32_t>(uint32_t* dst, size_t n, uint32_t val) {
  __asm__ __volatile__("rep stosl\n"
                       : "+D"(dst), "+c"(n)
                       : "a"(val)
                       : "memory", "cc");
}
#endif

template <typename T>
static void clear_buffer(Texture& t, T value, int x0, int x1, int y0, int y1,
                         int layer = 0, int skip_start = 0, int skip_end = 0) {
  if (!t.buf) return;
  skip_start = max(skip_start, x0);
  skip_end = max(skip_end, skip_start);
  size_t stride = t.stride(sizeof(T));
  if (x1 - x0 == t.width && y1 - y0 > 1 && skip_start >= skip_end) {
    x1 += (stride / sizeof(T)) * (y1 - y0 - 1);
    y1 = y0 + 1;
  }
  char* buf = t.buf + stride * (t.height * layer + y0) + x0 * sizeof(T);
  uint32_t chunk =
      sizeof(T) == 1
          ? uint32_t(value) * 0x01010101U
          : (sizeof(T) == 2 ? uint32_t(value) | (uint32_t(value) << 16)
                            : value);
  for (int y = y0; y < y1; y++) {
    if (x0 < skip_start) {
      fill_n((uint32_t*)buf, (skip_start - x0) / (4 / sizeof(T)), chunk);
      if (sizeof(T) < 4) {
        fill_n((T*)buf + ((skip_start - x0) & ~(4 / sizeof(T) - 1)),
               (skip_start - x0) & (4 / sizeof(T) - 1), value);
      }
    }
    if (skip_end < x1) {
      T* skip_buf = (T*)buf + (skip_end - x0);
      fill_n((uint32_t*)skip_buf, (x1 - skip_end) / (4 / sizeof(T)), chunk);
      if (sizeof(T) < 4) {
        fill_n(skip_buf + ((x1 - skip_end) & ~(4 / sizeof(T) - 1)),
               (x1 - skip_end) & (4 / sizeof(T) - 1), value);
      }
    }
    buf += stride;
  }
}

template <typename T>
static inline void clear_buffer(Texture& t, T value, int layer = 0) {
  int x0 = 0, y0 = 0, x1 = t.width, y1 = t.height;
  if (ctx->scissortest) {
    x0 = max(x0, ctx->scissor.x);
    y0 = max(y0, ctx->scissor.y);
    x1 = min(x1, ctx->scissor.x + ctx->scissor.width);
    y1 = min(y1, ctx->scissor.y + ctx->scissor.height);
  }
  if (x1 - x0 > 0) {
    clear_buffer<T>(t, value, x0, x1, y0, y1, layer);
  }
}

template <typename T>
static void force_clear(Texture& t, const IntRect* skip = nullptr) {
  if (!t.delay_clear || !t.cleared_rows) {
    return;
  }
  int y0 = 0;
  int y1 = t.height;
  int skip_start = 0;
  int skip_end = 0;
  if (skip) {
    y0 = min(max(skip->y, 0), t.height);
    y1 = min(max(skip->y + skip->height, y0), t.height);
    skip_start = min(max(skip->x, 0), t.width);
    skip_end = min(max(skip->x + skip->width, skip_start), t.width);
    if (skip_start <= 0 && skip_end >= t.width && y0 <= 0 && y1 >= t.height) {
      t.disable_delayed_clear();
      return;
    }
  }
  int num_masks = (y1 + 31) / 32;
  uint32_t* rows = t.cleared_rows;
  for (int i = y0 / 32; i < num_masks; i++) {
    uint32_t mask = rows[i];
    if (mask != ~0U) {
      rows[i] = ~0U;
      int start = i * 32;
      while (mask) {
        int count = __builtin_ctz(mask);
        if (count > 0) {
          clear_buffer<T>(t, t.clear_val, 0, t.width, start, start + count, 0,
                          skip_start, skip_end);
          t.delay_clear -= count;
          start += count;
          mask >>= count;
        }
        count = __builtin_ctz(mask + 1);
        start += count;
        mask >>= count;
      }
      int count = (i + 1) * 32 - start;
      if (count > 0) {
        clear_buffer<T>(t, t.clear_val, 0, t.width, start, start + count, 0,
                        skip_start, skip_end);
        t.delay_clear -= count;
      }
    }
  }
  if (t.delay_clear <= 0) t.disable_delayed_clear();
}

static void prepare_texture(Texture& t, const IntRect* skip) {
  if (t.delay_clear) {
    switch (t.internal_format) {
      case GL_RGBA8:
        force_clear<uint32_t>(t, skip);
        break;
      case GL_R8:
        force_clear<uint8_t>(t, skip);
        break;
      case GL_DEPTH_COMPONENT16:
        force_clear<uint16_t>(t, skip);
        break;
      default:
        assert(false);
        break;
    }
  }
}

extern "C" {

void InitDefaultFramebuffer(int width, int height) {
  Framebuffer& fb = ctx->framebuffers[0];
  if (!fb.color_attachment) {
    GenTextures(1, &fb.color_attachment);
    fb.layer = 0;
  }
  Texture& colortex = ctx->textures[fb.color_attachment];
  if (colortex.width != width || colortex.height != height) {
    colortex.cleanup();
    set_tex_storage(colortex, 1, GL_RGBA8, width, height);
  }
  if (!fb.depth_attachment) {
    GenTextures(1, &fb.depth_attachment);
  }
  Texture& depthtex = ctx->textures[fb.depth_attachment];
  if (depthtex.width != width || depthtex.height != height) {
    depthtex.cleanup();
    set_tex_storage(depthtex, 1, GL_DEPTH_COMPONENT16, width, height);
  }
}

void* GetColorBuffer(GLuint fbo, GLboolean flush, int32_t* width,
                     int32_t* height) {
  Framebuffer* fb = ctx->framebuffers.find(fbo);
  if (!fb || !fb->color_attachment) {
    return nullptr;
  }
  Texture& colortex = ctx->textures[fb->color_attachment];
  if (flush) {
    prepare_texture(colortex);
  }
  *width = colortex.width;
  *height = colortex.height;
  return colortex.buf
             ? colortex.buf +
                   (fb->layer ? fb->layer * colortex.layer_stride() : 0)
             : nullptr;
}

void SetTextureBuffer(GLuint texid, GLenum internal_format, GLsizei width,
                      GLsizei height, void* buf, GLsizei min_width,
                      GLsizei min_height) {
  Texture& t = ctx->textures[texid];
  set_tex_storage(t, 1, internal_format, width, height, !buf, buf, min_width,
                  min_height);
}

GLenum CheckFramebufferStatus(GLenum target) {
  Framebuffer* fb = get_framebuffer(target);
  if (!fb || !fb->color_attachment) {
    return GL_FRAMEBUFFER_UNSUPPORTED;
  }
  return GL_FRAMEBUFFER_COMPLETE;
}

static inline bool clear_requires_scissor(Texture& t) {
  return ctx->scissortest &&
         (ctx->scissor.x > 0 || ctx->scissor.y > 0 ||
          ctx->scissor.width < t.width || ctx->scissor.height < t.height);
}

void Clear(GLbitfield mask) {
  Framebuffer& fb = *get_framebuffer(GL_DRAW_FRAMEBUFFER);
  if ((mask & GL_COLOR_BUFFER_BIT) && fb.color_attachment) {
    Texture& t = ctx->textures[fb.color_attachment];
    if (t.internal_format == GL_RGBA8) {
      uint32_t color = ctx->clearcolor;
      if (clear_requires_scissor(t)) {
        force_clear<uint32_t>(t, &ctx->scissor);
        clear_buffer<uint32_t>(t, color, fb.layer);
      } else if (t.depth > 1) {
        t.disable_delayed_clear();
        clear_buffer<uint32_t>(t, color, fb.layer);
      } else {
        t.enable_delayed_clear(color);
      }
    } else if (t.internal_format == GL_R8) {
      uint8_t color = uint8_t((ctx->clearcolor >> 16) & 0xFF);
      if (clear_requires_scissor(t)) {
        force_clear<uint8_t>(t, &ctx->scissor);
        clear_buffer<uint8_t>(t, color, fb.layer);
      } else if (t.depth > 1) {
        t.disable_delayed_clear();
        clear_buffer<uint8_t>(t, color, fb.layer);
      } else {
        t.enable_delayed_clear(color);
      }
    } else {
      assert(false);
    }
  }
  if ((mask & GL_DEPTH_BUFFER_BIT) && fb.depth_attachment) {
    Texture& t = ctx->textures[fb.depth_attachment];
    assert(t.internal_format == GL_DEPTH_COMPONENT16);
    uint16_t depth = uint16_t(0xFFFF * ctx->cleardepth) - 0x8000;
    if (clear_requires_scissor(t)) {
      force_clear<uint16_t>(t, &ctx->scissor);
      clear_buffer<uint16_t>(t, depth);
    } else {
      t.enable_delayed_clear(depth);
    }
  }
}

void InvalidateFramebuffer(GLenum target, GLsizei num_attachments,
                           const GLenum* attachments) {
  Framebuffer* fb = get_framebuffer(target);
  if (!fb || num_attachments <= 0 || !attachments) {
    return;
  }
  for (GLsizei i = 0; i < num_attachments; i++) {
    switch (attachments[i]) {
      case GL_DEPTH_ATTACHMENT: {
        Texture& t = ctx->textures[fb->depth_attachment];
        t.disable_delayed_clear();
        break;
      }
      case GL_COLOR_ATTACHMENT0: {
        Texture& t = ctx->textures[fb->color_attachment];
        t.disable_delayed_clear();
        break;
      }
    }
  }
}

void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                GLenum type, void* data) {
  data = get_pixel_pack_buffer_data(data);
  if (!data) return;
  Framebuffer* fb = get_framebuffer(GL_READ_FRAMEBUFFER);
  if (!fb) return;
  assert(format == GL_RED || format == GL_RGBA || format == GL_RGBA_INTEGER ||
         format == GL_BGRA);
  Texture& t = ctx->textures[fb->color_attachment];
  if (!t.buf) return;
  prepare_texture(t);
  // debugf("read pixels %d, %d, %d, %d from fb %d with format %x\n", x, y,
  // width, height, ctx->read_framebuffer_binding, t.internal_format);
  assert(x + width <= t.width);
  assert(y + height <= t.height);
  if (internal_format_for_data(format, type) != t.internal_format) {
    debugf("mismatched format for read pixels: %x vs %x\n", t.internal_format,
           internal_format_for_data(format, type));
    assert(false);
  }
  int bpp = t.bpp();
  char* dest = (char*)data;
  size_t src_stride = t.stride(bpp);
  char* src = t.buf + (t.height * fb->layer + y) * src_stride + x * bpp;
  for (; height > 0; height--) {
    if (t.internal_format == GL_RGBA8 && format != GL_BGRA) {
      copy_bgra8_to_rgba8((uint32_t*)dest, (uint32_t*)src, width);
    } else {
      memcpy(dest, src, width * bpp);
    }
    dest += width * bpp;
    src += src_stride;
  }
}

void CopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel,
                      GLint srcX, GLint srcY, GLint srcZ, GLuint dstName,
                      GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY,
                      GLint dstZ, GLsizei srcWidth, GLsizei srcHeight,
                      GLsizei srcDepth) {
  if (srcTarget == GL_RENDERBUFFER) {
    Renderbuffer& rb = ctx->renderbuffers[srcName];
    srcName = rb.texture;
  }
  if (dstTarget == GL_RENDERBUFFER) {
    Renderbuffer& rb = ctx->renderbuffers[dstName];
    dstName = rb.texture;
  }
  Texture& srctex = ctx->textures[srcName];
  if (!srctex.buf) return;
  prepare_texture(srctex);
  Texture& dsttex = ctx->textures[dstName];
  if (!dsttex.buf) return;
  IntRect skip = {dstX, dstY, srcWidth, abs(srcHeight)};
  prepare_texture(dsttex, &skip);
  assert(srctex.internal_format == dsttex.internal_format);
  assert(srcX + srcWidth <= srctex.width);
  assert(srcY + srcHeight <= srctex.height);
  assert(srcZ + srcDepth <= max(srctex.depth, 1));
  assert(dstX + srcWidth <= dsttex.width);
  assert(max(dstY, dstY + srcHeight) <= dsttex.height);
  assert(dstZ + srcDepth <= max(dsttex.depth, 1));
  int bpp = srctex.bpp();
  int src_stride = srctex.stride(bpp);
  int dest_stride = dsttex.stride(bpp);
  for (int z = 0; z < srcDepth; z++) {
    char* dest = dsttex.buf +
                 (dsttex.height * (dstZ + z) + dstY) * dest_stride + dstX * bpp;
    char* src = srctex.buf + (srctex.height * (srcZ + z) + srcY) * src_stride +
                srcX * bpp;
    if (srcHeight < 0) {
      for (int y = srcHeight; y < 0; y++) {
        dest -= dest_stride;
        memcpy(dest, src, srcWidth * bpp);
        src += src_stride;
      }
    } else {
      for (int y = 0; y < srcHeight; y++) {
        memcpy(dest, src, srcWidth * bpp);
        dest += dest_stride;
        src += src_stride;
      }
    }
  }
}

void CopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                       GLint zoffset, GLint x, GLint y, GLsizei width,
                       GLsizei height) {
  Framebuffer* fb = get_framebuffer(GL_READ_FRAMEBUFFER);
  if (!fb) return;
  CopyImageSubData(fb->color_attachment, GL_TEXTURE_3D, 0, x, y, fb->layer,
                   ctx->get_binding(target), GL_TEXTURE_3D, 0, xoffset, yoffset,
                   zoffset, width, height, 1);
}

void CopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                       GLint x, GLint y, GLsizei width, GLsizei height) {
  Framebuffer* fb = get_framebuffer(GL_READ_FRAMEBUFFER);
  if (!fb) return;
  CopyImageSubData(fb->color_attachment, GL_TEXTURE_2D_ARRAY, 0, x, y,
                   fb->layer, ctx->get_binding(target), GL_TEXTURE_2D_ARRAY, 0,
                   xoffset, yoffset, 0, width, height, 1);
}

} // extern "C"

template <typename P>
static void scale_row(Texture& dsttex, P* dst, int dstX, int dstWidth,
                      Texture& srctex, const P* src, int srcX, int srcWidth) {
  int frac = 0;
  // limit to valid dest region
  int x0 = max(-dstX, 0);
  int x1 = min(dsttex.width - dstX, dstWidth);
  // limit to valid source region
  if (srcX < 0) {
    x0 = max(x0, (-srcX * dstWidth + srcWidth - 1) / srcWidth);
  }
  if (srcX + srcWidth > srctex.width) {
    x1 = min(x1, ((srctex.width - srcX) * dstWidth) / srcWidth);
  }
  // skip to clamped start
  if (x0 > 0) {
    dst += x0;
    src += (x0 * srcWidth) / dstWidth;
  }
  for (P* end = dst + (x1 - x0); dst < end; dst++) {
    *dst = *src;
    // Step source according to width ratio.
    for (frac += srcWidth; frac >= dstWidth; frac -= dstWidth) {
      src++;
    }
  }
}

static void scale_blit(GLuint srcName, GLint srcX, GLint srcY, GLint srcZ,
                       GLsizei srcWidth, GLsizei srcHeight,
                       GLuint dstName, GLint dstX, GLint dstY, GLint dstZ,
                       GLsizei dstWidth, GLsizei dstHeight) {
  Texture& srctex = ctx->textures[srcName];
  if (!srctex.buf) return;
  prepare_texture(srctex);
  Texture& dsttex = ctx->textures[dstName];
  if (!dsttex.buf) return;
  IntRect skip = {dstX, dstY, dstWidth, abs(dstHeight)};
  prepare_texture(dsttex, &skip);
  assert(srctex.internal_format == dsttex.internal_format);
  assert(srcHeight >= 0);
  assert(srcZ < max(srctex.depth, 1));
  assert(dstZ < max(dsttex.depth, 1));
  int bpp = srctex.bpp();
  int srcStride = srctex.stride(bpp);
  int destStride = dsttex.stride(bpp);
  char* dest = dsttex.buf + (dsttex.height * dstZ + dstY) * destStride +
               dstX * bpp;
  char* src = srctex.buf + (srctex.height * srcZ + srcY) * srcStride +
              srcX * bpp;
  if (dstHeight < 0) {
    dest -= destStride;
    destStride = -destStride;
    dstHeight = -dstHeight;
  }
  int frac = 0;
  int y0 = 0;
  int y1 = dstHeight;
  // limit rows to within valid dest region
  if (destStride < 0) {
    y0 = max(y0, dstY - dsttex.height);
    y1 = min(y1, dstY);
  } else {
    y0 = max(y0, -dstY);
    y1 = min(y1, dsttex.height - dstY);
  }
  // limit rows to valid source region
  if (srcY < 0) {
    y0 = max(y0, (-srcY * dstHeight + srcHeight - 1) / srcHeight);
  }
  if (srcY + srcHeight > srctex.height) {
    y1 = min(y1, ((srctex.height - srcY) * dstHeight) / srcHeight);
  }
  // skip to clamped start
  if (y0 > 0) {
    dest += y0 * destStride;
    src += ((y0 * srcHeight) / dstHeight) * srcStride;
  }
  for (int y = y0; y < y1; y++) {
    if (srcWidth == dstWidth) {
      // No scaling, so just do a fast copy.
      // clamp to valid dest and source regions
      int x0 = max(max(-srcX, -dstX), 0);
      int x1 = min(min(srctex.width - srcX, dsttex.width - dstX), srcWidth);
      if (x0 < x1) {
        memcpy(dest + x0 * bpp, src + x0 * bpp, (x1 - x0) * bpp);
      }
    } else {
      // Do scaling with different source and dest widths.
      switch (bpp) {
        case 1:
          scale_row(dsttex, (uint8_t*)dest, dstX, dstWidth,
                    srctex, (uint8_t*)src, srcX, srcWidth);
          break;
        case 2:
          scale_row(dsttex, (uint16_t*)dest, dstX, dstWidth,
                    srctex, (uint16_t*)src, srcX, srcWidth);
          break;
        case 4:
          scale_row(dsttex, (uint32_t*)dest, dstX, dstWidth,
                    srctex, (uint32_t*)src, srcX, srcWidth);
          break;
      }
    }
    dest += destStride;
    // Step source according to height ratio.
    for (frac += srcHeight; frac >= dstHeight; frac -= dstHeight) {
      src += srcStride;
    }
  }
}

extern "C" {

void BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                     GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                     GLbitfield mask, GLenum filter) {
  assert(mask == GL_COLOR_BUFFER_BIT);
  Framebuffer* srcfb = get_framebuffer(GL_READ_FRAMEBUFFER);
  if (!srcfb) return;
  Framebuffer* dstfb = get_framebuffer(GL_DRAW_FRAMEBUFFER);
  if (!dstfb) return;
  // TODO: support linear filtering
  scale_blit(srcfb->color_attachment, srcX0, srcY0, srcfb->layer,
             srcX1 - srcX0, srcY1 - srcY0,
             dstfb->color_attachment, dstX0, dstY0, dstfb->layer,
             dstX1 - dstX0, dstY1 - dstY0);
}

}  // extern "C"

using PackedRGBA8 = V16<uint8_t>;
using WideRGBA8 = V16<uint16_t>;
using HalfRGBA8 = V8<uint16_t>;

static inline WideRGBA8 unpack(PackedRGBA8 p) { return CONVERT(p, WideRGBA8); }

static inline PackedRGBA8 pack(WideRGBA8 p) {
#if USE_SSE2
  return _mm_packus_epi16(lowHalf(p), highHalf(p));
#elif USE_NEON
  return vcombine_u8(vqmovn_u16(lowHalf(p)), vqmovn_u16(highHalf(p)));
#else
  return CONVERT(p, PackedRGBA8);
#endif
}

static inline HalfRGBA8 packRGBA8(I32 a, I32 b) {
#if USE_SSE2
  return _mm_packs_epi32(a, b);
#elif USE_NEON
  return vcombine_u16(vqmovun_s32(a), vqmovun_s32(b));
#else
  return CONVERT(combine(a, b), HalfRGBA8);
#endif
}

using PackedR8 = V4<uint8_t>;
using WideR8 = V4<uint16_t>;

static inline WideR8 unpack(PackedR8 p) { return CONVERT(p, WideR8); }

static inline WideR8 packR8(I32 a) {
#if USE_SSE2
  return lowHalf(bit_cast<V8<uint16_t>>(_mm_packs_epi32(a, a)));
#elif USE_NEON
  return vqmovun_s32(a);
#else
  return CONVERT(a, WideR8);
#endif
}

static inline PackedR8 pack(WideR8 p) {
#if USE_SSE2
  auto m = expand(p);
  auto r = bit_cast<V16<uint8_t>>(_mm_packus_epi16(m, m));
  return SHUFFLE(r, r, 0, 1, 2, 3);
#elif USE_NEON
  return lowHalf(bit_cast<V8<uint8_t>>(vqmovn_u16(expand(p))));
#else
  return CONVERT(p, PackedR8);
#endif
}

using ZMask4 = V4<int16_t>;
using ZMask8 = V8<int16_t>;

static inline PackedRGBA8 unpack(ZMask4 mask, uint32_t*) {
  return bit_cast<PackedRGBA8>(mask.xxyyzzww);
}

static inline WideR8 unpack(ZMask4 mask, uint8_t*) {
  return bit_cast<WideR8>(mask);
}

#if USE_SSE2
#  define ZMASK_NONE_PASSED 0xFFFF
#  define ZMASK_ALL_PASSED 0
static inline uint32_t zmask_code(ZMask8 mask) {
  return _mm_movemask_epi8(mask);
}
static inline uint32_t zmask_code(ZMask4 mask) {
  return zmask_code(mask.xyzwxyzw);
}
#else
using ZMask4Code = V4<uint8_t>;
using ZMask8Code = V8<uint8_t>;
#  define ZMASK_NONE_PASSED 0xFFFFFFFFU
#  define ZMASK_ALL_PASSED 0
static inline uint32_t zmask_code(ZMask4 mask) {
  return bit_cast<uint32_t>(CONVERT(mask, ZMask4Code));
}
static inline uint32_t zmask_code(ZMask8 mask) {
  return zmask_code(
      ZMask4((U16(lowHalf(mask)) >> 12) | (U16(highHalf(mask)) << 4)));
}
#endif

template <int FUNC, bool MASK>
static ALWAYS_INLINE int check_depth8(uint16_t z, uint16_t* zbuf,
                                      ZMask8& outmask) {
  ZMask8 dest = unaligned_load<ZMask8>(zbuf);
  ZMask8 src = int16_t(z);
  // Invert the depth test to check which pixels failed and should be discarded.
  ZMask8 mask = FUNC == GL_LEQUAL ?
                                  // GL_LEQUAL: Not(LessEqual) = Greater
                    ZMask8(src > dest)
                                  :
                                  // GL_LESS: Not(Less) = GreaterEqual
                    ZMask8(src >= dest);
  switch (zmask_code(mask)) {
    case ZMASK_NONE_PASSED:
      return 0;
    case ZMASK_ALL_PASSED:
      if (MASK) {
        unaligned_store(zbuf, src);
      }
      return -1;
    default:
      if (MASK) {
        unaligned_store(zbuf, (mask & dest) | (~mask & src));
      }
      outmask = mask;
      return 1;
  }
}

template <bool FULL_SPANS, bool DISCARD>
static ALWAYS_INLINE bool check_depth4(ZMask4 src, uint16_t* zbuf,
                                       ZMask4& outmask, int span = 0) {
  ZMask4 dest = unaligned_load<ZMask4>(zbuf);
  // Invert the depth test to check which pixels failed and should be discarded.
  ZMask4 mask = ctx->depthfunc == GL_LEQUAL
                    ?
                    // GL_LEQUAL: Not(LessEqual) = Greater
                    ZMask4(src > dest)
                    :
                    // GL_LESS: Not(Less) = GreaterEqual
                    ZMask4(src >= dest);
  if (!FULL_SPANS) {
    mask |= ZMask4(span) < ZMask4{1, 2, 3, 4};
  }
  if (zmask_code(mask) == ZMASK_NONE_PASSED) {
    return false;
  }
  if (!DISCARD && ctx->depthmask) {
    unaligned_store(zbuf, (mask & dest) | (~mask & src));
  }
  outmask = mask;
  return true;
}

template <bool FULL_SPANS, bool DISCARD>
static ALWAYS_INLINE bool check_depth4(uint16_t z, uint16_t* zbuf,
                                       ZMask4& outmask, int span = 0) {
  return check_depth4<FULL_SPANS, DISCARD>(ZMask4(int16_t(z)), zbuf, outmask,
                                           span);
}

template <typename T>
static inline ZMask4 packZMask4(T a) {
#if USE_SSE2
  return lowHalf(bit_cast<ZMask8>(_mm_packs_epi32(a, a)));
#elif USE_NEON
  return vqmovn_s32(a);
#else
  return CONVERT(a, ZMask4);
#endif
}

static ALWAYS_INLINE ZMask4 packDepth() {
  return packZMask4(cast(fragment_shader->gl_FragCoord.z * 0xFFFF) - 0x8000);
}

static ALWAYS_INLINE void discard_depth(ZMask4 src, uint16_t* zbuf,
                                        ZMask4 mask) {
  if (ctx->depthmask) {
    ZMask4 dest = unaligned_load<ZMask4>(zbuf);
    mask |= packZMask4(fragment_shader->isPixelDiscarded);
    unaligned_store(zbuf, (mask & dest) | (~mask & src));
  }
}

static ALWAYS_INLINE void discard_depth(uint16_t z, uint16_t* zbuf,
                                        ZMask4 mask) {
  discard_depth(ZMask4(int16_t(z)), zbuf, mask);
}

static inline WideRGBA8 pack_pixels_RGBA8(const vec4& v) {
  ivec4 i = roundfast(v, 255.49f);
  HalfRGBA8 xz = packRGBA8(i.z, i.x);
  HalfRGBA8 yw = packRGBA8(i.y, i.w);
  HalfRGBA8 xy = zipLow(xz, yw);
  HalfRGBA8 zw = zipHigh(xz, yw);
  HalfRGBA8 lo = zip2Low(xy, zw);
  HalfRGBA8 hi = zip2High(xy, zw);
  return combine(lo, hi);
}

static inline WideRGBA8 pack_pixels_RGBA8(const vec4_scalar& v) {
  I32 i = roundfast((Float){v.z, v.y, v.x, v.w}, 255.49f);
  HalfRGBA8 c = packRGBA8(i, i);
  return combine(c, c);
}

static inline WideRGBA8 pack_pixels_RGBA8() {
  return pack_pixels_RGBA8(fragment_shader->gl_FragColor);
}

template <typename V>
static inline PackedRGBA8 pack_span(uint32_t*, const V& v) {
  return pack(pack_pixels_RGBA8(v));
}

static inline PackedRGBA8 pack_span(uint32_t*) {
  return pack(pack_pixels_RGBA8());
}

// (x*y + x) >> 8, cheap approximation of (x*y) / 255
template <typename T>
static inline T muldiv255(T x, T y) {
  return (x * y + x) >> 8;
}
static inline WideRGBA8 alphas(WideRGBA8 c) {
  return SHUFFLE(c, c, 3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15);
}

static inline WideRGBA8 blend_pixels_RGBA8(PackedRGBA8 pdst, WideRGBA8 src) {
  WideRGBA8 dst = unpack(pdst);
  const WideRGBA8 RGB_MASK = {0xFFFF, 0xFFFF, 0xFFFF, 0,      0xFFFF, 0xFFFF,
                              0xFFFF, 0,      0xFFFF, 0xFFFF, 0xFFFF, 0,
                              0xFFFF, 0xFFFF, 0xFFFF, 0};
  const WideRGBA8 ALPHA_MASK = {0, 0, 0, 0xFFFF, 0, 0, 0, 0xFFFF,
                                0, 0, 0, 0xFFFF, 0, 0, 0, 0xFFFF};
  switch (blend_key) {
    case BLEND_KEY_NONE:
      return src;
    case BLEND_KEY(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE):
      return dst + muldiv255((src - dst) | ALPHA_MASK, alphas(src));
    case BLEND_KEY(GL_ONE, GL_ONE_MINUS_SRC_ALPHA):
      return src + dst - muldiv255(dst, alphas(src));
    case BLEND_KEY(GL_ZERO, GL_ONE_MINUS_SRC_COLOR):
      return dst - muldiv255(dst, src);
    case BLEND_KEY(GL_ZERO, GL_ONE_MINUS_SRC_COLOR, GL_ZERO, GL_ONE):
      return dst - (muldiv255(dst, src) & RGB_MASK);
    case BLEND_KEY(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA):
      return dst - muldiv255(dst, alphas(src));
    case BLEND_KEY(GL_ZERO, GL_SRC_COLOR):
      return muldiv255(src, dst);
    case BLEND_KEY(GL_ONE, GL_ONE):
      return src + dst;
    case BLEND_KEY(GL_ONE, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA):
      return src + dst - (muldiv255(dst, src) & ALPHA_MASK);
    case BLEND_KEY(GL_ONE, GL_ZERO):
      return src;
    case BLEND_KEY(GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ZERO, GL_ONE):
      return dst + ((src - muldiv255(src, alphas(src))) & RGB_MASK);
    case BLEND_KEY(GL_CONSTANT_COLOR, GL_ONE_MINUS_SRC_COLOR):
      return dst +
             muldiv255(combine(ctx->blendcolor, ctx->blendcolor) - dst, src);
    case BLEND_KEY(GL_ONE, GL_ONE_MINUS_SRC1_COLOR): {
      WideRGBA8 secondary =
          pack_pixels_RGBA8(fragment_shader->gl_SecondaryFragColor);
      return src + dst - muldiv255(dst, secondary);
    }
    default:
      UNREACHABLE;
      // return src;
  }
}

template <bool DISCARD>
static inline void commit_output(uint32_t* buf, PackedRGBA8 mask) {
  fragment_shader->run();
  PackedRGBA8 dst = unaligned_load<PackedRGBA8>(buf);
  WideRGBA8 r = pack_pixels_RGBA8();
  if (blend_key) r = blend_pixels_RGBA8(dst, r);
  if (DISCARD) mask |= bit_cast<PackedRGBA8>(fragment_shader->isPixelDiscarded);
  unaligned_store(buf, (mask & dst) | (~mask & pack(r)));
}

template <bool DISCARD>
static inline void commit_output(uint32_t* buf) {
  commit_output<DISCARD>(buf, 0);
}

template <>
inline void commit_output<false>(uint32_t* buf) {
  fragment_shader->run();
  WideRGBA8 r = pack_pixels_RGBA8();
  if (blend_key) r = blend_pixels_RGBA8(unaligned_load<PackedRGBA8>(buf), r);
  unaligned_store(buf, pack(r));
}

static inline void commit_span(uint32_t* buf, PackedRGBA8 r) {
  if (blend_key)
    r = pack(blend_pixels_RGBA8(unaligned_load<PackedRGBA8>(buf), unpack(r)));
  unaligned_store(buf, r);
}

UNUSED static inline void commit_solid_span(uint32_t* buf, PackedRGBA8 r,
                                            int len) {
  if (blend_key) {
    auto src = unpack(r);
    for (uint32_t* end = &buf[len]; buf < end; buf += 4) {
      unaligned_store(
          buf, pack(blend_pixels_RGBA8(unaligned_load<PackedRGBA8>(buf), src)));
    }
  } else {
    fill_n(buf, len, bit_cast<U32>(r).x);
  }
}

UNUSED static inline void commit_texture_span(uint32_t* buf, uint32_t* src,
                                              int len) {
  if (blend_key) {
    for (uint32_t* end = &buf[len]; buf < end; buf += 4, src += 4) {
      PackedRGBA8 r = unaligned_load<PackedRGBA8>(src);
      unaligned_store(buf, pack(blend_pixels_RGBA8(
                               unaligned_load<PackedRGBA8>(buf), unpack(r))));
    }
  } else {
    memcpy(buf, src, len * sizeof(uint32_t));
  }
}

static inline PackedRGBA8 span_mask_RGBA8(int span) {
  return bit_cast<PackedRGBA8>(I32(span) < I32{1, 2, 3, 4});
}

template <bool DISCARD>
static inline void commit_output(uint32_t* buf, int span) {
  commit_output<DISCARD>(buf, span_mask_RGBA8(span));
}

static inline WideR8 pack_pixels_R8(Float c) {
  return packR8(roundfast(c, 255.49f));
}

static inline WideR8 pack_pixels_R8() {
  return pack_pixels_R8(fragment_shader->gl_FragColor.x);
}

template <typename C>
static inline PackedR8 pack_span(uint8_t*, C c) {
  return pack(pack_pixels_R8(c));
}

static inline PackedR8 pack_span(uint8_t*) { return pack(pack_pixels_R8()); }

static inline WideR8 blend_pixels_R8(WideR8 dst, WideR8 src) {
  switch (blend_key) {
    case BLEND_KEY_NONE:
      return src;
    case BLEND_KEY(GL_ZERO, GL_SRC_COLOR):
      return muldiv255(src, dst);
    case BLEND_KEY(GL_ONE, GL_ONE):
      return src + dst;
    case BLEND_KEY(GL_ONE, GL_ZERO):
      return src;
    default:
      UNREACHABLE;
      // return src;
  }
}

template <bool DISCARD>
static inline void commit_output(uint8_t* buf, WideR8 mask) {
  fragment_shader->run();
  WideR8 dst = unpack(unaligned_load<PackedR8>(buf));
  WideR8 r = pack_pixels_R8();
  if (blend_key) r = blend_pixels_R8(dst, r);
  if (DISCARD) mask |= packR8(fragment_shader->isPixelDiscarded);
  unaligned_store(buf, pack((mask & dst) | (~mask & r)));
}

template <bool DISCARD>
static inline void commit_output(uint8_t* buf) {
  commit_output<DISCARD>(buf, 0);
}

template <>
inline void commit_output<false>(uint8_t* buf) {
  fragment_shader->run();
  WideR8 r = pack_pixels_R8();
  if (blend_key) r = blend_pixels_R8(unpack(unaligned_load<PackedR8>(buf)), r);
  unaligned_store(buf, pack(r));
}

static inline void commit_span(uint8_t* buf, PackedR8 r) {
  if (blend_key)
    r = pack(blend_pixels_R8(unpack(unaligned_load<PackedR8>(buf)), unpack(r)));
  unaligned_store(buf, r);
}

UNUSED static inline void commit_solid_span(uint8_t* buf, PackedR8 r, int len) {
  if (blend_key) {
    auto src = unpack(r);
    for (uint8_t* end = &buf[len]; buf < end; buf += 4) {
      unaligned_store(buf, pack(blend_pixels_R8(
                               unpack(unaligned_load<PackedR8>(buf)), src)));
    }
  } else {
    fill_n((uint32_t*)buf, len / 4, bit_cast<uint32_t>(r));
  }
}

static inline WideR8 span_mask_R8(int span) {
  return bit_cast<WideR8>(WideR8(span) < WideR8{1, 2, 3, 4});
}

template <bool DISCARD>
static inline void commit_output(uint8_t* buf, int span) {
  commit_output<DISCARD>(buf, span_mask_R8(span));
}

template <bool DISCARD, typename P, typename Z>
static inline void commit_output(P* buf, Z z, uint16_t* zbuf) {
  ZMask4 zmask;
  if (check_depth4<true, DISCARD>(z, zbuf, zmask)) {
    commit_output<DISCARD>(buf, unpack(zmask, buf));
    if (DISCARD) {
      discard_depth(z, zbuf, zmask);
    }
  } else {
    fragment_shader->skip();
  }
}

template <bool DISCARD, typename P, typename Z>
static inline void commit_output(P* buf, Z z, uint16_t* zbuf, int span) {
  ZMask4 zmask;
  if (check_depth4<false, DISCARD>(z, zbuf, zmask, span)) {
    commit_output<DISCARD>(buf, unpack(zmask, buf));
    if (DISCARD) {
      discard_depth(z, zbuf, zmask);
    }
  }
}

static const size_t MAX_FLATS = 64;
typedef float Flats[MAX_FLATS];

static const size_t MAX_INTERPOLANTS = 16;
typedef VectorType<float, MAX_INTERPOLANTS> Interpolants;

template <typename S, typename P>
static ALWAYS_INLINE void dispatch_draw_span(S* shader, P* buf, int len) {
  int drawn = shader->draw_span(buf, len);
  if (drawn) shader->step_interp_inputs(drawn >> 2);
  for (buf += drawn; drawn < len; drawn += 4, buf += 4) {
    S::run(shader);
    commit_span(buf, pack_span(buf));
  }
}

#include "texture.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#include "load_shader.h"
#pragma GCC diagnostic pop

template <int FUNC, bool MASK, typename P>
static inline void draw_depth_span(uint16_t z, P* buf, uint16_t* depth,
                                   int span) {
  int skip = 0;
  if (fragment_shader->has_draw_span(buf)) {
    int len = 0;
    do {
      ZMask8 zmask;
      switch (check_depth8<FUNC, MASK>(z, depth, zmask)) {
        case 0:
          if (len) {
            fragment_shader->draw_span(buf - len, len);
            len = 0;
          }
          skip += 2;
          break;
        case -1:
          if (skip) {
            fragment_shader->skip(skip);
            skip = 0;
          }
          len += 8;
          break;
        default:
          if (len) {
            fragment_shader->draw_span(buf - len, len);
            len = 0;
          } else if (skip) {
            fragment_shader->skip(skip);
            skip = 0;
          }
          commit_output<false>(buf, unpack(lowHalf(zmask), buf));
          commit_output<false>(buf + 4, unpack(highHalf(zmask), buf));
          break;
      }
      buf += 8;
      depth += 8;
      span -= 8;
    } while (span >= 8);
    if (len) {
      fragment_shader->draw_span(buf - len, len);
    }
  } else {
    do {
      ZMask8 zmask;
      switch (check_depth8<FUNC, MASK>(z, depth, zmask)) {
        case 0:
          skip += 2;
          break;
        case -1:
          if (skip) {
            fragment_shader->skip(skip);
            skip = 0;
          }
          commit_output<false>(buf);
          commit_output<false>(buf + 4);
          break;
        default:
          if (skip) {
            fragment_shader->skip(skip);
            skip = 0;
          }
          commit_output<false>(buf, unpack(lowHalf(zmask), buf));
          commit_output<false>(buf + 4, unpack(highHalf(zmask), buf));
          break;
      }
      buf += 8;
      depth += 8;
      span -= 8;
    } while (span >= 8);
  }
  if (skip) {
    fragment_shader->skip(skip);
  }
}

typedef vec2_scalar Point2D;
typedef vec4_scalar Point3D;

struct ClipRect {
  float x0;
  float y0;
  float x1;
  float y1;

  ClipRect(Texture& t) : x0(0), y0(0), x1(t.width), y1(t.height) {
    if (ctx->scissortest) {
      scissor(ctx->scissor);
    }
  }

  void scissor(const IntRect& scissor) {
    x0 = max(x0, float(scissor.x));
    y0 = max(y0, float(scissor.y));
    x1 = min(x1, float(scissor.x + scissor.width));
    y1 = min(y1, float(scissor.y + scissor.height));
  }

  template <typename P>
  bool overlaps(int nump, const P* p) const {
    int sides = 0;
    for (int i = 0; i < nump; i++) {
      sides |= p[i].x < x1 ? (p[i].x > x0 ? 1 | 2 : 1) : 2;
      sides |= p[i].y < y1 ? (p[i].y > y0 ? 4 | 8 : 4) : 8;
    }
    return sides == 0xF;
  }
};

template <typename P>
static inline void draw_quad_spans(int nump, Point2D p[4], uint16_t z,
                                   Interpolants interp_outs[4],
                                   Texture& colortex, int layer,
                                   Texture& depthtex,
                                   const ClipRect& clipRect) {
  Point2D l0, r0, l1, r1;
  int l0i, r0i, l1i, r1i;
  {
    int top = nump > 3 && p[3].y < p[2].y
                  ? (p[0].y < p[1].y ? (p[0].y < p[3].y ? 0 : 3)
                                     : (p[1].y < p[3].y ? 1 : 3))
                  : (p[0].y < p[1].y ? (p[0].y < p[2].y ? 0 : 2)
                                     : (p[1].y < p[2].y ? 1 : 2));
#define NEXT_POINT(idx)   \
  ({                      \
    int cur = (idx) + 1;  \
    cur < nump ? cur : 0; \
  })
#define PREV_POINT(idx)        \
  ({                           \
    int cur = (idx)-1;         \
    cur >= 0 ? cur : nump - 1; \
  })
    int next = NEXT_POINT(top);
    int prev = PREV_POINT(top);
    if (p[top].y == p[next].y) {
      l0i = next;
      l1i = NEXT_POINT(next);
      r0i = top;
      r1i = prev;
    } else if (p[top].y == p[prev].y) {
      l0i = top;
      l1i = next;
      r0i = prev;
      r1i = PREV_POINT(prev);
    } else {
      l0i = r0i = top;
      l1i = next;
      r1i = prev;
    }
    l0 = p[l0i];
    r0 = p[r0i];
    l1 = p[l1i];
    r1 = p[r1i];
    //    debugf("l0: %d(%f,%f), r0: %d(%f,%f) -> l1: %d(%f,%f), r1:
    //    %d(%f,%f)\n", l0i, l0.x, l0.y, r0i, r0.x, r0.y, l1i, l1.x, l1.y, r1i,
    //    r1.x, r1.y);
  }

  float lx = l0.x;
  float lk = 1.0f / (l1.y - l0.y);
  float lm = (l1.x - l0.x) * lk;
  float rx = r0.x;
  float rk = 1.0f / (r1.y - r0.y);
  float rm = (r1.x - r0.x) * rk;
  assert(l0.y == r0.y);
  float y = floor(max(l0.y, clipRect.y0) + 0.5f) + 0.5f;
  lx += (y - l0.y) * lm;
  rx += (y - r0.y) * rm;
  Interpolants lo = interp_outs[l0i];
  Interpolants lom = (interp_outs[l1i] - lo) * lk;
  lo = lo + lom * (y - l0.y);
  Interpolants ro = interp_outs[r0i];
  Interpolants rom = (interp_outs[r1i] - ro) * rk;
  ro = ro + rom * (y - r0.y);
  P* fbuf = (P*)colortex.buf + (layer * colortex.height + int(y)) *
                                   colortex.stride(sizeof(P)) / sizeof(P);
  uint16_t* fdepth =
      (uint16_t*)depthtex.buf +
      int(y) * depthtex.stride(sizeof(uint16_t)) / sizeof(uint16_t);
  while (y < clipRect.y1) {
    if (y > l1.y) {
      l0i = l1i;
      l0 = l1;
      l1i = NEXT_POINT(l1i);
      l1 = p[l1i];
      if (l1.y <= l0.y) break;
      lk = 1.0f / (l1.y - l0.y);
      lm = (l1.x - l0.x) * lk;
      lx = l0.x + (y - l0.y) * lm;
      lo = interp_outs[l0i];
      lom = (interp_outs[l1i] - lo) * lk;
      lo += lom * (y - l0.y);
    }
    if (y > r1.y) {
      r0i = r1i;
      r0 = r1;
      r1i = PREV_POINT(r1i);
      r1 = p[r1i];
      if (r1.y <= r0.y) break;
      rk = 1.0f / (r1.y - r0.y);
      rm = (r1.x - r0.x) * rk;
      rx = r0.x + (y - r0.y) * rm;
      ro = interp_outs[r0i];
      rom = (interp_outs[r1i] - ro) * rk;
      ro += rom * (y - r0.y);
    }
    int startx = int(max(min(lx, rx), clipRect.x0) + 0.5f);
    int endx = int(min(max(lx, rx), clipRect.x1) + 0.5f);
    int span = endx - startx;
    if (span > 0) {
      ctx->shaded_rows++;
      ctx->shaded_pixels += span;
      P* buf = fbuf + startx;
      uint16_t* depth = fdepth + startx;
      bool use_depth = depthtex.buf != nullptr;
      bool use_discard = fragment_shader->use_discard();
      if (depthtex.delay_clear) {
        int yi = int(y);
        uint32_t& mask = depthtex.cleared_rows[yi / 32];
        if ((mask & (1 << (yi & 31))) == 0) {
          switch (ctx->depthfunc) {
            case GL_LESS:
              if (int16_t(z) < int16_t(depthtex.clear_val))
                break;
              else
                goto next_span;
            case GL_LEQUAL:
              if (int16_t(z) <= int16_t(depthtex.clear_val))
                break;
              else
                goto next_span;
          }
          if (ctx->depthmask) {
            mask |= 1 << (yi & 31);
            depthtex.delay_clear--;
            if (use_discard) {
              clear_buffer<uint16_t>(depthtex, depthtex.clear_val, 0,
                                     depthtex.width, yi, yi + 1);
            } else {
              if (startx > 0 || endx < depthtex.width) {
                clear_buffer<uint16_t>(depthtex, depthtex.clear_val, 0,
                                       depthtex.width, yi, yi + 1, 0, startx,
                                       endx);
              }
              clear_buffer<uint16_t>(depthtex, z, startx, endx, yi, yi + 1);
              use_depth = false;
            }
          } else {
            use_depth = false;
          }
        }
      }
      if (colortex.delay_clear) {
        int yi = int(y);
        uint32_t& mask = colortex.cleared_rows[yi / 32];
        if ((mask & (1 << (yi & 31))) == 0) {
          mask |= 1 << (yi & 31);
          colortex.delay_clear--;
          if (use_depth || blend_key || use_discard) {
            clear_buffer<P>(colortex, colortex.clear_val, 0, colortex.width, yi,
                            yi + 1, layer);
          } else if (startx > 0 || endx < colortex.width) {
            clear_buffer<P>(colortex, colortex.clear_val, 0, colortex.width, yi,
                            yi + 1, layer, startx, endx);
          }
        }
      }
      fragment_shader->gl_FragCoord.x = init_interp(startx + 0.5f, 1);
      fragment_shader->gl_FragCoord.y = y;
      {
        Interpolants step = (ro - lo) * (1.0f / (rx - lx));
        Interpolants o = lo + step * (startx + 0.5f - lx);
        fragment_shader->init_span(&o, &step, 4.0f);
      }
      if (!use_discard) {
        if (use_depth) {
          if (span >= 8) {
            if (ctx->depthfunc == GL_LEQUAL) {
              if (ctx->depthmask)
                draw_depth_span<GL_LEQUAL, true>(z, buf, depth, span);
              else
                draw_depth_span<GL_LEQUAL, false>(z, buf, depth, span);
            } else {
              if (ctx->depthmask)
                draw_depth_span<GL_LESS, true>(z, buf, depth, span);
              else
                draw_depth_span<GL_LESS, false>(z, buf, depth, span);
            }
            buf += span & ~7;
            depth += span & ~7;
            span &= 7;
          }
          for (; span >= 4; span -= 4, buf += 4, depth += 4) {
            commit_output<false>(buf, z, depth);
          }
          if (span > 0) {
            commit_output<false>(buf, z, depth, span);
          }
        } else {
          if (span >= 4) {
            if (fragment_shader->has_draw_span(buf)) {
              int len = span & ~3;
              fragment_shader->draw_span(buf, len);
              buf += len;
              span &= 3;
            } else {
              do {
                commit_output<false>(buf);
                buf += 4;
                span -= 4;
              } while (span >= 4);
            }
          }
          if (span > 0) {
            commit_output<false>(buf, span);
          }
        }
      } else {
        if (use_depth) {
          for (; span >= 4; span -= 4, buf += 4, depth += 4) {
            commit_output<true>(buf, z, depth);
          }
          if (span > 0) {
            commit_output<true>(buf, z, depth, span);
          }
        } else {
          for (; span >= 4; span -= 4, buf += 4) {
            commit_output<true>(buf);
          }
          if (span > 0) {
            commit_output<true>(buf, span);
          }
        }
      }
    }
  next_span:
    lx += lm;
    rx += rm;
    y++;
    lo += lom;
    ro += rom;
    fbuf += colortex.stride(sizeof(P)) / sizeof(P);
    fdepth += depthtex.stride(sizeof(uint16_t)) / sizeof(uint16_t);
  }
}

template <typename P>
static inline void draw_perspective_spans(int nump, Point3D* p,
                                          Interpolants* interp_outs,
                                          Texture& colortex, int layer,
                                          Texture& depthtex,
                                          const ClipRect& clipRect) {
  Point3D l0, r0, l1, r1;
  int l0i, r0i, l1i, r1i;
  {
    // find a top point
    int top = 0;
    for (int i = 1; i < nump; i++) {
      if (p[i].y < p[top].y) {
        top = i;
      }
    }
    // find left-most top point
    l0i = top;
    for (int i = top + 1; i < nump && p[i].y == p[top].y; i++) {
      l0i = i;
    }
    if (l0i == nump - 1) {
      for (int i = 0; i <= top && p[i].y == p[top].y; i++) {
        l0i = i;
      }
    }
    // find right-most top point
    r0i = top;
    for (int i = top - 1; i >= 0 && p[i].y == p[top].y; i--) {
      r0i = i;
    }
    if (r0i == 0) {
      for (int i = nump - 1; i >= top && p[i].y == p[top].y; i--) {
        r0i = i;
      }
    }
    l1i = NEXT_POINT(l0i);
    r1i = PREV_POINT(r0i);
    l0 = p[l0i];
    r0 = p[r0i];
    l1 = p[l1i];
    r1 = p[r1i];
  }

  Point3D lc = l0;
  float lk = 1.0f / (l1.y - l0.y);
  Point3D lm = (l1 - l0) * lk;
  Point3D rc = r0;
  float rk = 1.0f / (r1.y - r0.y);
  Point3D rm = (r1 - r0) * rk;
  assert(l0.y == r0.y);
  float y = floor(max(l0.y, clipRect.y0) + 0.5f) + 0.5f;
  lc += (y - l0.y) * lm;
  rc += (y - r0.y) * rm;
  Interpolants lo = interp_outs[l0i] * l0.w;
  Interpolants lom = (interp_outs[l1i] * l1.w - lo) * lk;
  lo = lo + lom * (y - l0.y);
  Interpolants ro = interp_outs[r0i] * r0.w;
  Interpolants rom = (interp_outs[r1i] * r1.w - ro) * rk;
  ro = ro + rom * (y - r0.y);
  P* fbuf = (P*)colortex.buf + (layer * colortex.height + int(y)) *
                                   colortex.stride(sizeof(P)) / sizeof(P);
  uint16_t* fdepth =
      (uint16_t*)depthtex.buf +
      int(y) * depthtex.stride(sizeof(uint16_t)) / sizeof(uint16_t);
  while (y < clipRect.y1) {
    if (y > l1.y) {
      l0i = l1i;
      l0 = l1;
      l1i = NEXT_POINT(l1i);
      l1 = p[l1i];
      if (l1.y <= l0.y) break;
      lk = 1.0f / (l1.y - l0.y);
      lm = (l1 - l0) * lk;
      lc = l0 + (y - l0.y) * lm;
      lo = interp_outs[l0i] * l0.w;
      lom = (interp_outs[l1i] * l1.w - lo) * lk;
      lo += lom * (y - l0.y);
    }
    if (y > r1.y) {
      r0i = r1i;
      r0 = r1;
      r1i = PREV_POINT(r1i);
      r1 = p[r1i];
      if (r1.y <= r0.y) break;
      rk = 1.0f / (r1.y - r0.y);
      rm = (r1 - r0) * rk;
      rc = r0 + (y - r0.y) * rm;
      ro = interp_outs[r0i] * r0.w;
      rom = (interp_outs[r1i] * r1.w - ro) * rk;
      ro += rom * (y - r0.y);
    }
    int startx = int(max(min(lc.x, rc.x), clipRect.x0) + 0.5f);
    int endx = int(min(max(lc.x, rc.x), clipRect.x1) + 0.5f);
    int span = endx - startx;
    if (span > 0) {
      ctx->shaded_rows++;
      ctx->shaded_pixels += span;
      P* buf = fbuf + startx;
      uint16_t* depth = fdepth + startx;
      bool use_depth = depthtex.buf != nullptr;
      bool use_discard = fragment_shader->use_discard();
      if (depthtex.delay_clear) {
        int yi = int(y);
        uint32_t& mask = depthtex.cleared_rows[yi / 32];
        if ((mask & (1 << (yi & 31))) == 0) {
          mask |= 1 << (yi & 31);
          depthtex.delay_clear--;
          clear_buffer<uint16_t>(depthtex, depthtex.clear_val, 0,
                                 depthtex.width, yi, yi + 1);
        }
      }
      if (colortex.delay_clear) {
        int yi = int(y);
        uint32_t& mask = colortex.cleared_rows[yi / 32];
        if ((mask & (1 << (yi & 31))) == 0) {
          mask |= 1 << (yi & 31);
          colortex.delay_clear--;
          if (use_depth || blend_key || use_discard) {
            clear_buffer<P>(colortex, colortex.clear_val, 0, colortex.width, yi,
                            yi + 1, layer);
          } else if (startx > 0 || endx < colortex.width) {
            clear_buffer<P>(colortex, colortex.clear_val, 0, colortex.width, yi,
                            yi + 1, layer, startx, endx);
          }
        }
      }
      fragment_shader->gl_FragCoord.x = init_interp(startx + 0.5f, 1);
      fragment_shader->gl_FragCoord.y = y;
      {
        vec2_scalar stepZW =
            (rc.sel(Z, W) - lc.sel(Z, W)) * (1.0f / (rc.x - lc.x));
        vec2_scalar zw = lc.sel(Z, W) + stepZW * (startx + 0.5f - lc.x);
        fragment_shader->gl_FragCoord.z = init_interp(zw.x, stepZW.x);
        fragment_shader->gl_FragCoord.w = init_interp(zw.y, stepZW.y);
        fragment_shader->stepZW = stepZW * 4.0f;
        Interpolants step = (ro - lo) * (1.0f / (rc.x - lc.x));
        Interpolants o = lo + step * (startx + 0.5f - lc.x);
        fragment_shader->init_span(&o, &step, 4.0f);
      }
      if (!use_discard) {
        if (use_depth) {
          for (; span >= 4; span -= 4, buf += 4, depth += 4) {
            commit_output<false>(buf, packDepth(), depth);
          }
          if (span > 0) {
            commit_output<false>(buf, packDepth(), depth, span);
          }
        } else {
          for (; span >= 4; span -= 4, buf += 4) {
            commit_output<false>(buf);
          }
          if (span > 0) {
            commit_output<false>(buf, span);
          }
        }
      } else {
        if (use_depth) {
          for (; span >= 4; span -= 4, buf += 4, depth += 4) {
            commit_output<true>(buf, packDepth(), depth);
          }
          if (span > 0) {
            commit_output<true>(buf, packDepth(), depth, span);
          }
        } else {
          for (; span >= 4; span -= 4, buf += 4) {
            commit_output<true>(buf);
          }
          if (span > 0) {
            commit_output<true>(buf, span);
          }
        }
      }
    }
    lc += lm;
    rc += rm;
    y++;
    lo += lom;
    ro += rom;
    fbuf += colortex.stride(sizeof(P)) / sizeof(P);
    fdepth += depthtex.stride(sizeof(uint16_t)) / sizeof(uint16_t);
  }
}

// Clip a primitive against the near or far Z planes, producing intermediate
// vertexes with interpolated attributes that will no longer intersect the
// selected plane. This overwrites the vertexes in-place, producing at most
// N+1 vertexes for each invocation, so appropriate storage should be reserved
// before calling.
template <int SIDE>
static int clip_near_far(int nump, Point3D* p, Interpolants* interp) {
  int numClip = 0;
  Point3D prev = p[nump - 1];
  Interpolants prevInterp = interp[nump - 1];
  float prevDist = SIDE * prev.z - prev.w;
  for (int i = 0; i < nump; i++) {
    Point3D cur = p[i];
    Interpolants curInterp = interp[i];
    float curDist = SIDE * cur.z - cur.w;
    if (curDist < 0.0f && prevDist < 0.0f) {
      p[numClip] = cur;
      interp[numClip] = curInterp;
      numClip++;
    } else if (curDist < 0.0f || prevDist < 0.0f) {
      float k = prevDist / (prevDist - curDist);
      p[numClip] = prev + (cur - prev) * k;
      interp[numClip] = prevInterp + (curInterp - prevInterp) * k;
      numClip++;
    }
    prev = cur;
    prevInterp = curInterp;
    prevDist = curDist;
  }
  return numClip;
}

// Draws a perspective-correct 3D primitive with varying Z value, as opposed
// to a simple 2D planar primitive with a constant Z value that could be
// trivially Z rejected. This requires clipping the primitive against the near
// and far planes to ensure it stays within the valid Z-buffer range. The Z
// and W of each fragment of the primitives are interpolated across the
// generated spans and then depth-tested as appropriate.
// Additionally, vertex attributes must be interpolated with perspective-
// correction by dividing by W before interpolation, and then later multiplied
// by W again to produce the final correct attribute value for each fragment.
// This process is expensive and should be avoided if possible for primitive
// batches that are known ahead of time to not need perspective-correction.
// To trigger this path, the shader should use the PERSPECTIVE feature so that
// the glsl-to-cxx compiler can generate the appropriate interpolation code
// needed to participate with SWGL's perspective-correction.
static void draw_perspective(int nump, Texture& colortex, int layer,
                             Texture& depthtex) {
  Flats flat_outs;
  Interpolants interp_outs[6] = {0};
  vertex_shader->run((char*)flat_outs, (char*)interp_outs,
                     sizeof(Interpolants));

  Point3D p[6];
  vec4 pos = vertex_shader->gl_Position;
  vec3_scalar scale(ctx->viewport.width * 0.5f, ctx->viewport.height * 0.5f,
                    0.5f);
  vec3_scalar offset(ctx->viewport.x, ctx->viewport.y, 0.0f);
  offset += scale;
  if (test_none(pos.z < -pos.w || pos.z > pos.w)) {
    Float w = 1.0f / pos.w;
    vec3 screen = pos.sel(X, Y, Z) * w * scale + offset;
    p[0] = Point3D(screen.x.x, screen.y.x, screen.z.x, w.x);
    p[1] = Point3D(screen.x.y, screen.y.y, screen.z.y, w.y);
    p[2] = Point3D(screen.x.z, screen.y.z, screen.z.z, w.z);
    p[3] = Point3D(screen.x.w, screen.y.w, screen.z.w, w.w);
  } else {
    p[0] = Point3D(pos.x.x, pos.y.x, pos.z.x, pos.w.x);
    p[1] = Point3D(pos.x.y, pos.y.y, pos.z.y, pos.w.y);
    p[2] = Point3D(pos.x.z, pos.y.z, pos.z.z, pos.w.z);
    p[3] = Point3D(pos.x.w, pos.y.w, pos.z.w, pos.w.w);
    nump = clip_near_far<-1>(nump, p, interp_outs);
    if (nump < 3) {
      return;
    }
    nump = clip_near_far<1>(nump, p, interp_outs);
    if (nump < 3) {
      return;
    }
    for (int i = 0; i < nump; i++) {
      float w = 1.0f / p[i].w;
      p[i] = Point3D(p[i].sel(X, Y, Z) * w * scale + offset, w);
    }
  }

  ClipRect clipRect(colortex);
  if (!clipRect.overlaps(nump, p)) {
    return;
  }

  fragment_shader->init_primitive(flat_outs);

  if (colortex.internal_format == GL_RGBA8) {
    draw_perspective_spans<uint32_t>(nump, p, interp_outs, colortex, layer,
                                     depthtex, clipRect);
  } else if (colortex.internal_format == GL_R8) {
    draw_perspective_spans<uint8_t>(nump, p, interp_outs, colortex, layer,
                                    depthtex, clipRect);
  } else {
    assert(false);
  }
}

static void draw_quad(int nump, Texture& colortex, int layer,
                      Texture& depthtex) {
  if (fragment_shader->use_perspective()) {
    draw_perspective(nump, colortex, layer, depthtex);
    return;
  }

  Flats flat_outs;
  Interpolants interp_outs[4] = {0};
  vertex_shader->run((char*)flat_outs, (char*)interp_outs,
                     sizeof(Interpolants));

  vec4 pos = vertex_shader->gl_Position;
  float w = 1.0f / pos.w.x;
  vec2 screen =
      (pos.sel(X, Y) * w + 1) *
          vec2_scalar(ctx->viewport.width * 0.5f, ctx->viewport.height * 0.5f) +
      vec2_scalar(ctx->viewport.x, ctx->viewport.y);
  Point2D p[4] = {{screen.x.x, screen.y.x},
                  {screen.x.y, screen.y.y},
                  {screen.x.z, screen.y.z},
                  {screen.x.w, screen.y.w}};

  ClipRect clipRect(colortex);
  if (!clipRect.overlaps(nump, p)) {
    return;
  }

  float screenZ = (vertex_shader->gl_Position.z.x * w + 1) * 0.5f;
  if (screenZ < 0 || screenZ > 1) {
    return;
  }
  // SSE2 does not support unsigned comparison, so bias Z to be negative.
  uint16_t z = uint16_t(0xFFFF * screenZ) - 0x8000;
  fragment_shader->gl_FragCoord.z = screenZ;
  fragment_shader->gl_FragCoord.w = w;

  fragment_shader->init_primitive(flat_outs);

  if (colortex.internal_format == GL_RGBA8) {
    draw_quad_spans<uint32_t>(nump, p, z, interp_outs, colortex, layer,
                              depthtex, clipRect);
  } else if (colortex.internal_format == GL_R8) {
    draw_quad_spans<uint8_t>(nump, p, z, interp_outs, colortex, layer, depthtex,
                             clipRect);
  } else {
    assert(false);
  }
}

void VertexArray::validate() {
  int last_enabled = -1;
  for (int i = 0; i <= max_attrib; i++) {
    if (attribs[i].enabled) {
      VertexAttrib& attr = attribs[i];
      // VertexArray &v = ctx->vertex_arrays[attr.vertex_array];
      Buffer& vertex_buf = ctx->buffers[attr.vertex_buffer];
      attr.buf = vertex_buf.buf;
      attr.buf_size = vertex_buf.size;
      // debugf("%d %x %d %d %d %d\n", i, attr.type, attr.size, attr.stride,
      // attr.offset, attr.divisor);
      last_enabled = i;
    }
  }
  max_attrib = last_enabled;
}

extern "C" {

void DrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                           void* indicesptr, GLsizei instancecount) {
  assert(mode == GL_TRIANGLES || mode == GL_QUADS);
  assert(type == GL_UNSIGNED_SHORT || type == GL_UNSIGNED_INT);
  assert(count == 6);
  assert(indicesptr == nullptr);
  if (count <= 0 || instancecount <= 0 || indicesptr) {
    return;
  }

  Framebuffer& fb = *get_framebuffer(GL_DRAW_FRAMEBUFFER);
  Texture& colortex = ctx->textures[fb.color_attachment];
  if (!colortex.buf) {
    return;
  }
  assert(colortex.internal_format == GL_RGBA8 ||
         colortex.internal_format == GL_R8);
  Texture& depthtex = ctx->textures[ctx->depthtest ? fb.depth_attachment : 0];
  if (depthtex.buf) {
    assert(depthtex.internal_format == GL_DEPTH_COMPONENT16);
    assert(colortex.width == depthtex.width &&
           colortex.height == depthtex.height);
  }

  Buffer& indices_buf = ctx->buffers[ctx->element_array_buffer_binding];
  if (!indices_buf.buf) {
    return;
  }

  // debugf("current_vertex_array %d\n", ctx->current_vertex_array);
  // debugf("indices size: %d\n", indices_buf.size);
  VertexArray& v = ctx->vertex_arrays[ctx->current_vertex_array];
  if (ctx->validate_vertex_array) {
    ctx->validate_vertex_array = false;
    v.validate();
  }

#ifndef NDEBUG
  // uint64_t start = get_time_value();
#endif

  ctx->shaded_rows = 0;
  ctx->shaded_pixels = 0;

  uint16_t* indices = (uint16_t*)indices_buf.buf;
  if (type == GL_UNSIGNED_INT) {
    assert(indices_buf.size == size_t(count) * 4);
    indices = new uint16_t[count];
    for (GLsizei i = 0; i < count; i++) {
      uint32_t val = ((uint32_t*)indices_buf.buf)[i];
      assert(val <= 0xFFFFU);
      indices[i] = val;
    }
  } else if (type == GL_UNSIGNED_SHORT) {
    assert(indices_buf.size == size_t(count) * 2);
  } else {
    assert(0);
  }

  vertex_shader->init_batch(program_impl);
  fragment_shader->init_batch(program_impl);
  if (count == 6 && indices[3] == indices[2] && indices[4] == indices[1]) {
    uint16_t quad_indices[4] = {indices[0], indices[1], indices[5], indices[2]};
    vertex_shader->load_attribs(program_impl, v.attribs, quad_indices, 0, 0, 4);
    // debugf("emulate quad %d %d %d %d\n", indices[0], indices[1], indices[5],
    // indices[2]);
    draw_quad(4, colortex, fb.layer, depthtex);
    for (GLsizei instance = 1; instance < instancecount; instance++) {
      vertex_shader->load_attribs(program_impl, v.attribs, nullptr, 0, instance,
                                  4);
      draw_quad(4, colortex, fb.layer, depthtex);
    }
  } else {
    for (GLsizei instance = 0; instance < instancecount; instance++) {
      if (mode == GL_QUADS)
        for (GLsizei i = 0; i + 4 <= count; i += 4) {
          vertex_shader->load_attribs(program_impl, v.attribs, indices, i,
                                      instance, 4);
          // debugf("native quad %d %d %d %d\n", indices[i], indices[i+1],
          // indices[i+2], indices[i+3]);
          draw_quad(4, colortex, fb.layer, depthtex);
        }
      else
        for (GLsizei i = 0; i + 3 <= count; i += 3) {
          if (i + 6 <= count && indices[i + 3] == indices[i + 2] &&
              indices[i + 4] == indices[i + 1]) {
            uint16_t quad_indices[4] = {indices[i], indices[i + 1],
                                        indices[i + 5], indices[i + 2]};
            vertex_shader->load_attribs(program_impl, v.attribs, quad_indices,
                                        0, instance, 4);
            // debugf("emulate quad %d %d %d %d\n", indices[i], indices[i+1],
            // indices[i+5], indices[i+2]);
            draw_quad(4, colortex, fb.layer, depthtex);
            i += 3;
          } else {
            vertex_shader->load_attribs(program_impl, v.attribs, indices, i,
                                        instance, 3);
            // debugf("triangle %d %d %d %d\n", indices[i], indices[i+1],
            // indices[i+2]);
            draw_quad(3, colortex, fb.layer, depthtex);
          }
        }
    }
  }

  if (indices != (uint16_t*)indices_buf.buf) {
    delete[] indices;
  }

  if (ctx->samples_passed_query) {
    Query& q = ctx->queries[ctx->samples_passed_query];
    q.value += ctx->shaded_pixels;
  }

#ifndef NDEBUG
  // uint64_t end = get_time_value();
  // debugf("draw(%d): %fms for %d pixels in %d rows (avg %f pixels/row, %f
  // ns/pixel)\n", instancecount, double(end - start)/(1000.*1000.),
  // ctx->shaded_pixels, ctx->shaded_rows,
  // double(ctx->shaded_pixels)/ctx->shaded_rows, double(end -
  // start)/max(ctx->shaded_pixels, 1));
#endif
}

void Finish() {}

void MakeCurrent(void* ctx_ptr) {
  ctx = (Context*)ctx_ptr;
  if (ctx) {
    setup_program(ctx->current_program);
    blend_key = ctx->blend ? ctx->blend_key : BLEND_KEY_NONE;
  } else {
    setup_program(0);
    blend_key = BLEND_KEY_NONE;
  }
}

void* CreateContext() { return new Context; }

void DestroyContext(void* ctx_ptr) {
  if (!ctx_ptr) {
    return;
  }
  if (ctx == ctx_ptr) {
    MakeCurrent(nullptr);
  }
  delete (Context*)ctx_ptr;
}

void Composite(GLuint srcId, GLint srcX, GLint srcY, GLsizei srcWidth,
               GLsizei srcHeight, GLint dstX, GLint dstY, GLboolean opaque,
               GLboolean flip) {
  Framebuffer& fb = ctx->framebuffers[0];
  if (!fb.color_attachment) {
    return;
  }
  Texture& srctex = ctx->textures[srcId];
  if (!srctex.buf) return;
  prepare_texture(srctex);
  Texture& dsttex = ctx->textures[fb.color_attachment];
  if (!dsttex.buf) return;
  assert(srctex.bpp() == 4);
  const int bpp = 4;
  size_t src_stride = srctex.stride(bpp);
  size_t dest_stride = dsttex.stride(bpp);
  if (srcY < 0) {
    dstY -= srcY;
    srcHeight += srcY;
    srcY = 0;
  }
  if (dstY < 0) {
    srcY -= dstY;
    srcHeight += dstY;
    dstY = 0;
  }
  if (srcY + srcHeight > srctex.height) {
    srcHeight = srctex.height - srcY;
  }
  if (dstY + srcHeight > dsttex.height) {
    srcHeight = dsttex.height - dstY;
  }
  IntRect skip = {dstX, dstY, srcWidth, srcHeight};
  prepare_texture(dsttex, &skip);
  char* dest = dsttex.buf +
               (flip ? dsttex.height - 1 - dstY : dstY) * dest_stride +
               dstX * bpp;
  char* src = srctex.buf + srcY * src_stride + srcX * bpp;
  if (flip) {
    dest_stride = -dest_stride;
  }
  if (opaque) {
    for (int y = 0; y < srcHeight; y++) {
      memcpy(dest, src, srcWidth * bpp);
      dest += dest_stride;
      src += src_stride;
    }
  } else {
    for (int y = 0; y < srcHeight; y++) {
      char* end = src + srcWidth * bpp;
      while (src + 4 * bpp <= end) {
        WideRGBA8 srcpx = unpack(unaligned_load<PackedRGBA8>(src));
        WideRGBA8 dstpx = unpack(unaligned_load<PackedRGBA8>(dest));
        PackedRGBA8 r = pack(srcpx + dstpx - muldiv255(dstpx, alphas(srcpx)));
        unaligned_store(dest, r);
        src += 4 * bpp;
        dest += 4 * bpp;
      }
      if (src < end) {
        WideRGBA8 srcpx = unpack(unaligned_load<PackedRGBA8>(src));
        WideRGBA8 dstpx = unpack(unaligned_load<PackedRGBA8>(dest));
        U32 r = bit_cast<U32>(
            pack(srcpx + dstpx - muldiv255(dstpx, alphas(srcpx))));
        unaligned_store(dest, r.x);
        if (src + bpp < end) {
          unaligned_store(dest + bpp, r.y);
          if (src + 2 * bpp < end) {
            unaligned_store(dest + 2 * bpp, r.z);
          }
        }
        dest += end - src;
        src = end;
      }
      dest += dest_stride - srcWidth * bpp;
      src += src_stride - srcWidth * bpp;
    }
  }
}

}  // extern "C"
