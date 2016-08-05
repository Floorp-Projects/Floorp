/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "avutil.h"
#include "opt.h"

// cpu_internal.c
int ff_get_cpu_flags_aarch64(void) { return 0; }
int ff_get_cpu_flags_arm(void) { return 0; }
int ff_get_cpu_flags_ppc(void) { return 0; }

// float_dsp.c
#include "float_dsp.h"
void ff_float_dsp_init_aarch64(AVFloatDSPContext *fdsp) {}
void ff_float_dsp_init_arm(AVFloatDSPContext *fdsp) {}
void ff_float_dsp_init_ppc(AVFloatDSPContext *fdsp, int strict) {}
void ff_float_dsp_init_mips(AVFloatDSPContext *fdsp) {}

// opt.c
int av_opt_show2(void *obj, void *av_log_obj, int req_flags, int rej_flags) { return 0; }
void av_opt_set_defaults(void *s) {}
void av_opt_set_defaults2(void *s, int mask, int flags) {}
int av_set_options_string(void *ctx, const char *opts,
                          const char *key_val_sep, const char *pairs_sep) { return 0; }
int av_opt_set_from_string(void *ctx, const char *opts,
                           const char *const *shorthand,
                           const char *key_val_sep, const char *pairs_sep) { return 0; }
void av_opt_free(void *obj) {}
int av_opt_flag_is_set(void *obj, const char *field_name, const char *flag_name) { return 0; }
int av_opt_set_dict(void *obj, struct AVDictionary **options) { return 0; }
int av_opt_set_dict2(void *obj, struct AVDictionary **options, int search_flags) { return 0; }
int av_opt_get_key_value(const char **ropts,
                         const char *key_val_sep, const char *pairs_sep,
                         unsigned flags,
                         char **rkey, char **rval) { return 0; }
int av_opt_eval_flags (void *obj, const AVOption *o, const char *val, int       *flags_out) { return 0; }
int av_opt_eval_int   (void *obj, const AVOption *o, const char *val, int       *int_out) { return 0; }
int av_opt_eval_int64 (void *obj, const AVOption *o, const char *val, int64_t   *int64_out) { return 0; }
int av_opt_eval_float (void *obj, const AVOption *o, const char *val, float     *float_out) { return 0; }
int av_opt_eval_double(void *obj, const AVOption *o, const char *val, double    *double_out) { return 0; }
int av_opt_eval_q     (void *obj, const AVOption *o, const char *val, AVRational *q_out) { return 0; }
const AVOption *av_opt_find(void *obj, const char *name, const char *unit,
                            int opt_flags, int search_flags) { return 0; }
const AVOption *av_opt_find2(void *obj, const char *name, const char *unit,
                             int opt_flags, int search_flags, void **target_obj) { return 0; }
const AVOption *av_opt_next(const void *obj, const AVOption *prev) { return 0; }
void *av_opt_child_next(void *obj, void *prev) { return 0; }
const AVClass *av_opt_child_class_next(const AVClass *parent, const AVClass *prev) { return 0; }
int av_opt_set         (void *obj, const char *name, const char *val, int search_flags) { return 0; }
int av_opt_set_int     (void *obj, const char *name, int64_t     val, int search_flags) { return 0; }
int av_opt_set_double  (void *obj, const char *name, double      val, int search_flags) { return 0; }
int av_opt_set_q       (void *obj, const char *name, AVRational  val, int search_flags) { return 0; }
int av_opt_set_bin     (void *obj, const char *name, const uint8_t *val, int size, int search_flags) { return 0; }
int av_opt_set_image_size(void *obj, const char *name, int w, int h, int search_flags) { return 0; }
int av_opt_set_pixel_fmt (void *obj, const char *name, enum AVPixelFormat fmt, int search_flags) { return 0; }
int av_opt_set_sample_fmt(void *obj, const char *name, enum AVSampleFormat fmt, int search_flags) { return 0; }
int av_opt_set_video_rate(void *obj, const char *name, AVRational val, int search_flags) { return 0; }
int av_opt_set_channel_layout(void *obj, const char *name, int64_t ch_layout, int search_flags) { return 0; }
int av_opt_set_dict_val(void *obj, const char *name, const AVDictionary *val, int search_flags) { return 0; }
int av_opt_get         (void *obj, const char *name, int search_flags, uint8_t **out_val) { return 0; }
int av_opt_get_int     (void *obj, const char *name, int search_flags, int64_t *out_val) { return 0; }
int av_opt_get_double  (void *obj, const char *name, int search_flags, double *out_val) { return 0; }
int av_opt_get_q       (void *obj, const char *name, int search_flags, AVRational *out_val) { return 0; }
int av_opt_get_image_size(void *obj, const char *name, int search_flags, int *w_out, int *h_out) { return 0; }
int av_opt_get_pixel_fmt (void *obj, const char *name, int search_flags, enum AVPixelFormat *out_fmt) { return 0; }
int av_opt_get_sample_fmt(void *obj, const char *name, int search_flags, enum AVSampleFormat *out_fmt) { return 0; }
int av_opt_get_video_rate(void *obj, const char *name, int search_flags, AVRational *out_val) { return 0; }
int av_opt_get_channel_layout(void *obj, const char *name, int search_flags, int64_t *ch_layout) { return 0; }
int av_opt_get_dict_val(void *obj, const char *name, int search_flags, AVDictionary **out_val) { return 0; }
void *av_opt_ptr(const AVClass *avclass, void *obj, const char *name) { return 0; }
void av_opt_freep_ranges(AVOptionRanges **ranges) {}
int av_opt_query_ranges(AVOptionRanges **b, void *obj, const char *key, int flags) { return 0; }
int av_opt_copy(void *dest, const void *src) { return 0; }
int av_opt_query_ranges_default(AVOptionRanges **b, void *obj, const char *key, int flags) { return 0; }
int av_opt_is_set_to_default(void *obj, const AVOption *o) { return 0; }
int av_opt_is_set_to_default_by_name(void *obj, const char *name, int search_flags) { return 0; }
int av_opt_serialize(void *obj, int opt_flags, int flags, char **buffer,
                     const char key_val_sep, const char pairs_sep) { return 0; }
