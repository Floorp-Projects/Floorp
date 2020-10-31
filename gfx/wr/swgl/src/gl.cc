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
#  define NO_INLINE __declspec(noinline)
#else
#  define ALWAYS_INLINE __attribute__((always_inline)) inline
#  define NO_INLINE __attribute__((noinline))
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
#include "texture.h"

using namespace glsl;

struct IntRect {
  int x0;
  int y0;
  int x1;
  int y1;

  int width() const { return x1 - x0; }
  int height() const { return y1 - y0; }
  bool is_empty() const { return width() <= 0 || height() <= 0; }

  bool same_size(const IntRect& o) const {
    return width() == o.width() && height() == o.height();
  }

  bool contains(const IntRect& o) const {
    return o.x0 >= x0 && o.y0 >= y0 && o.x1 <= x1 && o.y1 <= y1;
  }

  IntRect& intersect(const IntRect& o) {
    x0 = max(x0, o.x0);
    y0 = max(y0, o.y0);
    x1 = min(x1, o.x1);
    y1 = min(y1, o.y1);
    return *this;
  }

  // Scale from source-space to dest-space, optionally rounding inward
  IntRect& scale(int srcWidth, int srcHeight, int dstWidth, int dstHeight,
                 bool roundIn = false) {
    x0 = (x0 * dstWidth + (roundIn ? srcWidth - 1 : 0)) / srcWidth;
    y0 = (y0 * dstHeight + (roundIn ? srcHeight - 1 : 0)) / srcHeight;
    x1 = (x1 * dstWidth) / srcWidth;
    y1 = (y1 * dstHeight) / srcHeight;
    return *this;
  }

  // Flip the rect's Y coords around inflection point at Y=offset
  void invert_y(int offset) {
    y0 = offset - y0;
    y1 = offset - y1;
    swap(y0, y1);
  }

  IntRect& offset(int dx, int dy) {
    x0 += dx;
    y0 += dy;
    x1 += dx;
    y1 += dy;
    return *this;
  }
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
    case GL_RG8:
    case GL_RG:
      return 2;
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32:
      return 4;
    case GL_RGB_RAW_422_APPLE:
      return 2;
    case GL_R16:
      return 2;
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
    case GL_RG8:
      return TextureFormat::RG8;
    case GL_R16:
      return TextureFormat::R16;
    case GL_RGB_RAW_422_APPLE:
      return TextureFormat::YUV422;
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

// The SWGL depth buffer is roughly organized as a span buffer where each row
// of the depth buffer is a list of spans, and each span has a constant depth
// and a run length (represented by DepthRun). The span from start..start+count
// is placed directly at that start index in the row's array of runs, so that
// there is no need to explicitly record the start index at all. This also
// avoids the need to move items around in the run array to manage insertions
// since space is implicitly always available for a run between any two
// pre-existing runs. Linkage from one run to the next is implicitly defined by
// the count, so if a run exists from start..start+count, the next run will
// implicitly pick up right at index start+count where that preceding run left
// off. All of the DepthRun items that are after the head of the run can remain
// uninitialized until the run needs to be split and a new run needs to start
// somewhere in between.
// For uses like perspective-correct rasterization or with a discard mask, a
// run is not an efficient representation, and it is more beneficial to have
// a flattened array of individual depth samples that can be masked off easily.
// To support this case, the first run in a given row's run array may have a
// zero count, signaling that this entire row is flattened. Critically, the
// depth and count fields in DepthRun are ordered (endian-dependently) so that
// the DepthRun struct can be interpreted as a sign-extended int32_t depth. It
// is then possible to just treat the entire row as an array of int32_t depth
// samples that can be processed with SIMD comparisons, since the count field
// behaves as just the sign-extension of the depth field.
// When a depth buffer is cleared, each row is initialized to a single run
// spanning the entire row. In the normal case, the depth buffer will continue
// to manage itself as a list of runs. If perspective or discard is used for
// a given row, the row will be converted to the flattened representation to
// support it, after which it will only ever revert back to runs if the depth
// buffer is cleared.
struct DepthRun {
  // Ensure that depth always occupies the LSB and count the MSB so that we
  // can sign-extend depth just by setting count to zero, marking it flat.
  // When count is non-zero, then this is interpreted as an actual run and
  // depth is read in isolation.
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  uint16_t depth;
  uint16_t count;
#else
  uint16_t count;
  uint16_t depth;
#endif

  DepthRun() = default;
  DepthRun(uint16_t depth, uint16_t count) : depth(depth), count(count) {}

  // If count is zero, this is actually a flat depth sample rather than a run.
  bool is_flat() const { return !count; }

  // Compare a source depth from rasterization with a stored depth value.
  template <int FUNC>
  ALWAYS_INLINE bool compare(uint16_t src) const {
    switch (FUNC) {
      case GL_LEQUAL:
        return src <= depth;
      case GL_LESS:
        return src < depth;
      case GL_ALWAYS:
        return true;
      default:
        assert(false);
        return false;
    }
  }
};

// A cursor for reading and modifying a row's depth run array. It locates
// and iterates through a desired span within all the runs, testing if
// the depth of this span passes or fails the depth test against existing
// runs. If desired, new runs may be inserted to represent depth occlusion
// from this span in the run array.
struct DepthCursor {
  // Current position of run the cursor has advanced to.
  DepthRun* cur = nullptr;
  // The start of the remaining potential samples in the desired span.
  DepthRun* start = nullptr;
  // The end of the potential samples in the desired span.
  DepthRun* end = nullptr;

  DepthCursor() = default;

  // Construct a cursor with runs for a given row's run array and the bounds
  // of the span we wish to iterate within it.
  DepthCursor(DepthRun* runs, int num_runs, int span_offset, int span_count)
      : cur(runs), start(&runs[span_offset]), end(start + span_count) {
    // This cursor should never iterate over flat runs
    assert(!runs->is_flat());
    DepthRun* end_runs = &runs[num_runs];
    // Clamp end of span to end of row
    if (end > end_runs) {
      end = end_runs;
    }
    // If the span starts past the end of the row, just advance immediately
    // to it to signal that we're done.
    if (start >= end_runs) {
      cur = end_runs;
      start = end_runs;
      return;
    }
    // Otherwise, find the first depth run that contains the start of the span.
    // If the span starts after the given run, then we need to keep searching
    // through the row to find an appropriate run. The check above already
    // guaranteed that the span starts within the row's runs, and the search
    // won't fall off the end.
    for (;;) {
      assert(cur < end);
      DepthRun* next = cur + cur->count;
      if (start < next) {
        break;
      }
      cur = next;
    }
  }

  // The cursor is valid if the current position is at the end or if the run
  // contains the start position.
  bool valid() const {
    return cur >= end || (cur <= start && start < cur + cur->count);
  }

  // Skip past any initial runs that fail the depth test. If we find a run that
  // would pass, then return the accumulated length between where we started
  // and that position. Otherwise, if we fall off the end, return -1 to signal
  // that there are no more passed runs at the end of this failed region and
  // so it is safe for the caller to stop processing any more regions in this
  // row.
  template <int FUNC>
  int skip_failed(uint16_t val) {
    assert(valid());
    DepthRun* prev = start;
    while (cur < end) {
      if (cur->compare<FUNC>(val)) {
        return start - prev;
      }
      cur += cur->count;
      start = cur;
    }
    return -1;
  }

  // Helper to convert function parameters into template parameters to hoist
  // some checks out of inner loops.
  ALWAYS_INLINE int skip_failed(uint16_t val, GLenum func) {
    switch (func) {
      case GL_LEQUAL:
        return skip_failed<GL_LEQUAL>(val);
      case GL_LESS:
        return skip_failed<GL_LESS>(val);
      default:
        assert(false);
        return -1;
    }
  }

  // Find a region of runs that passes the depth test. It is assumed the caller
  // has called skip_failed first to skip past any runs that failed the depth
  // test. This stops when it finds a run that fails the depth test or we fall
  // off the end of the row. If the write mask is enabled, this will insert runs
  // to represent this new region that passed the depth test. The length of the
  // region is returned.
  template <int FUNC, bool MASK>
  int check_passed(uint16_t val) {
    assert(valid());
    DepthRun* prev = cur;
    while (cur < end) {
      if (!cur->compare<FUNC>(val)) {
        break;
      }
      DepthRun* next = cur + cur->count;
      if (next > end) {
        if (MASK) {
          // Chop the current run where the end of the span falls, making a new
          // run from the end of the span till the next run. The beginning of
          // the current run will be folded into the run from the start of the
          // passed region before returning below.
          *end = DepthRun(cur->depth, next - end);
        }
        // If the next run starts past the end, then just advance the current
        // run to the end to signal that we're now at the end of the row.
        next = end;
      }
      cur = next;
    }
    // If we haven't advanced past the start of the span region, then we found
    // nothing that passed.
    if (cur <= start) {
      return 0;
    }
    // If 'end' fell within the middle of a passing run, then 'cur' will end up
    // pointing at the new partial run created at 'end' where the passing run
    // was split to accommodate starting in the middle. The preceding runs will
    // be fixed below to properly join with this new split.
    int passed = cur - start;
    if (MASK) {
      // If the search started from a run before the start of the span, then
      // edit that run to meet up with the start.
      if (prev < start) {
        prev->count = start - prev;
      }
      // Create a new run for the entirety of the passed samples.
      *start = DepthRun(val, passed);
    }
    start = cur;
    return passed;
  }

  // Helper to convert function parameters into template parameters to hoist
  // some checks out of inner loops.
  template <bool MASK>
  ALWAYS_INLINE int check_passed(uint16_t val, GLenum func) {
    switch (func) {
      case GL_LEQUAL:
        return check_passed<GL_LEQUAL, MASK>(val);
      case GL_LESS:
        return check_passed<GL_LESS, MASK>(val);
      default:
        assert(false);
        return 0;
    }
  }

  ALWAYS_INLINE int check_passed(uint16_t val, GLenum func, bool mask) {
    return mask ? check_passed<true>(val, func)
                : check_passed<false>(val, func);
  }

  // Fill a region of runs with a given depth value, bypassing any depth test.
  ALWAYS_INLINE void fill(uint16_t depth) {
    check_passed<GL_ALWAYS, true>(depth);
  }
};

struct Texture {
  GLenum internal_format = 0;
  int width = 0;
  int height = 0;
  int depth = 0;
  char* buf = nullptr;
  size_t buf_size = 0;
  uint32_t buf_stride = 0;
  uint8_t buf_bpp = 0;
  GLenum min_filter = GL_NEAREST;
  GLenum mag_filter = GL_LINEAR;
  // The number of active locks on this texture. If this texture has any active
  // locks, we need to disallow modifying or destroying the texture as it may
  // be accessed by other threads where modifications could lead to races.
  int32_t locked = 0;

  enum FLAGS {
    // If the buffer is internally-allocated by SWGL
    SHOULD_FREE = 1 << 1,
    // If the buffer has been cleared to initialize it. Currently this is only
    // utilized by depth buffers which need to know when depth runs have reset
    // to a valid row state. When unset, the depth runs may contain garbage.
    CLEARED = 1 << 2,
  };
  int flags = SHOULD_FREE;
  bool should_free() const { return bool(flags & SHOULD_FREE); }
  bool cleared() const { return bool(flags & CLEARED); }

  void set_flag(int flag, bool val) {
    if (val) {
      flags |= flag;
    } else {
      flags &= ~flag;
    }
  }
  void set_should_free(bool val) { set_flag(SHOULD_FREE, val); }
  void set_cleared(bool val) { set_flag(CLEARED, val); }

  // Delayed-clearing state. When a clear of an FB is requested, we don't
  // immediately clear each row, as the rows may be subsequently overwritten
  // by draw calls, allowing us to skip the work of clearing the affected rows
  // either fully or partially. Instead, we keep a bit vector of rows that need
  // to be cleared later and save the value they need to be cleared with so
  // that we can clear these rows individually when they are touched by draws.
  // This currently only works for 2D textures, but not on texture arrays.
  int delay_clear = 0;
  uint32_t clear_val = 0;
  uint32_t* cleared_rows = nullptr;

  void init_depth_runs(uint16_t z);
  void fill_depth_runs(uint16_t z);

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

  int bpp() const { return buf_bpp; }
  void set_bpp() { buf_bpp = bytes_for_internal_format(internal_format); }

  size_t stride() const { return buf_stride; }
  void set_stride() { buf_stride = aligned_stride(buf_bpp * width); }

  // Set an external backing buffer of this texture.
  void set_buffer(void* new_buf, size_t new_stride) {
    assert(!should_free());
    // Ensure that the supplied stride is at least as big as the internally
    // calculated aligned stride.
    set_bpp();
    set_stride();
    assert(new_stride >= buf_stride);

    buf = (char*)new_buf;
    buf_size = 0;
    buf_stride = new_stride;
  }

  bool allocate(bool force = false, int min_width = 0, int min_height = 0) {
    assert(!locked);  // Locked textures shouldn't be reallocated
    // If we get here, some GL API call that invalidates the texture was used.
    // Mark the buffer as not-cleared to signal this.
    set_cleared(false);
    // Check if there is either no buffer currently or if we forced validation
    // of the buffer size because some dimension might have changed.
    if ((!buf || force) && should_free()) {
      // Initialize the buffer's BPP and stride, since they may have changed.
      set_bpp();
      set_stride();
      // Compute new size based on the maximum potential stride, rather than
      // the current stride, to hopefully avoid reallocations when size would
      // otherwise change too much...
      size_t max_stride = max(buf_stride, aligned_stride(buf_bpp * min_width));
      size_t size = max_stride * max(height, min_height) * max(depth, 1);
      if (!buf || size > buf_size) {
        // Allocate with a SIMD register-sized tail of padding at the end so we
        // can safely read or write past the end of the texture with SIMD ops.
        char* new_buf = (char*)realloc(buf, size + sizeof(Float));
        assert(new_buf);
        if (new_buf) {
          // Successfully reallocated the buffer, so go ahead and set it.
          buf = new_buf;
          buf_size = size;
          return true;
        }
        // Allocation failed, so ensure we don't leave stale buffer state.
        cleanup();
      }
    }
    // Nothing changed...
    return false;
  }

  void cleanup() {
    assert(!locked);  // Locked textures shouldn't be destroyed
    if (buf && should_free()) {
      free(buf);
      buf = nullptr;
      buf_size = 0;
      buf_bpp = 0;
      buf_stride = 0;
    }
    disable_delayed_clear();
  }

  ~Texture() { cleanup(); }

  IntRect bounds() const { return IntRect{0, 0, width, height}; }

  // Find the valid sampling bounds relative to the requested region
  IntRect sample_bounds(const IntRect& req, bool invertY = false) const {
    IntRect bb = bounds().intersect(req).offset(-req.x0, -req.y0);
    if (invertY) bb.invert_y(req.height());
    return bb;
  }

  // Get a pointer for sampling at the given offset
  char* sample_ptr(int x, int y, int z = 0) const {
    return buf + (height * z + y) * stride() + x * bpp();
  }

  // Get a pointer for sampling the requested region and limit to the provided
  // sampling bounds
  char* sample_ptr(const IntRect& req, const IntRect& bounds, int z,
                   bool invertY = false) const {
    // Offset the sample pointer by the clamped bounds
    int x = req.x0 + bounds.x0;
    // Invert the Y offset if necessary
    int y = invertY ? req.y1 - 1 - bounds.y0 : req.y0 + bounds.y0;
    return sample_ptr(x, y, z);
  }
};

// The last vertex attribute is reserved as a null attribute in case a vertex
// attribute is used without being set.
#define MAX_ATTRIBS 17
#define NULL_ATTRIB 16
struct VertexArray {
  VertexAttrib attribs[MAX_ATTRIBS];
  int max_attrib = -1;
  // The GL spec defines element array buffer binding to be part of VAO state.
  GLuint element_array_buffer_binding = 0;

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

  ~Program() { delete impl; }
};

// clang-format off
// for GL defines to fully expand
#define CONCAT_KEY(prefix, x, y, z, w, ...) prefix##x##y##z##w
#define BLEND_KEY(...) CONCAT_KEY(BLEND_, __VA_ARGS__, 0, 0)
#define FOR_EACH_BLEND_KEY(macro)                                              \
  macro(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)  \
  macro(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, 0, 0)                                  \
  macro(GL_ZERO, GL_ONE_MINUS_SRC_COLOR, 0, 0)                                 \
  macro(GL_ZERO, GL_ONE_MINUS_SRC_COLOR, GL_ZERO, GL_ONE)                      \
  macro(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA, 0, 0)                                 \
  macro(GL_ZERO, GL_SRC_COLOR, 0, 0)                                           \
  macro(GL_ONE, GL_ONE, 0, 0)                                                  \
  macro(GL_ONE, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)                        \
  macro(GL_ONE, GL_ZERO, 0, 0)                                                 \
  macro(GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ZERO, GL_ONE)                       \
  macro(GL_CONSTANT_COLOR, GL_ONE_MINUS_SRC_COLOR, 0, 0)                       \
  macro(GL_ONE, GL_ONE_MINUS_SRC1_COLOR, 0, 0)
// clang-format on

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

  template <typename T>
  void on_erase(T*, ...) {}
  template <typename T>
  void on_erase(T* o, decltype(&T::on_erase)) {
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
  int32_t references = 1;

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
        return vertex_arrays[current_vertex_array].element_array_buffer_binding;
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

  IntRect apply_scissor(IntRect bb) const {
    return scissortest ? bb.intersect(scissor) : bb;
  }
};
static Context* ctx = nullptr;
static VertexShaderImpl* vertex_shader = nullptr;
static FragmentShaderImpl* fragment_shader = nullptr;
static BlendKey blend_key = BLEND_KEY_NONE;

static void prepare_texture(Texture& t, const IntRect* skip = nullptr);

template <typename S>
static inline void init_depth(S* s, Texture& t) {
  s->depth = max(t.depth, 1);
  s->height_stride = s->stride * t.height;
}

template <typename S>
static inline void init_filter(S* s, Texture& t) {
  // If the width is not at least 2 pixels, then we can't safely sample the end
  // of the row with a linear filter. In that case, just punt to using nearest
  // filtering instead.
  s->filter = t.width >= 2 ? gl_filter_to_texture_filter(t.mag_filter)
                           : TextureFilter::NEAREST;
}

template <typename S>
static inline void init_sampler(S* s, Texture& t) {
  prepare_texture(t);
  s->width = t.width;
  s->height = t.height;
  s->stride = t.stride();
  int bpp = t.bpp();
  if (bpp >= 4)
    s->stride /= 4;
  else if (bpp == 2)
    s->stride /= 2;
  else
    assert(bpp == 1);
  // Use uint32_t* for easier sampling, but need to cast to uint8_t* or
  // uint16_t* for formats with bpp < 4.
  s->buf = (uint32_t*)t.buf;
  s->format = gl_format_to_texture_format(t.internal_format);
}

template <typename S>
static inline void null_sampler(S* s) {
  // For null texture data, just make the sampler provide a 1x1 buffer that is
  // transparent black. Ensure buffer holds at least a SIMD vector of zero data
  // for SIMD padding of unaligned loads.
  static const uint32_t zeroBuf[sizeof(Float) / sizeof(uint32_t)] = {0};
  s->width = 1;
  s->height = 1;
  s->stride = s->width;
  s->buf = (uint32_t*)zeroBuf;
  s->format = TextureFormat::RGBA8;
}

template <typename S>
static inline void null_filter(S* s) {
  s->filter = TextureFilter::NEAREST;
}

template <typename S>
static inline void null_depth(S* s) {
  s->depth = 1;
  s->height_stride = s->stride;
}

template <typename S>
S* lookup_sampler(S* s, int texture) {
  Texture& t = ctx->get_texture(s, texture);
  if (!t.buf) {
    null_sampler(s);
    null_filter(s);
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
    null_sampler(s);
  } else {
    init_sampler(s, t);
  }
  return s;
}

template <typename S>
S* lookup_sampler_array(S* s, int texture) {
  Texture& t = ctx->get_texture(s, texture);
  if (!t.buf) {
    null_sampler(s);
    null_depth(s);
    null_filter(s);
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

template <typename S, typename C>
static inline S expand_attrib(const char* buf, size_t size, bool normalized) {
  typedef typename ElementType<S>::ty elem_type;
  S scalar = {0};
  const C* src = reinterpret_cast<const C*>(buf);
  if (normalized) {
    const float scale = 1.0f / ((1 << (8 * sizeof(C))) - 1);
    for (size_t i = 0; i < size / sizeof(C); i++) {
      put_nth_component(scalar, i, elem_type(src[i]) * scale);
    }
  } else {
    for (size_t i = 0; i < size / sizeof(C); i++) {
      put_nth_component(scalar, i, elem_type(src[i]));
    }
  }
  return scalar;
}

template <typename S>
static inline S load_attrib_scalar(VertexAttrib& va, const char* src) {
  if (sizeof(S) <= va.size) {
    return *reinterpret_cast<const S*>(src);
  }
  if (va.type == GL_UNSIGNED_SHORT) {
    return expand_attrib<S, uint16_t>(src, va.size, va.normalized);
  }
  if (va.type == GL_UNSIGNED_BYTE) {
    return expand_attrib<S, uint8_t>(src, va.size, va.normalized);
  }
  assert(sizeof(typename ElementType<S>::ty) == bytes_per_type(va.type));
  S scalar = {0};
  memcpy(&scalar, src, va.size);
  return scalar;
}

template <typename T>
void load_attrib(T& attrib, VertexAttrib& va, uint32_t start, int instance,
                 int count) {
  typedef decltype(force_scalar(attrib)) scalar_type;
  if (!va.enabled) {
    attrib = T(scalar_type{0});
  } else if (va.divisor != 0) {
    char* src = (char*)va.buf + va.stride * instance + va.offset;
    assert(src + va.size <= va.buf + va.buf_size);
    attrib = T(load_attrib_scalar<scalar_type>(va, src));
  } else {
    // Specialized for WR's primitive vertex order/winding.
    if (!count) return;
    assert(count >= 2 && count <= 4);
    char* src = (char*)va.buf + va.stride * start + va.offset;
    switch (count) {
      case 2: {
        // Lines must be indexed at offsets 0, 1.
        // Line vertexes fill vertex shader SIMD lanes as 0, 1, 1, 0.
        scalar_type lanes[2] = {
            load_attrib_scalar<scalar_type>(va, src),
            load_attrib_scalar<scalar_type>(va, src + va.stride)};
        attrib = (T){lanes[0], lanes[1], lanes[1], lanes[0]};
        break;
      }
      case 3: {
        // Triangles must be indexed at offsets 0, 1, 2.
        // Triangle vertexes fill vertex shader SIMD lanes as 0, 1, 2, 2.
        scalar_type lanes[3] = {
            load_attrib_scalar<scalar_type>(va, src),
            load_attrib_scalar<scalar_type>(va, src + va.stride),
            load_attrib_scalar<scalar_type>(va, src + va.stride * 2)};
        attrib = (T){lanes[0], lanes[1], lanes[2], lanes[2]};
        break;
      }
      default:
        // Quads must be successive triangles indexed at offsets 0, 1, 2, 2,
        // 1, 3. Quad vertexes fill vertex shader SIMD lanes as 0, 1, 3, 2, so
        // that the points form a convex path that can be traversed by the
        // rasterizer.
        attrib = (T){load_attrib_scalar<scalar_type>(va, src),
                     load_attrib_scalar<scalar_type>(va, src + va.stride),
                     load_attrib_scalar<scalar_type>(va, src + va.stride * 3),
                     load_attrib_scalar<scalar_type>(va, src + va.stride * 2)};
        break;
    }
  }
}

template <typename T>
void load_flat_attrib(T& attrib, VertexAttrib& va, uint32_t start, int instance,
                      int count) {
  typedef decltype(force_scalar(attrib)) scalar_type;
  if (!va.enabled) {
    attrib = T{0};
    return;
  }
  char* src = nullptr;
  if (va.divisor != 0) {
    src = (char*)va.buf + va.stride * instance + va.offset;
  } else {
    if (!count) return;
    src = (char*)va.buf + va.stride * start + va.offset;
  }
  assert(src + va.size <= va.buf + va.buf_size);
  attrib = T(load_attrib_scalar<scalar_type>(va, src));
}

void setup_program(GLuint program) {
  if (!program) {
    vertex_shader = nullptr;
    fragment_shader = nullptr;
    return;
  }
  Program& p = ctx->programs[program];
  assert(p.impl);
  assert(p.vert_impl);
  assert(p.frag_impl);
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
  ctx->viewport = IntRect{x, y, x + width, y + height};
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
    "GL_APPLE_rgb_422",
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
  I32 c = round_pixel((Float){b, g, r, a});
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
  ctx->scissor = IntRect{x, y, x + width, y + height};
}

void ClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
  I32 c = round_pixel((Float){b, g, r, a});
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
  if (!p.impl) {
    return;
  }
  assert(p.impl->interpolants_size() <= sizeof(Interpolants));
  if (!p.vert_impl) p.vert_impl = p.impl->get_vertex_shader();
  if (!p.frag_impl) p.frag_impl = p.impl->get_fragment_shader();
}

GLint GetLinkStatus(GLuint program) {
  if (auto* p = ctx->programs.find(program)) {
    return p->impl ? 1 : 0;
  }
  return 0;
}

void BindAttribLocation(GLuint program, GLuint index, char* name) {
  Program& p = ctx->programs[program];
  assert(p.impl);
  if (!p.impl) {
    return;
  }
  p.impl->bind_attrib(name, index);
}

GLint GetAttribLocation(GLuint program, char* name) {
  Program& p = ctx->programs[program];
  assert(p.impl);
  if (!p.impl) {
    return -1;
  }
  return p.impl->get_attrib(name);
}

GLint GetUniformLocation(GLuint program, char* name) {
  Program& p = ctx->programs[program];
  assert(p.impl);
  if (!p.impl) {
    return -1;
  }
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
    case GL_RG:
      return GL_RG8;
    case GL_RGB_422_APPLE:
      return GL_RGB_RAW_422_APPLE;
    default:
      return format;
  }
}

void TexStorage3D(GLenum target, GLint levels, GLenum internal_format,
                  GLsizei width, GLsizei height, GLsizei depth) {
  assert(levels == 1);
  Texture& t = ctx->textures[ctx->get_binding(target)];
  internal_format = remap_internal_format(internal_format);
  bool changed = false;
  if (t.width != width || t.height != height || t.depth != depth ||
      t.internal_format != internal_format) {
    changed = true;
    t.internal_format = internal_format;
    t.width = width;
    t.height = height;
    t.depth = depth;
  }
  t.disable_delayed_clear();
  t.allocate(changed);
}

}  // extern "C"

static bool format_requires_conversion(GLenum external_format,
                                       GLenum internal_format) {
  switch (external_format) {
    case GL_RGBA:
      return internal_format == GL_RGBA8;
    default:
      return false;
  }
}

static inline void copy_bgra8_to_rgba8(uint32_t* dest, const uint32_t* src,
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

static void convert_copy(GLenum external_format, GLenum internal_format,
                         uint8_t* dst_buf, size_t dst_stride,
                         const uint8_t* src_buf, size_t src_stride,
                         size_t width, size_t height) {
  switch (external_format) {
    case GL_RGBA:
      if (internal_format == GL_RGBA8) {
        for (; height; height--) {
          copy_bgra8_to_rgba8((uint32_t*)dst_buf, (const uint32_t*)src_buf,
                              width);
          dst_buf += dst_stride;
          src_buf += src_stride;
        }
        return;
      }
      break;
    default:
      break;
  }
  size_t row_bytes = width * bytes_for_internal_format(internal_format);
  for (; height; height--) {
    memcpy(dst_buf, src_buf, row_bytes);
    dst_buf += dst_stride;
    src_buf += src_stride;
  }
}

static void set_tex_storage(Texture& t, GLenum external_format, GLsizei width,
                            GLsizei height, void* buf = nullptr,
                            GLsizei stride = 0, GLsizei min_width = 0,
                            GLsizei min_height = 0) {
  GLenum internal_format = remap_internal_format(external_format);
  bool changed = false;
  if (t.width != width || t.height != height || t.depth != 0 ||
      t.internal_format != internal_format) {
    changed = true;
    t.internal_format = internal_format;
    t.width = width;
    t.height = height;
    t.depth = 0;
  }
  // If we are changed from an internally managed buffer to an externally
  // supplied one or vice versa, ensure that we clean up old buffer state.
  // However, if we have to convert the data from a non-native format, then
  // always treat it as internally managed since we will need to copy to an
  // internally managed native format buffer.
  bool should_free = buf == nullptr || format_requires_conversion(
                                           external_format, internal_format);
  if (t.should_free() != should_free) {
    changed = true;
    t.cleanup();
    t.set_should_free(should_free);
  }
  // If now an external buffer, explicitly set it...
  if (!should_free) {
    t.set_buffer(buf, stride);
  }
  t.disable_delayed_clear();
  t.allocate(changed, min_width, min_height);
  // If we have a buffer that needs format conversion, then do that now.
  if (buf && should_free) {
    convert_copy(external_format, internal_format, (uint8_t*)t.buf, t.stride(),
                 (const uint8_t*)buf, stride, width, height);
  }
}

extern "C" {

void TexStorage2D(GLenum target, GLint levels, GLenum internal_format,
                  GLsizei width, GLsizei height) {
  assert(levels == 1);
  Texture& t = ctx->textures[ctx->get_binding(target)];
  set_tex_storage(t, internal_format, width, height);
}

GLenum internal_format_for_data(GLenum format, GLenum ty) {
  if (format == GL_RED && ty == GL_UNSIGNED_BYTE) {
    return GL_R8;
  } else if ((format == GL_RGBA || format == GL_BGRA) &&
             (ty == GL_UNSIGNED_BYTE || ty == GL_UNSIGNED_INT_8_8_8_8_REV)) {
    return GL_RGBA8;
  } else if (format == GL_RGBA && ty == GL_FLOAT) {
    return GL_RGBA32F;
  } else if (format == GL_RGBA_INTEGER && ty == GL_INT) {
    return GL_RGBA32I;
  } else if (format == GL_RG && ty == GL_UNSIGNED_BYTE) {
    return GL_RG8;
  } else if (format == GL_RGB_422_APPLE &&
             ty == GL_UNSIGNED_SHORT_8_8_REV_APPLE) {
    return GL_RGB_RAW_422_APPLE;
  } else if (format == GL_RED && ty == GL_UNSIGNED_SHORT) {
    return GL_R16;
  } else {
    debugf("unknown internal format for format %x, type %x\n", format, ty);
    assert(false);
    return 0;
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
  if (level != 0) {
    assert(false);
    return;
  }
  data = get_pixel_unpack_buffer_data(data);
  if (!data) return;
  Texture& t = ctx->textures[ctx->get_binding(target)];
  IntRect skip = {xoffset, yoffset, xoffset + width, yoffset + height};
  prepare_texture(t, &skip);
  assert(xoffset + width <= t.width);
  assert(yoffset + height <= t.height);
  assert(ctx->unpack_row_length == 0 || ctx->unpack_row_length >= width);
  GLsizei row_length =
      ctx->unpack_row_length != 0 ? ctx->unpack_row_length : width;
  assert(t.internal_format == internal_format_for_data(format, ty));
  int src_bpp = format_requires_conversion(format, t.internal_format)
                    ? bytes_for_internal_format(format)
                    : t.bpp();
  if (!src_bpp || !t.buf) return;
  convert_copy(format, t.internal_format,
               (uint8_t*)t.sample_ptr(xoffset, yoffset), t.stride(),
               (const uint8_t*)data, row_length * src_bpp, width, height);
}

void TexImage2D(GLenum target, GLint level, GLint internal_format,
                GLsizei width, GLsizei height, GLint border, GLenum format,
                GLenum ty, void* data) {
  if (level != 0) {
    assert(false);
    return;
  }
  assert(border == 0);
  TexStorage2D(target, 1, internal_format, width, height);
  TexSubImage2D(target, 0, 0, 0, width, height, format, ty, data);
}

void TexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                   GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                   GLenum format, GLenum ty, void* data) {
  if (level != 0) {
    assert(false);
    return;
  }
  data = get_pixel_unpack_buffer_data(data);
  if (!data) return;
  Texture& t = ctx->textures[ctx->get_binding(target)];
  prepare_texture(t);
  assert(ctx->unpack_row_length == 0 || ctx->unpack_row_length >= width);
  GLsizei row_length =
      ctx->unpack_row_length != 0 ? ctx->unpack_row_length : width;
  assert(t.internal_format == internal_format_for_data(format, ty));
  int src_bpp = format_requires_conversion(format, t.internal_format)
                    ? bytes_for_internal_format(format)
                    : t.bpp();
  if (!src_bpp || !t.buf) return;
  const uint8_t* src = (const uint8_t*)data;
  assert(xoffset + width <= t.width);
  assert(yoffset + height <= t.height);
  assert(zoffset + depth <= t.depth);
  size_t dest_stride = t.stride();
  size_t src_stride = row_length * src_bpp;
  for (int z = 0; z < depth; z++) {
    convert_copy(format, t.internal_format,
                 (uint8_t*)t.sample_ptr(xoffset, yoffset, zoffset + z),
                 dest_stride, src, src_stride, width, height);
    src += src_stride * height;
  }
}

void TexImage3D(GLenum target, GLint level, GLint internal_format,
                GLsizei width, GLsizei height, GLsizei depth, GLint border,
                GLenum format, GLenum ty, void* data) {
  if (level != 0) {
    assert(false);
    return;
  }
  assert(border == 0);
  TexStorage3D(target, 1, internal_format, width, height, depth);
  TexSubImage3D(target, 0, 0, 0, 0, width, height, depth, format, ty, data);
}

void GenerateMipmap(UNUSED GLenum target) {
  // TODO: support mipmaps
}

void SetTextureParameter(GLuint texid, GLenum pname, GLint param) {
  Texture& t = ctx->textures[texid];
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

void TexParameteri(GLenum target, GLenum pname, GLint param) {
  SetTextureParameter(ctx->get_binding(target), pname, param);
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
  set_tex_storage(ctx->textures[r.texture], internal_format, width, height);
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
  // Only support divisor being 0 (per-vertex) or 1 (per-instance).
  if (index >= NULL_ATTRIB || divisor > 1) {
    assert(0);
    return;
  }
  VertexAttrib& va = v.attribs[index];
  va.divisor = divisor;
}

void BufferData(GLenum target, GLsizeiptr size, void* data,
                UNUSED GLenum usage) {
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

void* MapBuffer(GLenum target, UNUSED GLbitfield access) {
  Buffer& b = ctx->buffers[ctx->get_binding(target)];
  return b.buf;
}

void* MapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                     UNUSED GLbitfield access) {
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
  vertex_shader->set_uniform_1i(location, V0);
}
void Uniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  assert(count == 1);
  vertex_shader->set_uniform_4fv(location, v);
}
void UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                      const GLfloat* value) {
  assert(count == 1);
  assert(!transpose);
  vertex_shader->set_uniform_matrix4fv(location, value);
}

void FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
                          GLuint texture, GLint level) {
  assert(target == GL_READ_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER);
  assert(textarget == GL_TEXTURE_2D || textarget == GL_TEXTURE_RECTANGLE);
  assert(level == 0);
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
  assert(target == GL_READ_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER);
  assert(level == 0);
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

static inline uint32_t clear_chunk(uint8_t value) {
  return uint32_t(value) * 0x01010101U;
}

static inline uint32_t clear_chunk(uint16_t value) {
  return uint32_t(value) | (uint32_t(value) << 16);
}

static inline uint32_t clear_chunk(uint32_t value) { return value; }

template <typename T>
static inline void clear_row(T* buf, size_t len, T value, uint32_t chunk) {
  const size_t N = sizeof(uint32_t) / sizeof(T);
  // fill any leading unaligned values
  if (N > 1) {
    size_t align = (-(intptr_t)buf & (sizeof(uint32_t) - 1)) / sizeof(T);
    if (align <= len) {
      fill_n(buf, align, value);
      len -= align;
      buf += align;
    }
  }
  // fill as many aligned chunks as possible
  fill_n((uint32_t*)buf, len / N, chunk);
  // fill any remaining values
  if (N > 1) {
    fill_n(buf + (len & ~(N - 1)), len & (N - 1), value);
  }
}

template <typename T>
static void clear_buffer(Texture& t, T value, int layer, IntRect bb,
                         int skip_start = 0, int skip_end = 0) {
  if (!t.buf) return;
  skip_start = max(skip_start, bb.x0);
  skip_end = max(skip_end, skip_start);
  assert(sizeof(T) == t.bpp());
  size_t stride = t.stride();
  // When clearing multiple full-width rows, collapse them into a single
  // large "row" to avoid redundant setup from clearing each row individually.
  if (bb.width() == t.width && bb.height() > 1 && skip_start >= skip_end) {
    bb.x1 += (stride / sizeof(T)) * (bb.height() - 1);
    bb.y1 = bb.y0 + 1;
  }
  T* buf = (T*)t.sample_ptr(bb.x0, bb.y0, layer);
  uint32_t chunk = clear_chunk(value);
  for (int rows = bb.height(); rows > 0; rows--) {
    if (bb.x0 < skip_start) {
      clear_row(buf, skip_start - bb.x0, value, chunk);
    }
    if (skip_end < bb.x1) {
      clear_row(buf + (skip_end - bb.x0), bb.x1 - skip_end, value, chunk);
    }
    buf += stride / sizeof(T);
  }
}

template <typename T>
static inline void clear_buffer(Texture& t, T value, int layer = 0) {
  IntRect bb = ctx->apply_scissor(t.bounds());
  if (bb.width() > 0) {
    clear_buffer<T>(t, value, layer, bb);
  }
}

template <typename T>
static inline void force_clear_row(Texture& t, int y, int skip_start = 0,
                                   int skip_end = 0) {
  assert(t.buf != nullptr);
  assert(sizeof(T) == t.bpp());
  assert(skip_start <= skip_end);
  T* buf = (T*)t.sample_ptr(0, y);
  uint32_t chunk = clear_chunk((T)t.clear_val);
  if (skip_start > 0) {
    clear_row<T>(buf, skip_start, t.clear_val, chunk);
  }
  if (skip_end < t.width) {
    clear_row<T>(buf + skip_end, t.width - skip_end, t.clear_val, chunk);
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
    y0 = clamp(skip->y0, 0, t.height);
    y1 = clamp(skip->y1, y0, t.height);
    skip_start = clamp(skip->x0, 0, t.width);
    skip_end = clamp(skip->x1, skip_start, t.width);
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
          clear_buffer<T>(t, t.clear_val, 0,
                          IntRect{0, start, t.width, start + count}, skip_start,
                          skip_end);
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
        clear_buffer<T>(t, t.clear_val, 0,
                        IntRect{0, start, t.width, start + count}, skip_start,
                        skip_end);
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
      case GL_RG8:
        force_clear<uint16_t>(t, skip);
        break;
      default:
        assert(false);
        break;
    }
  }
}

static inline bool clear_requires_scissor(Texture& t) {
  return ctx->scissortest && !ctx->scissor.contains(t.bounds());
}

// Setup a clear on a texture. This may either force an immediate clear or
// potentially punt to a delayed clear, if applicable.
template <typename T>
static void request_clear(Texture& t, int layer, T value) {
  // If the clear would require a scissor, force clear anything outside
  // the scissor, and then immediately clear anything inside the scissor.
  if (clear_requires_scissor(t)) {
    force_clear<T>(t, &ctx->scissor);
    clear_buffer<T>(t, value, layer);
  } else if (t.depth > 1) {
    // Delayed clear is not supported on texture arrays.
    t.disable_delayed_clear();
    clear_buffer<T>(t, value, layer);
  } else {
    // Do delayed clear for 2D texture without scissor.
    t.enable_delayed_clear(value);
  }
}

// Initialize a depth texture by setting the first run in each row to encompass
// the entire row.
void Texture::init_depth_runs(uint16_t depth) {
  if (!buf) return;
  DepthRun* runs = (DepthRun*)buf;
  for (int y = 0; y < height; y++) {
    runs[0] = DepthRun(depth, width);
    runs += stride() / sizeof(DepthRun);
  }
  set_cleared(true);
}

// Fill a portion of the run array with flattened depth samples.
static ALWAYS_INLINE void fill_depth_run(DepthRun* dst, size_t n,
                                         uint16_t depth) {
  fill_n((uint32_t*)dst, n, uint32_t(depth));
}

// Fills a scissored region of a depth texture with a given depth.
void Texture::fill_depth_runs(uint16_t depth) {
  if (!buf) return;
  assert(cleared());
  IntRect bb = ctx->apply_scissor(bounds());
  DepthRun* runs = (DepthRun*)sample_ptr(0, bb.y0);
  for (int rows = bb.height(); rows > 0; rows--) {
    if (bb.width() >= width) {
      // If the scissor region encompasses the entire row, reset the row to a
      // single run encompassing the entire row.
      runs[0] = DepthRun(depth, width);
    } else if (runs->is_flat()) {
      // If the row is flattened, just directly fill the portion of the row.
      fill_depth_run(&runs[bb.x0], bb.width(), depth);
    } else {
      // Otherwise, if we are still using runs, then set up a cursor to fill
      // it with depth runs.
      DepthCursor(runs, width, bb.x0, bb.width()).fill(depth);
    }
    runs += stride() / sizeof(DepthRun);
  }
}

extern "C" {

void InitDefaultFramebuffer(int width, int height, int stride, void* buf) {
  Framebuffer& fb = ctx->framebuffers[0];
  if (!fb.color_attachment) {
    GenTextures(1, &fb.color_attachment);
    fb.layer = 0;
  }
  // If the dimensions or buffer properties changed, we need to reallocate
  // the underlying storage for the color buffer texture.
  Texture& colortex = ctx->textures[fb.color_attachment];
  set_tex_storage(colortex, GL_RGBA8, width, height, buf, stride);
  if (!fb.depth_attachment) {
    GenTextures(1, &fb.depth_attachment);
  }
  // Ensure dimensions of the depth buffer match the color buffer.
  Texture& depthtex = ctx->textures[fb.depth_attachment];
  set_tex_storage(depthtex, GL_DEPTH_COMPONENT16, width, height);
}

void* GetColorBuffer(GLuint fbo, GLboolean flush, int32_t* width,
                     int32_t* height, int32_t* stride) {
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
  *stride = colortex.stride();
  return colortex.buf ? colortex.sample_ptr(0, 0, fb->layer) : nullptr;
}

void SetTextureBuffer(GLuint texid, GLenum internal_format, GLsizei width,
                      GLsizei height, GLsizei stride, void* buf,
                      GLsizei min_width, GLsizei min_height) {
  Texture& t = ctx->textures[texid];
  set_tex_storage(t, internal_format, width, height, buf, stride, min_width,
                  min_height);
}

GLenum CheckFramebufferStatus(GLenum target) {
  Framebuffer* fb = get_framebuffer(target);
  if (!fb || !fb->color_attachment) {
    return GL_FRAMEBUFFER_UNSUPPORTED;
  }
  return GL_FRAMEBUFFER_COMPLETE;
}

void Clear(GLbitfield mask) {
  Framebuffer& fb = *get_framebuffer(GL_DRAW_FRAMEBUFFER);
  if ((mask & GL_COLOR_BUFFER_BIT) && fb.color_attachment) {
    Texture& t = ctx->textures[fb.color_attachment];
    assert(!t.locked);
    if (t.internal_format == GL_RGBA8) {
      uint32_t color = ctx->clearcolor;
      request_clear<uint32_t>(t, fb.layer, color);
    } else if (t.internal_format == GL_R8) {
      uint8_t color = uint8_t((ctx->clearcolor >> 16) & 0xFF);
      request_clear<uint8_t>(t, fb.layer, color);
    } else if (t.internal_format == GL_RG8) {
      uint16_t color = uint16_t((ctx->clearcolor & 0xFF00) |
                                ((ctx->clearcolor >> 16) & 0xFF));
      request_clear<uint16_t>(t, fb.layer, color);
    } else {
      assert(false);
    }
  }
  if ((mask & GL_DEPTH_BUFFER_BIT) && fb.depth_attachment) {
    Texture& t = ctx->textures[fb.depth_attachment];
    assert(t.internal_format == GL_DEPTH_COMPONENT16);
    uint16_t depth = uint16_t(0xFFFF * ctx->cleardepth);
    if (t.cleared() && clear_requires_scissor(t)) {
      // If we need to scissor the clear and the depth buffer was already
      // initialized, then just fill runs for that scissor area.
      t.fill_depth_runs(depth);
    } else {
      // Otherwise, the buffer is either uninitialized or the clear would
      // encompass the entire buffer. If uninitialized, we can safely fill
      // the entire buffer with any value and thus ignore any scissoring.
      t.init_depth_runs(depth);
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
        t.set_cleared(false);
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
         format == GL_BGRA || format == GL_RG);
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
  // Only support readback conversions that are reversible
  assert(!format_requires_conversion(format, t.internal_format) ||
         bytes_for_internal_format(format) == t.bpp());
  convert_copy(format, t.internal_format, (uint8_t*)data, width * t.bpp(),
               (const uint8_t*)t.sample_ptr(x, y, fb->layer), t.stride(), width,
               height);
}

void CopyImageSubData(GLuint srcName, GLenum srcTarget, UNUSED GLint srcLevel,
                      GLint srcX, GLint srcY, GLint srcZ, GLuint dstName,
                      GLenum dstTarget, UNUSED GLint dstLevel, GLint dstX,
                      GLint dstY, GLint dstZ, GLsizei srcWidth,
                      GLsizei srcHeight, GLsizei srcDepth) {
  assert(srcLevel == 0 && dstLevel == 0);
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
  assert(!dsttex.locked);
  IntRect skip = {dstX, dstY, dstX + srcWidth, dstY + srcHeight};
  prepare_texture(dsttex, &skip);
  assert(srctex.internal_format == dsttex.internal_format);
  assert(srcWidth >= 0);
  assert(srcHeight >= 0);
  assert(srcDepth >= 0);
  assert(srcX + srcWidth <= srctex.width);
  assert(srcY + srcHeight <= srctex.height);
  assert(srcZ + srcDepth <= max(srctex.depth, 1));
  assert(dstX + srcWidth <= dsttex.width);
  assert(dstY + srcHeight <= dsttex.height);
  assert(dstZ + srcDepth <= max(dsttex.depth, 1));
  int bpp = srctex.bpp();
  int src_stride = srctex.stride();
  int dest_stride = dsttex.stride();
  for (int z = 0; z < srcDepth; z++) {
    char* dest = dsttex.sample_ptr(dstX, dstY, dstZ + z);
    char* src = srctex.sample_ptr(srcX, srcY, srcZ + z);
    for (int y = 0; y < srcHeight; y++) {
      memcpy(dest, src, srcWidth * bpp);
      dest += dest_stride;
      src += src_stride;
    }
  }
}

void CopyTexSubImage3D(GLenum target, UNUSED GLint level, GLint xoffset,
                       GLint yoffset, GLint zoffset, GLint x, GLint y,
                       GLsizei width, GLsizei height) {
  assert(level == 0);
  Framebuffer* fb = get_framebuffer(GL_READ_FRAMEBUFFER);
  if (!fb) return;
  CopyImageSubData(fb->color_attachment, GL_TEXTURE_3D, 0, x, y, fb->layer,
                   ctx->get_binding(target), GL_TEXTURE_3D, 0, xoffset, yoffset,
                   zoffset, width, height, 1);
}

void CopyTexSubImage2D(GLenum target, UNUSED GLint level, GLint xoffset,
                       GLint yoffset, GLint x, GLint y, GLsizei width,
                       GLsizei height) {
  assert(level == 0);
  Framebuffer* fb = get_framebuffer(GL_READ_FRAMEBUFFER);
  if (!fb) return;
  CopyImageSubData(fb->color_attachment, GL_TEXTURE_2D_ARRAY, 0, x, y,
                   fb->layer, ctx->get_binding(target), GL_TEXTURE_2D_ARRAY, 0,
                   xoffset, yoffset, 0, width, height, 1);
}

}  // extern "C"

using ZMask = I32;

static inline PackedRGBA8 convert_zmask(ZMask mask, uint32_t*) {
  return bit_cast<PackedRGBA8>(mask);
}

static inline WideR8 convert_zmask(ZMask mask, uint8_t*) {
  return CONVERT(mask, WideR8);
}

#if USE_SSE2
#  define ZMASK_NONE_PASSED 0xFFFF
#  define ZMASK_ALL_PASSED 0
static inline uint32_t zmask_code(ZMask mask) {
  return _mm_movemask_epi8(mask);
}
#else
#  define ZMASK_NONE_PASSED 0xFFFFFFFFU
#  define ZMASK_ALL_PASSED 0
static inline uint32_t zmask_code(ZMask mask) {
  return bit_cast<uint32_t>(CONVERT(mask, U8));
}
#endif

// Interprets items in the depth buffer as sign-extended 32-bit depth values
// instead of as runs. Returns a mask that signals which samples in the given
// chunk passed or failed the depth test with given Z value.
template <bool DISCARD, typename Z>
static ALWAYS_INLINE bool check_depth(Z z, DepthRun* zbuf, ZMask& outmask,
                                      int span = 4) {
  // SSE2 does not support unsigned comparison. So ensure Z value is
  // sign-extended to int32_t.
  I32 src = I32(z);
  I32 dest = unaligned_load<I32>(zbuf);
  // Invert the depth test to check which pixels failed and should be discarded.
  ZMask mask = ctx->depthfunc == GL_LEQUAL
                   ?
                   // GL_LEQUAL: Not(LessEqual) = Greater
                   ZMask(src > dest)
                   :
                   // GL_LESS: Not(Less) = GreaterEqual
                   ZMask(src >= dest);
  // Mask off any unused lanes in the span.
  mask |= ZMask(span) < ZMask{1, 2, 3, 4};
  if (zmask_code(mask) == ZMASK_NONE_PASSED) {
    return false;
  }
  if (!DISCARD && ctx->depthmask) {
    unaligned_store(zbuf, (mask & dest) | (~mask & src));
  }
  outmask = mask;
  return true;
}

static ALWAYS_INLINE I32 packDepth() {
  return cast(fragment_shader->gl_FragCoord.z * 0xFFFF);
}

template <typename Z>
static ALWAYS_INLINE void discard_depth(Z z, DepthRun* zbuf, I32 mask) {
  if (ctx->depthmask) {
    I32 src = I32(z);
    I32 dest = unaligned_load<I32>(zbuf);
    mask |= fragment_shader->swgl_IsPixelDiscarded;
    unaligned_store(zbuf, (mask & dest) | (~mask & src));
  }
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

static inline WideRGBA8 pack_pixels_RGBA8(const vec4& v) {
  ivec4 i = round_pixel(v);
  HalfRGBA8 xz = packRGBA8(i.z, i.x);
  HalfRGBA8 yw = packRGBA8(i.y, i.w);
  HalfRGBA8 xyzwl = zipLow(xz, yw);
  HalfRGBA8 xyzwh = zipHigh(xz, yw);
  HalfRGBA8 lo = zip2Low(xyzwl, xyzwh);
  HalfRGBA8 hi = zip2High(xyzwl, xyzwh);
  return combine(lo, hi);
}

UNUSED static inline WideRGBA8 pack_pixels_RGBA8(const vec4_scalar& v) {
  I32 i = round_pixel((Float){v.z, v.y, v.x, v.w});
  HalfRGBA8 c = packRGBA8(i, i);
  return combine(c, c);
}

static inline WideRGBA8 pack_pixels_RGBA8() {
  return pack_pixels_RGBA8(fragment_shader->gl_FragColor);
}

// (x*y + x) >> 8, cheap approximation of (x*y) / 255
template <typename T>
static inline T muldiv255(T x, T y) {
  return (x * y + x) >> 8;
}

// Byte-wise addition for when x or y is a signed 8-bit value stored in the
// low byte of a larger type T only with zeroed-out high bits, where T is
// greater than 8 bits, i.e. uint16_t. This can result when muldiv255 is used
// upon signed operands, using up all the precision in a 16 bit integer, and
// potentially losing the sign bit in the last >> 8 shift. Due to the
// properties of two's complement arithmetic, even though we've discarded the
// sign bit, we can still represent a negative number under addition (without
// requiring any extra sign bits), just that any negative number will behave
// like a large unsigned number under addition, generating a single carry bit
// on overflow that we need to discard. Thus, just doing a byte-wise add will
// overflow without the troublesome carry, giving us only the remaining 8 low
// bits we actually need while keeping the high bits at zero.
template <typename T>
static inline T addlow(T x, T y) {
  typedef VectorType<uint8_t, sizeof(T)> bytes;
  return bit_cast<T>(bit_cast<bytes>(x) + bit_cast<bytes>(y));
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
  const WideRGBA8 ALPHA_OPAQUE = {0, 0, 0, 255, 0, 0, 0, 255,
                                  0, 0, 0, 255, 0, 0, 0, 255};
  switch (blend_key) {
    case BLEND_KEY_NONE:
      return src;
    case BLEND_KEY(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                   GL_ONE_MINUS_SRC_ALPHA):
      // dst + src.a*(src.rgb1 - dst)
      // use addlow for signed overflow
      return addlow(dst, muldiv255(alphas(src), (src | ALPHA_OPAQUE) - dst));
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
      // src*(1-dst.a) + dst*1 = src - src*dst.a + dst
      return dst + ((src - muldiv255(src, alphas(dst))) & RGB_MASK);
    case BLEND_KEY(GL_CONSTANT_COLOR, GL_ONE_MINUS_SRC_COLOR):
      // src*k + (1-src)*dst = src*k + dst - src*dst = dst + src*(k - dst)
      // use addlow for signed overflow
      return addlow(
          dst, muldiv255(src, combine(ctx->blendcolor, ctx->blendcolor) - dst));
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
static inline void discard_output(uint32_t* buf, PackedRGBA8 mask) {
  PackedRGBA8 dst = unaligned_load<PackedRGBA8>(buf);
  WideRGBA8 r = pack_pixels_RGBA8();
  if (blend_key) r = blend_pixels_RGBA8(dst, r);
  if (DISCARD)
    mask |= bit_cast<PackedRGBA8>(fragment_shader->swgl_IsPixelDiscarded);
  unaligned_store(buf, (mask & dst) | (~mask & pack(r)));
}

template <bool DISCARD>
static inline void discard_output(uint32_t* buf) {
  discard_output<DISCARD>(buf, 0);
}

template <>
inline void discard_output<false>(uint32_t* buf) {
  WideRGBA8 r = pack_pixels_RGBA8();
  if (blend_key) r = blend_pixels_RGBA8(unaligned_load<PackedRGBA8>(buf), r);
  unaligned_store(buf, pack(r));
}

static inline PackedRGBA8 span_mask_RGBA8(int span) {
  return bit_cast<PackedRGBA8>(I32(span) < I32{1, 2, 3, 4});
}

static inline PackedRGBA8 span_mask(uint32_t*, int span) {
  return span_mask_RGBA8(span);
}

static inline WideR8 packR8(I32 a) {
#if USE_SSE2
  return lowHalf(bit_cast<V8<uint16_t>>(_mm_packs_epi32(a, a)));
#elif USE_NEON
  return vqmovun_s32(a);
#else
  return CONVERT(a, WideR8);
#endif
}

static inline WideR8 pack_pixels_R8(Float c) { return packR8(round_pixel(c)); }

static inline WideR8 pack_pixels_R8() {
  return pack_pixels_R8(fragment_shader->gl_FragColor.x);
}

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
static inline void discard_output(uint8_t* buf, WideR8 mask) {
  WideR8 dst = unpack(unaligned_load<PackedR8>(buf));
  WideR8 r = pack_pixels_R8();
  if (blend_key) r = blend_pixels_R8(dst, r);
  if (DISCARD) mask |= packR8(fragment_shader->swgl_IsPixelDiscarded);
  unaligned_store(buf, pack((mask & dst) | (~mask & r)));
}

template <bool DISCARD>
static inline void discard_output(uint8_t* buf) {
  discard_output<DISCARD>(buf, 0);
}

template <>
inline void discard_output<false>(uint8_t* buf) {
  WideR8 r = pack_pixels_R8();
  if (blend_key) r = blend_pixels_R8(unpack(unaligned_load<PackedR8>(buf)), r);
  unaligned_store(buf, pack(r));
}

static inline WideR8 span_mask_R8(int span) {
  return bit_cast<WideR8>(WideR8(span) < WideR8{1, 2, 3, 4});
}

static inline WideR8 span_mask(uint8_t*, int span) {
  return span_mask_R8(span);
}

UNUSED static inline PackedRG8 span_mask_RG8(int span) {
  return bit_cast<PackedRG8>(I16(span) < I16{1, 2, 3, 4});
}

template <bool DISCARD, bool W, typename P, typename M>
static inline void commit_output(P* buf, M mask) {
  fragment_shader->run<W>();
  discard_output<DISCARD>(buf, mask);
}

template <bool DISCARD, bool W, typename P>
static inline void commit_output(P* buf) {
  fragment_shader->run<W>();
  discard_output<DISCARD>(buf);
}

template <bool DISCARD, bool W, typename P>
static inline void commit_output(P* buf, int span) {
  commit_output<DISCARD, W>(buf, span_mask(buf, span));
}

template <bool DISCARD, bool W, typename P, typename Z>
static inline void commit_output(P* buf, Z z, DepthRun* zbuf) {
  ZMask zmask;
  if (check_depth<DISCARD>(z, zbuf, zmask)) {
    commit_output<DISCARD, W>(buf, convert_zmask(zmask, buf));
    if (DISCARD) {
      discard_depth(z, zbuf, zmask);
    }
  } else {
    fragment_shader->skip<W>();
  }
}

template <bool DISCARD, bool W, typename P, typename Z>
static inline void commit_output(P* buf, Z z, DepthRun* zbuf, int span) {
  ZMask zmask;
  if (check_depth<DISCARD>(z, zbuf, zmask, span)) {
    commit_output<DISCARD, W>(buf, convert_zmask(zmask, buf));
    if (DISCARD) {
      discard_depth(z, zbuf, zmask);
    }
  }
}

#include "swgl_ext.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#ifdef __clang__
#  pragma GCC diagnostic ignored "-Wunused-private-field"
#else
#  pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include "load_shader.h"
#pragma GCC diagnostic pop

typedef vec2_scalar Point2D;
typedef vec4_scalar Point3D;

struct ClipRect {
  float x0;
  float y0;
  float x1;
  float y1;

  ClipRect(const IntRect& i) : x0(i.x0), y0(i.y0), x1(i.x1), y1(i.y1) {}
  ClipRect(Texture& t) : ClipRect(ctx->apply_scissor(t.bounds())) {}

  template <typename P>
  bool overlaps(int nump, const P* p) const {
    // Generate a mask of which side of the clip rect all of a polygon's points
    // fall inside of. This is a cheap conservative estimate of whether the
    // bounding box of the polygon might overlap the clip rect, rather than an
    // exact test that would require multiple slower line intersections.
    int sides = 0;
    for (int i = 0; i < nump; i++) {
      sides |= p[i].x < x1 ? (p[i].x > x0 ? 1 | 2 : 1) : 2;
      sides |= p[i].y < y1 ? (p[i].y > y0 ? 4 | 8 : 4) : 8;
    }
    return sides == 0xF;
  }
};

// Converts a run array into a flattened array of depth samples. This just
// walks through every run and fills the samples with the depth value from
// the run.
static void flatten_depth_runs(DepthRun* runs, size_t width) {
  if (runs->is_flat()) {
    return;
  }
  while (width > 0) {
    size_t n = runs->count;
    fill_depth_run(runs, n, runs->depth);
    runs += n;
    width -= n;
  }
}

// Helper function for drawing passed depth runs within the depth buffer.
// Flattened depth (perspective or discard) is not supported.
template <typename P>
static ALWAYS_INLINE void draw_depth_span(uint16_t z, P* buf,
                                          DepthCursor& cursor) {
  for (;;) {
    // Get the span that passes the depth test. Assume on entry that
    // any failed runs have already been skipped.
    int span = cursor.check_passed(z, ctx->depthfunc, ctx->depthmask);
    // If nothing passed, since we already skipped passed failed runs
    // previously, we must have hit the end of the row. Bail out.
    if (span <= 0) {
      break;
    }
    if (span >= 4) {
      // If we have a draw specialization, try to process as many 4-pixel
      // chunks as possible using it.
      if (fragment_shader->has_draw_span(buf)) {
        int len = span & ~3;
        fragment_shader->draw_span(buf, len);
        buf += len;
        span &= 3;
      } else {
        // Otherwise, just process each chunk individually.
        while (span >= 4) {
          commit_output<false, false>(buf);
          buf += 4;
          span -= 4;
        }
      }
    }
    // If we have a partial chunk left over, we still have to process it as if
    // it were a full chunk. Mask off only the part of the chunk we want to
    // use.
    if (span > 0) {
      commit_output<false, false>(buf, span_mask(buf, span));
      buf += span;
    }
    // Skip past any runs that fail the depth test.
    int skip = cursor.skip_failed(z, ctx->depthfunc);
    // If there aren't any, that means we won't encounter any more passing runs
    // and so it's safe to bail out.
    if (skip <= 0) {
      break;
    }
    // Advance interpolants for the fragment shader past the skipped region.
    // If we processed a partial chunk above, we actually advanced the
    // interpolants a full chunk in the fragment shader's run function. Thus,
    // we need to first subtract off that 4-pixel chunk and only partially
    // advance them to that partial chunk before we can add on the rest of the
    // skips. This is combined with the skip here for efficiency's sake.
    fragment_shader->skip(skip - (span > 0 ? 4 - span : 0));
    buf += skip;
  }
}

// Draw a simple span in 4-pixel wide chunks, optionally using depth.
template <bool DISCARD, bool W, typename P, typename Z>
static ALWAYS_INLINE void draw_span(P* buf, DepthRun* depth, int span, Z z) {
  if (depth) {
    // Depth testing is enabled. If perspective is used, Z values will vary
    // across the span, we use packDepth to generate 16-bit Z values suitable
    // for depth testing based on current values from gl_FragCoord.z.
    // Otherwise, for the no-perspective case, we just use the provided Z.
    // Process 4-pixel chunks first.
    for (; span >= 4; span -= 4, buf += 4, depth += 4) {
      commit_output<DISCARD, W>(buf, z(), depth);
    }
    // If there are any remaining pixels, do a partial chunk.
    if (span > 0) {
      commit_output<DISCARD, W>(buf, z(), depth, span);
    }
  } else {
    // Process 4-pixel chunks first.
    for (; span >= 4; span -= 4, buf += 4) {
      commit_output<DISCARD, W>(buf);
    }
    // If there are any remaining pixels, do a partial chunk.
    if (span > 0) {
      commit_output<DISCARD, W>(buf, span);
    }
  }
}

// Called during rasterization to forcefully clear a row on which delayed clear
// has been enabled. If we know that we are going to completely overwrite a part
// of the row, then we only need to clear the row outside of that part. However,
// if blending or discard is enabled, the values of that underlying part of the
// row may be used regardless to produce the final rasterization result, so we
// have to then clear the entire underlying row to prepare it.
template <typename P>
static inline void prepare_row(Texture& colortex, int y, int startx, int endx,
                               bool use_discard, DepthRun* depth,
                               uint16_t z = 0, DepthCursor* cursor = nullptr) {
  assert(colortex.delay_clear > 0);
  // Delayed clear is enabled for the color buffer. Check if needs clear.
  uint32_t& mask = colortex.cleared_rows[y / 32];
  if ((mask & (1 << (y & 31))) == 0) {
    mask |= 1 << (y & 31);
    colortex.delay_clear--;
    if (blend_key || use_discard) {
      // If depth test, blending, or discard is used, old color values
      // might be sampled, so we need to clear the entire row to fill it.
      force_clear_row<P>(colortex, y);
    } else if (depth) {
      if (depth->is_flat() || !cursor) {
        // If flat depth is used, we can't cheaply predict if which samples will
        // pass.
        force_clear_row<P>(colortex, y);
      } else {
        // Otherwise if depth runs are used, see how many samples initially pass
        // the depth test and only fill the row outside those. The fragment
        // shader will fill the row within the passed samples.
        int passed =
            DepthCursor(*cursor).check_passed<false>(z, ctx->depthfunc);
        if (startx > 0 || startx + passed < colortex.width) {
          force_clear_row<P>(colortex, y, startx, startx + passed);
        }
      }
    } else if (startx > 0 || endx < colortex.width) {
      // Otherwise, we only need to clear the row outside of the span.
      // The fragment shader will fill the row within the span itself.
      force_clear_row<P>(colortex, y, startx, endx);
    }
  }
}

// Draw spans for each row of a given quad (or triangle) with a constant Z
// value. The quad is assumed convex. It is clipped to fall within the given
// clip rect. In short, this function rasterizes a quad by first finding a
// top most starting point and then from there tracing down the left and right
// sides of this quad until it hits the bottom, outputting a span between the
// current left and right positions at each row along the way. Points are
// assumed to be ordered in either CW or CCW to support this, but currently
// both orders (CW and CCW) are supported and equivalent.
template <typename P>
static inline void draw_quad_spans(int nump, Point2D p[4], uint16_t z,
                                   Interpolants interp_outs[4],
                                   Texture& colortex, int layer,
                                   Texture& depthtex,
                                   const ClipRect& clipRect) {
  // Only triangles and convex quads supported.
  assert(nump == 3 || nump == 4);
  Point2D l0, r0, l1, r1;
  int l0i, r0i, l1i, r1i;
  {
    // Find the index of the top-most (smallest Y) point from which
    // rasterization can start.
    int top = nump > 3 && p[3].y < p[2].y
                  ? (p[0].y < p[1].y ? (p[0].y < p[3].y ? 0 : 3)
                                     : (p[1].y < p[3].y ? 1 : 3))
                  : (p[0].y < p[1].y ? (p[0].y < p[2].y ? 0 : 2)
                                     : (p[1].y < p[2].y ? 1 : 2));
    // Helper to find next index in the points array, walking forward.
#define NEXT_POINT(idx)   \
  ({                      \
    int cur = (idx) + 1;  \
    cur < nump ? cur : 0; \
  })
    // Helper to find the previous index in the points array, walking backward.
#define PREV_POINT(idx)        \
  ({                           \
    int cur = (idx)-1;         \
    cur >= 0 ? cur : nump - 1; \
  })
    // Start looking for "left"-side and "right"-side descending edges starting
    // from the determined top point.
    int next = NEXT_POINT(top);
    int prev = PREV_POINT(top);
    if (p[top].y == p[next].y) {
      // If the next point is on the same row as the top, then advance one more
      // time to the next point and use that as the "left" descending edge.
      l0i = next;
      l1i = NEXT_POINT(next);
      // Assume top and prev form a descending "right" edge, as otherwise this
      // will be a collapsed polygon and harmlessly bail out down below.
      r0i = top;
      r1i = prev;
    } else if (p[top].y == p[prev].y) {
      // If the prev point is on the same row as the top, then advance to the
      // prev again and use that as the "right" descending edge.
      // Assume top and next form a non-empty descending "left" edge.
      l0i = top;
      l1i = next;
      r0i = prev;
      r1i = PREV_POINT(prev);
    } else {
      // Both next and prev are on distinct rows from top, so both "left" and
      // "right" edges are non-empty/descending.
      l0i = r0i = top;
      l1i = next;
      r1i = prev;
    }
    // Load the points from the indices.
    l0 = p[l0i];  // Start of left edge
    r0 = p[r0i];  // End of left edge
    l1 = p[l1i];  // Start of right edge
    r1 = p[r1i];  // End of right edge
    //    debugf("l0: %d(%f,%f), r0: %d(%f,%f) -> l1: %d(%f,%f), r1:
    //    %d(%f,%f)\n", l0i, l0.x, l0.y, r0i, r0.x, r0.y, l1i, l1.x, l1.y, r1i,
    //    r1.x, r1.y);
  }

  struct Edge {
    float yScale;
    float xSlope;
    float x;
    Interpolants interpSlope;
    Interpolants interp;

    Edge(float y, const Point2D& p0, const Point2D& p1, const Interpolants& i0,
         const Interpolants& i1)
        :  // Inverse Y scale for slope calculations. Avoid divide on 0-length
           // edge. Later checks below ensure that Y <= p1.y, or otherwise we
           // don't use this edge. We just need to guard against Y == p1.y ==
           // p0.y. In that case, Y - p0.y == 0 and will cancel out the slopes
           // below, except if yScale is Inf for some reason (or worse, NaN),
           // which 1/(p1.y-p0.y) might produce if we don't bound it.
          yScale(1.0f / max(p1.y - p0.y, 1.0f / 256)),
          // Calculate dX/dY slope
          xSlope((p1.x - p0.x) * yScale),
          // Initialize current X based on Y and slope
          x(p0.x + (y - p0.y) * xSlope),
          // Calculate change in interpolants per change in Y
          interpSlope((i1 - i0) * yScale),
          // Initialize current interpolants based on Y and slope
          interp(i0 + (y - p0.y) * interpSlope) {}

    void nextRow() {
      // step current X and interpolants to next row from slope
      x += xSlope;
      interp += interpSlope;
    }
  };

  // Vertex selection above should result in equal left and right start rows
  assert(l0.y == r0.y);
  // Find the start y, clip to within the clip rect, and round to row center.
  float y = floor(max(l0.y, clipRect.y0) + 0.5f) + 0.5f;
  // Initialize left and right edges from end points and start Y
  Edge left(y, l0, l1, interp_outs[l0i], interp_outs[l1i]);
  Edge right(y, r0, r1, interp_outs[r0i], interp_outs[r1i]);
  // Get pointer to color buffer and depth buffer at current Y
  P* fbuf = (P*)colortex.sample_ptr(0, int(y), layer);
  DepthRun* fdepth = (DepthRun*)depthtex.sample_ptr(0, int(y));
  // Loop along advancing Ys, rasterizing spans at each row
  float checkY = min(min(l1.y, r1.y), clipRect.y1);
  for (;;) {
    // Check if we maybe passed edge ends or outside clip rect...
    if (y > checkY) {
      // If we're outside the clip rect, we're done.
      if (y > clipRect.y1) break;
        // Helper to find the next non-duplicate vertex that doesn't loop back.
#define STEP_EDGE(e0i, e0, e1i, e1, STEP_POINT, end)               \
  for (;;) {                                                       \
    /* Set new start of edge to be end of old edge */              \
    e0i = e1i;                                                     \
    e0 = e1;                                                       \
    /* Set new end of edge to next point */                        \
    e1i = STEP_POINT(e1i);                                         \
    e1 = p[e1i];                                                   \
    /* If the edge is descending, use it. */                       \
    if (e1.y > e0.y) break;                                        \
    /* If the edge is ascending or crossed the end, we're done. */ \
    if (e1.y < e0.y || e0i == end) return;                         \
    /* Otherwise, it's a duplicate, so keep searching. */          \
  }
      // Check if Y advanced past the end of the left edge
      if (y > l1.y) {
        // Step to next left edge past Y and reset edge interpolants.
        do {
          STEP_EDGE(l0i, l0, l1i, l1, NEXT_POINT, r1i);
        } while (y > l1.y);
        left = Edge(y, l0, l1, interp_outs[l0i], interp_outs[l1i]);
      }
      // Check if Y advanced past the end of the right edge
      if (y > r1.y) {
        // Step to next right edge past Y and reset edge interpolants.
        do {
          STEP_EDGE(r0i, r0, r1i, r1, PREV_POINT, l1i);
        } while (y > r1.y);
        right = Edge(y, r0, r1, interp_outs[r0i], interp_outs[r1i]);
      }
      // Reset check condition for next time around.
      checkY = min(min(l1.y, r1.y), clipRect.y1);
    }
    // lx..rx form the bounds of the span. WR does not use backface culling,
    // so we need to use min/max to support the span in either orientation.
    // Clip the span to fall within the clip rect and then round to nearest
    // column.
    int startx = int(max(min(left.x, right.x), clipRect.x0) + 0.5f);
    int endx = int(min(max(left.x, right.x), clipRect.x1) + 0.5f);
    // Check if span is non-empty.
    int span = endx - startx;
    if (span > 0) {
      ctx->shaded_rows++;
      ctx->shaded_pixels += span;
      // Advance color/depth buffer pointers to the start of the span.
      P* buf = fbuf + startx;
      // Check if the we will need to use depth-buffer or discard on this span.
      DepthRun* depth =
          depthtex.buf != nullptr && depthtex.cleared() ? fdepth : nullptr;
      DepthCursor cursor;
      bool use_discard = fragment_shader->use_discard();
      if (use_discard) {
        if (depth) {
          // If we're using discard, we may have to unpredictably drop out some
          // samples. Flatten the depth run array here to allow this.
          if (!depth->is_flat()) {
            flatten_depth_runs(depth, depthtex.width);
          }
          // Advance to the depth sample at the start of the span.
          depth += startx;
        }
      } else if (depth) {
        if (!depth->is_flat()) {
          // We're not using discard and the depth row is still organized into
          // runs. Skip past any runs that would fail the depth test so we
          // don't have to do any extra work to process them with the rest of
          // the span.
          cursor = DepthCursor(depth, depthtex.width, startx, span);
          int skipped = cursor.skip_failed(z, ctx->depthfunc);
          // If we fell off the row, that means we couldn't find any passing
          // runs. We can just skip the entire span.
          if (skipped < 0) {
            goto next_span;
          }
          buf += skipped;
          startx += skipped;
          span -= skipped;
        } else {
          // The row is already flattened, so just advance to the span start.
          depth += startx;
        }
      }

      if (colortex.delay_clear) {
        // Delayed clear is enabled for the color buffer. Check if needs clear.
        prepare_row<P>(colortex, int(y), startx, endx, use_discard, depth, z,
                       &cursor);
      }

      // Initialize fragment shader interpolants to current span position.
      fragment_shader->gl_FragCoord.x = init_interp(startx + 0.5f, 1);
      fragment_shader->gl_FragCoord.y = y;
      {
        // Change in interpolants is difference between current right and left
        // edges per the change in right and left X.
        Interpolants step =
            (right.interp - left.interp) * (1.0f / (right.x - left.x));
        // Advance current interpolants to X at start of span.
        Interpolants o = left.interp + step * (startx + 0.5f - left.x);
        fragment_shader->init_span(&o, &step);
      }
      if (!use_discard) {
        // Fast paths for the case where fragment discard is not used.
        if (depth) {
          // If depth is used, we want to process entire depth runs if depth is
          // not flattened.
          if (!depth->is_flat()) {
            draw_depth_span(z, buf, cursor);
            goto next_span;
          }
          // Otherwise, flattened depth must fall back to the slightly slower
          // per-chunk depth test path in draw_span below.
        } else {
          // Check if the fragment shader has an optimized draw specialization.
          if (span >= 4 && fragment_shader->has_draw_span(buf)) {
            // Draw specialization expects 4-pixel chunks.
            int len = span & ~3;
            fragment_shader->draw_span(buf, len);
            buf += len;
            span &= 3;
          }
        }
        draw_span<false, false>(buf, depth, span, [=] { return z; });
      } else {
        // If discard is used, then use slower fallbacks. This should be rare.
        // Just needs to work, doesn't need to be too fast yet...
        draw_span<true, false>(buf, depth, span, [=] { return z; });
      }
    }
  next_span:
    // Advance Y and edge interpolants to next row.
    y++;
    left.nextRow();
    right.nextRow();
    // Advance buffers to next row.
    fbuf += colortex.stride() / sizeof(P);
    fdepth += depthtex.stride() / sizeof(DepthRun);
  }
}

// Draw perspective-correct spans for a convex quad that has been clipped to
// the near and far Z planes, possibly producing a clipped convex polygon with
// more than 4 sides. This assumes the Z value will vary across the spans and
// requires interpolants to factor in W values. This tends to be slower than
// the simpler 2D draw_quad_spans above, especially since we can't optimize the
// depth test easily when Z values, and should be used only rarely if possible.
template <typename P>
static inline void draw_perspective_spans(int nump, Point3D* p,
                                          Interpolants* interp_outs,
                                          Texture& colortex, int layer,
                                          Texture& depthtex,
                                          const ClipRect& clipRect) {
  Point3D l0, r0, l1, r1;
  int l0i, r0i, l1i, r1i;
  {
    // Find the index of the top-most point (smallest Y) from which
    // rasterization can start.
    int top = 0;
    for (int i = 1; i < nump; i++) {
      if (p[i].y < p[top].y) {
        top = i;
      }
    }
    // Find left-most top point, the start of the left descending edge.
    // Advance forward in the points array, searching at most nump points
    // in case the polygon is flat.
    l0i = top;
    for (int i = top + 1; i < nump && p[i].y == p[top].y; i++) {
      l0i = i;
    }
    if (l0i == nump - 1) {
      for (int i = 0; i <= top && p[i].y == p[top].y; i++) {
        l0i = i;
      }
    }
    // Find right-most top point, the start of the right descending edge.
    // Advance backward in the points array, searching at most nump points.
    r0i = top;
    for (int i = top - 1; i >= 0 && p[i].y == p[top].y; i--) {
      r0i = i;
    }
    if (r0i == 0) {
      for (int i = nump - 1; i >= top && p[i].y == p[top].y; i--) {
        r0i = i;
      }
    }
    // End of left edge is next point after left edge start.
    l1i = NEXT_POINT(l0i);
    // End of right edge is prev point after right edge start.
    r1i = PREV_POINT(r0i);
    l0 = p[l0i];  // Start of left edge
    r0 = p[r0i];  // End of left edge
    l1 = p[l1i];  // Start of right edge
    r1 = p[r1i];  // End of right edge
  }

  struct Edge {
    float yScale;
    // Current coordinates for edge. Where in the 2D case of draw_quad_spans,
    // it is enough to just track the X coordinate as we advance along the rows,
    // for the perspective case we also need to keep track of Z and W. For
    // simplicity, we just use the full 3D point to track all these coordinates.
    Point3D pSlope;
    Point3D p;
    Interpolants interpSlope;
    Interpolants interp;

    Edge(float y, const Point3D& p0, const Point3D& p1, const Interpolants& i0,
         const Interpolants& i1)
        :  // Inverse Y scale for slope calculations. Avoid divide on 0-length
           // edge.
          yScale(1.0f / max(p1.y - p0.y, 1.0f / 256)),
          // Calculate dX/dY slope
          pSlope((p1 - p0) * yScale),
          // Initialize current coords based on Y and slope
          p(p0 + (y - p0.y) * pSlope),
          // Crucially, these interpolants must be scaled by the point's 1/w
          // value, which allows linear interpolation in a perspective-correct
          // manner. This will be canceled out inside the fragment shader later.
          // Calculate change in interpolants per change in Y
          interpSlope((i1 * p1.w - i0 * p0.w) * yScale),
          // Initialize current interpolants based on Y and slope
          interp(i0 * p0.w + (y - p0.y) * interpSlope) {}

    float x() const { return p.x; }
    vec2_scalar zw() const { return {p.z, p.w}; }

    void nextRow() {
      // step current coords and interpolants to next row from slope
      p += pSlope;
      interp += interpSlope;
    }
  };

  // Vertex selection above should result in equal left and right start rows
  assert(l0.y == r0.y);
  // Find the start y, clip to within the clip rect, and round to row center.
  float y = floor(max(l0.y, clipRect.y0) + 0.5f) + 0.5f;
  // Initialize left and right edges from end points and start Y
  Edge left(y, l0, l1, interp_outs[l0i], interp_outs[l1i]);
  Edge right(y, r0, r1, interp_outs[r0i], interp_outs[r1i]);
  // Get pointer to color buffer and depth buffer at current Y
  P* fbuf = (P*)colortex.sample_ptr(0, int(y), layer);
  DepthRun* fdepth = (DepthRun*)depthtex.sample_ptr(0, int(y));
  // Loop along advancing Ys, rasterizing spans at each row
  float checkY = min(min(l1.y, r1.y), clipRect.y1);
  for (;;) {
    // Check if we maybe passed edge ends or outside clip rect...
    if (y > checkY) {
      // If we're outside the clip rect, we're done.
      if (y > clipRect.y1) break;
      // Check if Y advanced past the end of the left edge
      if (y > l1.y) {
        // Step to next left edge past Y and reset edge interpolants.
        do {
          STEP_EDGE(l0i, l0, l1i, l1, NEXT_POINT, r1i);
        } while (y > l1.y);
        left = Edge(y, l0, l1, interp_outs[l0i], interp_outs[l1i]);
      }
      // Check if Y advanced past the end of the right edge
      if (y > r1.y) {
        // Step to next right edge past Y and reset edge interpolants.
        do {
          STEP_EDGE(r0i, r0, r1i, r1, PREV_POINT, l1i);
        } while (y > r1.y);
        right = Edge(y, r0, r1, interp_outs[r0i], interp_outs[r1i]);
      }
      // Reset check condition for next time around.
      checkY = min(min(l1.y, r1.y), clipRect.y1);
    }
    // lx..rx form the bounds of the span. WR does not use backface culling,
    // so we need to use min/max to support the span in either orientation.
    // Clip the span to fall within the clip rect and then round to nearest
    // column.
    int startx = int(max(min(left.x(), right.x()), clipRect.x0) + 0.5f);
    int endx = int(min(max(left.x(), right.x()), clipRect.x1) + 0.5f);
    // Check if span is non-empty.
    int span = endx - startx;
    if (span > 0) {
      ctx->shaded_rows++;
      ctx->shaded_pixels += span;
      // Advance color/depth buffer pointers to the start of the span.
      P* buf = fbuf + startx;
      // Check if the we will need to use depth-buffer or discard on this span.
      DepthRun* depth =
          depthtex.buf != nullptr && depthtex.cleared() ? fdepth : nullptr;
      bool use_discard = fragment_shader->use_discard();
      if (depth) {
        // Perspective may cause the depth value to vary on a per sample basis.
        // Ensure the depth row is flattened to allow testing of individual
        // samples
        if (!depth->is_flat()) {
          flatten_depth_runs(depth, depthtex.width);
        }
        // Advance to the depth sample at the start of the span.
        depth += startx;
      }
      if (colortex.delay_clear) {
        // Delayed clear is enabled for the color buffer. Check if needs clear.
        prepare_row<P>(colortex, int(y), startx, endx, use_discard, depth);
      }
      // Initialize fragment shader interpolants to current span position.
      fragment_shader->gl_FragCoord.x = init_interp(startx + 0.5f, 1);
      fragment_shader->gl_FragCoord.y = y;
      {
        // Calculate the fragment Z and W change per change in fragment X step.
        vec2_scalar stepZW =
            (right.zw() - left.zw()) * (1.0f / (right.x() - left.x()));
        // Calculate initial Z and W values for span start.
        vec2_scalar zw = left.zw() + stepZW * (startx + 0.5f - left.x());
        // Set fragment shader's Z and W values so that it can use them to
        // cancel out the 1/w baked into the interpolants.
        fragment_shader->gl_FragCoord.z = init_interp(zw.x, stepZW.x);
        fragment_shader->gl_FragCoord.w = init_interp(zw.y, stepZW.y);
        fragment_shader->swgl_StepZW = stepZW;
        // Change in interpolants is difference between current right and left
        // edges per the change in right and left X. The left and right
        // interpolant values were previously multipled by 1/w, so the step and
        // initial span values take this into account.
        Interpolants step =
            (right.interp - left.interp) * (1.0f / (right.x() - left.x()));
        // Advance current interpolants to X at start of span.
        Interpolants o = left.interp + step * (startx + 0.5f - left.x());
        fragment_shader->init_span<true>(&o, &step);
      }
      if (!use_discard) {
        // No discard is used. Common case.
        draw_span<false, true>(buf, depth, span, packDepth);
      } else {
        // Discard is used. Rare.
        draw_span<true, true>(buf, depth, span, packDepth);
      }
    }
    // Advance Y and edge interpolants to next row.
    y++;
    left.nextRow();
    right.nextRow();
    // Advance buffers to next row.
    fbuf += colortex.stride() / sizeof(P);
    fdepth += depthtex.stride() / sizeof(DepthRun);
  }
}

// Clip a primitive against both sides of a view-frustum axis, producing
// intermediate vertexes with interpolated attributes that will no longer
// intersect the selected axis planes. This assumes the primitive is convex
// and should produce at most N+2 vertexes for each invocation (only in the
// worst case where one point falls outside on each of the opposite sides
// with the rest of the points inside).
template <XYZW AXIS>
static int clip_side(int nump, Point3D* p, Interpolants* interp, Point3D* outP,
                     Interpolants* outInterp) {
  int numClip = 0;
  Point3D prev = p[nump - 1];
  Interpolants prevInterp = interp[nump - 1];
  float prevCoord = prev.select(AXIS);
  // Coordinate must satisfy -W <= C <= W. Determine if it is outside, and
  // if so, remember which side it is outside of.
  int prevSide = prevCoord < -prev.w ? -1 : (prevCoord > prev.w ? 1 : 0);
  // Loop through points, finding edges that cross the planes by evaluating
  // the side at each point.
  for (int i = 0; i < nump; i++) {
    Point3D cur = p[i];
    Interpolants curInterp = interp[i];
    float curCoord = cur.select(AXIS);
    int curSide = curCoord < -cur.w ? -1 : (curCoord > cur.w ? 1 : 0);
    // Check if the previous and current end points are on different sides.
    if (curSide != prevSide) {
      // One of the edge's end points is outside the plane with the other
      // inside the plane. Find the offset where it crosses the plane and
      // adjust the point and interpolants to there.
      if (prevSide) {
        // Edge that was previously outside crosses inside.
        // Evaluate plane equation for previous and current end-point
        // based on previous side and calculate relative offset.
        assert(numClip < nump + 2);
        float prevDist = prevCoord - prevSide * prev.w;
        float curDist = curCoord - prevSide * cur.w;
        float k = prevDist / (prevDist - curDist);
        outP[numClip] = prev + (cur - prev) * k;
        outInterp[numClip] = prevInterp + (curInterp - prevInterp) * k;
        numClip++;
      }
      if (curSide) {
        // Edge that was previously inside crosses outside.
        // Evaluate plane equation for previous and current end-point
        // based on current side and calculate relative offset.
        assert(numClip < nump + 2);
        float prevDist = prevCoord - curSide * prev.w;
        float curDist = curCoord - curSide * cur.w;
        float k = prevDist / (prevDist - curDist);
        outP[numClip] = prev + (cur - prev) * k;
        outInterp[numClip] = prevInterp + (curInterp - prevInterp) * k;
        numClip++;
      }
    }
    if (!curSide) {
      // The current end point is inside the plane, so output point unmodified.
      assert(numClip < nump + 2);
      outP[numClip] = cur;
      outInterp[numClip] = curInterp;
      numClip++;
    }
    prev = cur;
    prevInterp = curInterp;
    prevCoord = curCoord;
    prevSide = curSide;
  }
  return numClip;
}

// Helper function to dispatch to perspective span drawing with points that
// have already been transformed and clipped.
static inline void draw_perspective_clipped(int nump, Point3D* p_clip,
                                            Interpolants* interp_clip,
                                            Texture& colortex, int layer,
                                            Texture& depthtex) {
  // If polygon is ouside clip rect, nothing to draw.
  ClipRect clipRect(colortex);
  if (!clipRect.overlaps(nump, p_clip)) {
    return;
  }

  // Finally draw perspective-correct spans for the polygon.
  if (colortex.internal_format == GL_RGBA8) {
    draw_perspective_spans<uint32_t>(nump, p_clip, interp_clip, colortex, layer,
                                     depthtex, clipRect);
  } else if (colortex.internal_format == GL_R8) {
    draw_perspective_spans<uint8_t>(nump, p_clip, interp_clip, colortex, layer,
                                    depthtex, clipRect);
  } else {
    assert(false);
  }
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
static void draw_perspective(int nump, Interpolants interp_outs[4],
                             Texture& colortex, int layer, Texture& depthtex) {
  // Lines are not supported with perspective.
  assert(nump >= 3);
  // Convert output of vertex shader to screen space.
  vec4 pos = vertex_shader->gl_Position;
  vec3_scalar scale =
      vec3_scalar(ctx->viewport.width(), ctx->viewport.height(), 1) * 0.5f;
  vec3_scalar offset =
      vec3_scalar(ctx->viewport.x0, ctx->viewport.y0, 0.0f) + scale;
  if (test_none(pos.z <= -pos.w || pos.z >= pos.w)) {
    // No points cross the near or far planes, so no clipping required.
    // Just divide coords by W and convert to viewport.
    Float w = 1.0f / pos.w;
    vec3 screen = pos.sel(X, Y, Z) * w * scale + offset;
    Point3D p[4] = {{screen.x.x, screen.y.x, screen.z.x, w.x},
                    {screen.x.y, screen.y.y, screen.z.y, w.y},
                    {screen.x.z, screen.y.z, screen.z.z, w.z},
                    {screen.x.w, screen.y.w, screen.z.w, w.w}};
    draw_perspective_clipped(nump, p, interp_outs, colortex, layer, depthtex);
  } else {
    // Points cross the near or far planes, so we need to clip.
    // Start with the original 3 or 4 points...
    Point3D p[4] = {{pos.x.x, pos.y.x, pos.z.x, pos.w.x},
                    {pos.x.y, pos.y.y, pos.z.y, pos.w.y},
                    {pos.x.z, pos.y.z, pos.z.z, pos.w.z},
                    {pos.x.w, pos.y.w, pos.z.w, pos.w.w}};
    // Clipping can expand the points by 1 for each of 6 view frustum planes.
    Point3D p_clip[4 + 6];
    Interpolants interp_clip[4 + 6];
    // Clip against near and far Z planes.
    nump = clip_side<Z>(nump, p, interp_outs, p_clip, interp_clip);
    // If no points are left inside the view frustum, there's nothing to draw.
    if (nump < 3) {
      return;
    }
    // After clipping against only the near and far planes, we might still
    // produce points where W = 0, exactly at the camera plane. OpenGL specifies
    // that for clip coordinates, points must satisfy:
    //   -W <= X <= W
    //   -W <= Y <= W
    //   -W <= Z <= W
    // When Z = W = 0, this is trivially satisfied, but when we transform and
    // divide by W below it will produce a divide by 0. Usually we want to only
    // clip Z to avoid the extra work of clipping X and Y. We can still project
    // points that fall outside the view frustum X and Y so long as Z is valid.
    // The span drawing code will then ensure X and Y are clamped to viewport
    // boundaries. However, in the Z = W = 0 case, sometimes clipping X and Y,
    // will push W further inside the view frustum so that it is no longer 0,
    // allowing us to finally proceed to projecting the points to the screen.
    for (int i = 0; i < nump; i++) {
      // Found an invalid W, so need to clip against X and Y...
      if (p_clip[i].w <= 0.0f) {
        // Ping-pong p_clip -> p_tmp -> p_clip.
        Point3D p_tmp[4 + 6];
        Interpolants interp_tmp[4 + 6];
        nump = clip_side<X>(nump, p_clip, interp_clip, p_tmp, interp_tmp);
        if (nump < 3) return;
        nump = clip_side<Y>(nump, p_tmp, interp_tmp, p_clip, interp_clip);
        if (nump < 3) return;
        // After clipping against X and Y planes, there's still points left
        // to draw, so proceed to trying projection now...
        break;
      }
    }
    // Divide coords by W and convert to viewport.
    for (int i = 0; i < nump; i++) {
      float w = 1.0f / p_clip[i].w;
      p_clip[i] = Point3D(p_clip[i].sel(X, Y, Z) * w * scale + offset, w);
    }
    draw_perspective_clipped(nump, p_clip, interp_clip, colortex, layer,
                             depthtex);
  }
}

static void draw_quad(int nump, Texture& colortex, int layer,
                      Texture& depthtex) {
  // Run vertex shader once for the primitive's vertices.
  // Reserve space for 6 sets of interpolants, in case we need to clip against
  // near and far planes in the perspective case.
  Interpolants interp_outs[4];
  vertex_shader->run_primitive((char*)interp_outs, sizeof(Interpolants));
  vec4 pos = vertex_shader->gl_Position;
  // Check if any vertex W is different from another. If so, use perspective.
  if (test_any(pos.w != pos.w.x)) {
    draw_perspective(nump, interp_outs, colortex, layer, depthtex);
    return;
  }

  // Convert output of vertex shader to screen space.
  // Divide coords by W and convert to viewport.
  float w = 1.0f / pos.w.x;
  vec2 screen = (pos.sel(X, Y) * w + 1) * 0.5f *
                    vec2_scalar(ctx->viewport.width(), ctx->viewport.height()) +
                vec2_scalar(ctx->viewport.x0, ctx->viewport.y0);
  Point2D p[4] = {{screen.x.x, screen.y.x},
                  {screen.x.y, screen.y.y},
                  {screen.x.z, screen.y.z},
                  {screen.x.w, screen.y.w}};

  // If quad is ouside clip rect, nothing to draw.
  ClipRect clipRect(colortex);
  if (!clipRect.overlaps(nump, p)) {
    return;
  }

  // Since the quad is assumed 2D, Z is constant across the quad.
  float screenZ = (pos.z.x * w + 1) * 0.5f;
  if (screenZ < 0 || screenZ > 1) {
    // Z values would cross the near or far plane, so just bail.
    return;
  }
  // Since Z doesn't need to be interpolated, just set the fragment shader's
  // Z and W values here, once and for all fragment shader invocations.
  uint16_t z = uint16_t(0xFFFF * screenZ);
  fragment_shader->gl_FragCoord.z = screenZ;
  fragment_shader->gl_FragCoord.w = w;

  // If supplied a line, adjust it so that it is a quad at least 1 pixel thick.
  // Assume that for a line that all 4 SIMD lanes were actually filled with
  // vertexes 0, 1, 1, 0.
  if (nump == 2) {
    // Nudge Y height to span at least 1 pixel by advancing to next pixel
    // boundary so that we step at least 1 row when drawing spans.
    if (int(p[0].y + 0.5f) == int(p[1].y + 0.5f)) {
      p[2].y = 1 + int(p[1].y + 0.5f);
      p[3].y = p[2].y;
      // Nudge X width to span at least 1 pixel so that rounded coords fall on
      // separate pixels.
      if (int(p[0].x + 0.5f) == int(p[1].x + 0.5f)) {
        p[1].x += 1.0f;
        p[2].x += 1.0f;
      }
    } else {
      // If the line already spans at least 1 row, then assume line is vertical
      // or diagonal and just needs to be dilated horizontally.
      p[2].x += 1.0f;
      p[3].x += 1.0f;
    }
    // Pretend that it's a quad now...
    nump = 4;
  }

  // Finally draw 2D spans for the quad. Currently only supports drawing to
  // RGBA8 and R8 color buffers.
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
    VertexAttrib& attr = attribs[i];
    if (attr.enabled) {
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

template <typename INDEX>
static inline void draw_elements(GLsizei count, GLsizei instancecount,
                                 size_t offset, VertexArray& v,
                                 Texture& colortex, int layer,
                                 Texture& depthtex) {
  Buffer& indices_buf = ctx->buffers[v.element_array_buffer_binding];
  if (!indices_buf.buf || offset >= indices_buf.size) {
    return;
  }
  assert((offset & (sizeof(INDEX) - 1)) == 0);
  INDEX* indices = (INDEX*)(indices_buf.buf + offset);
  count = min(count, (GLsizei)((indices_buf.size - offset) / sizeof(INDEX)));
  // Triangles must be indexed at offsets 0, 1, 2.
  // Quads must be successive triangles indexed at offsets 0, 1, 2, 2, 1, 3.
  if (count == 6 && indices[1] == indices[0] + 1 &&
      indices[2] == indices[0] + 2 && indices[5] == indices[0] + 3) {
    assert(indices[3] == indices[0] + 2 && indices[4] == indices[0] + 1);
    // Fast path - since there is only a single quad, we only load per-vertex
    // attribs once for all instances, as they won't change across instances
    // or within an instance.
    vertex_shader->load_attribs(v.attribs, indices[0], 0, 4);
    draw_quad(4, colortex, layer, depthtex);
    for (GLsizei instance = 1; instance < instancecount; instance++) {
      vertex_shader->load_attribs(v.attribs, indices[0], instance, 0);
      draw_quad(4, colortex, layer, depthtex);
    }
  } else {
    for (GLsizei instance = 0; instance < instancecount; instance++) {
      for (GLsizei i = 0; i + 3 <= count; i += 3) {
        if (indices[i + 1] != indices[i] + 1 ||
            indices[i + 2] != indices[i] + 2) {
          continue;
        }
        if (i + 6 <= count && indices[i + 5] == indices[i] + 3) {
          assert(indices[i + 3] == indices[i] + 2 &&
                 indices[i + 4] == indices[i] + 1);
          vertex_shader->load_attribs(v.attribs, indices[i], instance, 4);
          draw_quad(4, colortex, layer, depthtex);
          i += 3;
        } else {
          vertex_shader->load_attribs(v.attribs, indices[i], instance, 3);
          draw_quad(3, colortex, layer, depthtex);
        }
      }
    }
  }
}

extern "C" {

void DrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                           GLintptr offset, GLsizei instancecount) {
  if (offset < 0 || count <= 0 || instancecount <= 0) {
    return;
  }

  Framebuffer& fb = *get_framebuffer(GL_DRAW_FRAMEBUFFER);
  Texture& colortex = ctx->textures[fb.color_attachment];
  if (!colortex.buf) {
    return;
  }
  assert(!colortex.locked);
  assert(colortex.internal_format == GL_RGBA8 ||
         colortex.internal_format == GL_R8);
  Texture& depthtex = ctx->textures[ctx->depthtest ? fb.depth_attachment : 0];
  if (depthtex.buf) {
    assert(depthtex.internal_format == GL_DEPTH_COMPONENT16);
    assert(colortex.width == depthtex.width &&
           colortex.height == depthtex.height);
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

  vertex_shader->init_batch();

  switch (type) {
    case GL_UNSIGNED_SHORT:
      assert(mode == GL_TRIANGLES);
      draw_elements<uint16_t>(count, instancecount, offset, v, colortex,
                              fb.layer, depthtex);
      break;
    case GL_UNSIGNED_INT:
      assert(mode == GL_TRIANGLES);
      draw_elements<uint32_t>(count, instancecount, offset, v, colortex,
                              fb.layer, depthtex);
      break;
    case GL_NONE:
      // Non-standard GL extension - if element type is GL_NONE, then we don't
      // use any element buffer and behave as if DrawArrays was called instead.
      for (GLsizei instance = 0; instance < instancecount; instance++) {
        switch (mode) {
          case GL_LINES:
            for (GLsizei i = 0; i + 2 <= count; i += 2) {
              vertex_shader->load_attribs(v.attribs, offset + i, instance, 2);
              draw_quad(2, colortex, fb.layer, depthtex);
            }
            break;
          case GL_TRIANGLES:
            for (GLsizei i = 0; i + 3 <= count; i += 3) {
              vertex_shader->load_attribs(v.attribs, offset + i, instance, 3);
              draw_quad(3, colortex, fb.layer, depthtex);
            }
            break;
          default:
            assert(false);
            break;
        }
      }
      break;
    default:
      assert(false);
      break;
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

void MakeCurrent(Context* c) {
  if (ctx == c) {
    return;
  }
  ctx = c;
  if (ctx) {
    setup_program(ctx->current_program);
    blend_key = ctx->blend ? ctx->blend_key : BLEND_KEY_NONE;
  } else {
    setup_program(0);
    blend_key = BLEND_KEY_NONE;
  }
}

Context* CreateContext() { return new Context; }

void ReferenceContext(Context* c) {
  if (!c) {
    return;
  }
  ++c->references;
}

void DestroyContext(Context* c) {
  if (!c) {
    return;
  }
  assert(c->references > 0);
  --c->references;
  if (c->references > 0) {
    return;
  }
  if (ctx == c) {
    MakeCurrent(nullptr);
  }
  delete c;
}

}  // extern "C"

#include "composite.h"
