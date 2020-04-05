/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WR_h
#define WR_h

#include "mozilla/gfx/Types.h"
#include "nsTArray.h"

extern "C" {

// ----
// Functions invoked from Rust code
// ----

bool is_in_compositor_thread();
bool is_in_main_thread();
bool is_in_render_thread();
bool is_glcontext_gles(void* glcontext_ptr);
bool is_glcontext_angle(void* glcontext_ptr);
bool gfx_use_wrench();
const char* gfx_wr_resource_path_override();
void gfx_critical_note(const char* msg);
void gfx_critical_error(const char* msg);
void gecko_printf_stderr_output(const char* msg);
void* get_proc_address_from_glcontext(void* glcontext_ptr,
                                      const char* procname);
void gecko_profiler_register_thread(const char* threadname);
void gecko_profiler_unregister_thread();

void gecko_profiler_start_marker(const char* name);
void gecko_profiler_end_marker(const char* name);
void gecko_profiler_event_marker(const char* name);
void gecko_profiler_add_text_marker(const char* name, const char* text_ptr,
                                    size_t text_len, uint64_t microseconds);
bool gecko_profiler_thread_is_being_profiled();

// IMPORTANT: Keep this synchronized with enumerate_interners in
// gfx/wr/webrender_api
#define WEBRENDER_FOR_EACH_INTERNER(macro) \
  macro(clip);                             \
  macro(prim);                             \
  macro(normal_border);                    \
  macro(image_border);                     \
  macro(image);                            \
  macro(yuv_image);                        \
  macro(line_decoration);                  \
  macro(linear_grad);                      \
  macro(radial_grad);                      \
  macro(conic_grad);                       \
  macro(picture);                          \
  macro(text_run);                         \
  macro(filterdata);                       \
  macro(backdrop);

// Prelude of types necessary before including webrender_ffi_generated.h
namespace mozilla {
namespace wr {

// Because this struct is macro-generated on the Rust side, cbindgen can't see
// it. Work around that by re-declaring it here.
#define DECLARE_MEMBER(id) uintptr_t id;
struct InternerSubReport {
  WEBRENDER_FOR_EACH_INTERNER(DECLARE_MEMBER)
};

#undef DECLARE_MEMBER

struct Transaction;
struct WrWindowId;
struct DocumentId;
struct WrPipelineInfo;

struct WrPipelineIdAndEpoch;
using WrPipelineIdEpochs = nsTArray<WrPipelineIdAndEpoch>;

const uint64_t ROOT_CLIP_CHAIN = ~0;

}  // namespace wr
}  // namespace mozilla

void apz_register_updater(mozilla::wr::WrWindowId aWindowId);
void apz_pre_scene_swap(mozilla::wr::WrWindowId aWindowId);
void apz_post_scene_swap(mozilla::wr::WrWindowId aWindowId,
                         const mozilla::wr::WrPipelineInfo* aInfo);
void apz_run_updater(mozilla::wr::WrWindowId aWindowId);
void apz_deregister_updater(mozilla::wr::WrWindowId aWindowId);

void apz_register_sampler(mozilla::wr::WrWindowId aWindowId);
void apz_sample_transforms(
    mozilla::wr::WrWindowId aWindowId, mozilla::wr::Transaction* aTransaction,
    mozilla::wr::DocumentId aRenderRootId,
    const mozilla::wr::WrPipelineIdEpochs* aPipelineEpochs);
void apz_deregister_sampler(mozilla::wr::WrWindowId aWindowId);
}  // extern "C"

// Work-around wingdi.h define which conflcits with WR color constant
#pragma push_macro("TRANSPARENT")
#undef TRANSPARENT

#include "webrender_ffi_generated.h"

#pragma pop_macro("TRANSPARENT")

// More functions invoked from Rust code. These are down here because they
// refer to data structures from webrender_ffi_generated.h
extern "C" {
void record_telemetry_time(mozilla::wr::TelemetryProbe aProbe,
                           uint64_t aTimeNs);
}

namespace mozilla {
namespace wr {

// Cast a blob image key into a regular image for use in
// a display item.
inline ImageKey AsImageKey(BlobImageKey aKey) { return aKey._0; }

}  // namespace wr
}  // namespace mozilla

#endif  // WR_h
