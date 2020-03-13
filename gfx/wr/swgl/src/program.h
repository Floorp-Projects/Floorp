/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

struct VertexAttrib;

namespace glsl {

struct VertexShaderImpl;
struct FragmentShaderImpl;

struct ProgramImpl {
  virtual ~ProgramImpl() {}
  virtual int get_uniform(const char* name) const = 0;
  virtual bool set_sampler(int index, int value) = 0;
  virtual void bind_attrib(const char* name, int index) = 0;
  virtual int get_attrib(const char* name) const = 0;
  virtual VertexShaderImpl* get_vertex_shader() = 0;
  virtual FragmentShaderImpl* get_fragment_shader() = 0;
};

typedef ProgramImpl* (*ProgramLoader)();

struct ShaderImpl {
  typedef void (*SetUniform1iFunc)(ShaderImpl*, int index, int value);
  typedef void (*SetUniform4fvFunc)(ShaderImpl*, int index, const float* value);
  typedef void (*SetUniformMatrix4fvFunc)(ShaderImpl*, int index,
                                          const float* value);

  SetUniform1iFunc set_uniform_1i_func = nullptr;
  SetUniform4fvFunc set_uniform_4fv_func = nullptr;
  SetUniformMatrix4fvFunc set_uniform_matrix4fv_func = nullptr;

  void set_uniform_1i(int index, int value) {
    (*set_uniform_1i_func)(this, index, value);
  }

  void set_uniform_4fv(int index, const float* value) {
    (*set_uniform_4fv_func)(this, index, value);
  }

  void set_uniform_matrix4fv(int index, const float* value) {
    (*set_uniform_matrix4fv_func)(this, index, value);
  }
};

struct VertexShaderImpl : ShaderImpl {
  typedef void (*InitBatchFunc)(VertexShaderImpl*, ProgramImpl* prog);
  typedef void (*LoadAttribsFunc)(VertexShaderImpl*, ProgramImpl* prog,
                                  VertexAttrib* attribs,
                                  unsigned short* indices, int start,
                                  int instance, int count);
  typedef void (*RunFunc)(VertexShaderImpl*, char* flats, char* interps,
                          size_t interp_stride);

  InitBatchFunc init_batch_func = nullptr;
  LoadAttribsFunc load_attribs_func = nullptr;
  RunFunc run_func = nullptr;

  vec4 gl_Position;

  void init_batch(ProgramImpl* prog) { (*init_batch_func)(this, prog); }

  ALWAYS_INLINE void load_attribs(ProgramImpl* prog, VertexAttrib* attribs,
                                  unsigned short* indices, int start,
                                  int instance, int count) {
    (*load_attribs_func)(this, prog, attribs, indices, start, instance, count);
  }

  ALWAYS_INLINE void run(char* flats, char* interps, size_t interp_stride) {
    (*run_func)(this, flats, interps, interp_stride);
  }
};

struct FragmentShaderImpl : ShaderImpl {
  typedef void (*InitBatchFunc)(FragmentShaderImpl*, ProgramImpl* prog);
  typedef void (*InitPrimitiveFunc)(FragmentShaderImpl*, const void* flats);
  typedef void (*InitSpanFunc)(FragmentShaderImpl*, const void* interps,
                               const void* step, float step_width);
  typedef void (*RunFunc)(FragmentShaderImpl*);
  typedef void (*SkipFunc)(FragmentShaderImpl*, int chunks);
  typedef bool (*UseDiscardFunc)(FragmentShaderImpl*);
  typedef void (*DrawSpanRGBA8Func)(FragmentShaderImpl*, uint32_t* buf,
                                    int len);
  typedef void (*DrawSpanR8Func)(FragmentShaderImpl*, uint8_t* buf, int len);

  InitBatchFunc init_batch_func = nullptr;
  InitPrimitiveFunc init_primitive_func = nullptr;
  InitSpanFunc init_span_func = nullptr;
  RunFunc run_func = nullptr;
  SkipFunc skip_func = nullptr;
  UseDiscardFunc use_discard_func = nullptr;
  DrawSpanRGBA8Func draw_span_RGBA8_func = nullptr;
  DrawSpanR8Func draw_span_R8_func = nullptr;

  vec2 gl_FragCoordXY;
  vec2_scalar gl_FragCoordZW;
  Bool isPixelDiscarded;
  vec4 gl_FragColor;
  vec4 gl_SecondaryFragColor;

  ALWAYS_INLINE void step_fragcoord() { gl_FragCoordXY.x += 4; }

  ALWAYS_INLINE void step_fragcoord(int chunks) {
    gl_FragCoordXY.x += 4 * chunks;
  }

  void init_batch(ProgramImpl* prog) { (*init_batch_func)(this, prog); }

  void init_primitive(const void* flats) {
    (*init_primitive_func)(this, flats);
  }

  ALWAYS_INLINE void init_span(const void* interps, const void* step,
                               float step_width) {
    (*init_span_func)(this, interps, step, step_width);
  }

  ALWAYS_INLINE void run() { (*run_func)(this); }

  ALWAYS_INLINE void skip(int chunks = 1) { (*skip_func)(this, chunks); }

  ALWAYS_INLINE bool use_discard() { return (*use_discard_func)(this); }

  ALWAYS_INLINE void draw_span(uint32_t* buf, int len) {
    (*draw_span_RGBA8_func)(this, buf, len);
  }

  ALWAYS_INLINE bool has_draw_span(uint32_t*) {
    return draw_span_RGBA8_func != nullptr;
  }

  ALWAYS_INLINE void draw_span(uint8_t* buf, int len) {
    (*draw_span_R8_func)(this, buf, len);
  }

  ALWAYS_INLINE bool has_draw_span(uint8_t*) {
    return draw_span_R8_func != nullptr;
  }
};

}  // namespace glsl
