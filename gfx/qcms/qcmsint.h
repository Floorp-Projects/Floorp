/* vim: set ts=8 sw=8 noexpandtab: */
#ifndef QCMS_INT_H
#define QCMS_INT_H

#include "qcms.h"
#include "qcmstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#define ALIGN __declspec(align(16))
#else
#define ALIGN __attribute__(( aligned (16) ))
#endif

struct _qcms_transform;

typedef void (*transform_fn_t)(const struct _qcms_transform *transform, const unsigned char *src, unsigned char *dest, size_t length);


void qcms_transform_data_rgb_out_lut(const qcms_transform *transform,
                                     const unsigned char *src,
                                     unsigned char *dest,
                                     size_t length);
void qcms_transform_data_rgba_out_lut(const qcms_transform *transform,
                                      const unsigned char *src,
                                      unsigned char *dest,
                                      size_t length);
void qcms_transform_data_bgra_out_lut(const qcms_transform *transform,
                                      const unsigned char *src,
                                      unsigned char *dest,
                                      size_t length);

void qcms_transform_data_rgb_out_lut_precache(const qcms_transform *transform,
                                              const unsigned char *src,
                                              unsigned char *dest,
                                              size_t length);
void qcms_transform_data_rgba_out_lut_precache(const qcms_transform *transform,
                                               const unsigned char *src,
                                               unsigned char *dest,
                                               size_t length);
void qcms_transform_data_bgra_out_lut_precache(const qcms_transform *transform,
                                               const unsigned char *src,
                                               unsigned char *dest,
                                               size_t length);

void qcms_transform_data_rgb_out_lut_avx(const qcms_transform *transform,
                                         const unsigned char *src,
                                         unsigned char *dest,
                                         size_t length);
void qcms_transform_data_rgba_out_lut_avx(const qcms_transform *transform,
                                          const unsigned char *src,
                                          unsigned char *dest,
                                          size_t length);
void qcms_transform_data_bgra_out_lut_avx(const qcms_transform *transform,
                                          const unsigned char *src,
                                          unsigned char *dest,
                                          size_t length);
void qcms_transform_data_rgb_out_lut_sse2(const qcms_transform *transform,
                                          const unsigned char *src,
                                          unsigned char *dest,
                                          size_t length);
void qcms_transform_data_rgba_out_lut_sse2(const qcms_transform *transform,
                                          const unsigned char *src,
                                          unsigned char *dest,
                                          size_t length);
void qcms_transform_data_bgra_out_lut_sse2(const qcms_transform *transform,
                                          const unsigned char *src,
                                          unsigned char *dest,
                                          size_t length);
void qcms_transform_data_rgb_out_lut_sse1(const qcms_transform *transform,
                                          const unsigned char *src,
                                          unsigned char *dest,
                                          size_t length);
void qcms_transform_data_rgba_out_lut_sse1(const qcms_transform *transform,
                                          const unsigned char *src,
                                          unsigned char *dest,
                                          size_t length);
void qcms_transform_data_bgra_out_lut_sse1(const qcms_transform *transform,
                                          const unsigned char *src,
                                          unsigned char *dest,
                                          size_t length);

void qcms_transform_data_rgb_out_lut_altivec(const qcms_transform *transform,
                                             const unsigned char *src,
                                             unsigned char *dest,
                                             size_t length);
void qcms_transform_data_rgba_out_lut_altivec(const qcms_transform *transform,
                                              const unsigned char *src,
                                              unsigned char *dest,
                                              size_t length);
void qcms_transform_data_bgra_out_lut_altivec(const qcms_transform *transform,
                                              const unsigned char *src,
                                              unsigned char *dest,
                                              size_t length);

void qcms_transform_data_rgb_out_lut_neon(const qcms_transform *transform,
                                          const unsigned char *src,
                                          unsigned char *dest,
                                          size_t length);
void qcms_transform_data_rgba_out_lut_neon(const qcms_transform *transform,
                                           const unsigned char *src,
                                           unsigned char *dest,
                                           size_t length);
void qcms_transform_data_bgra_out_lut_neon(const qcms_transform *transform,
                                           const unsigned char *src,
                                           unsigned char *dest,
                                           size_t length);

extern bool qcms_supports_iccv4;
extern bool qcms_supports_neon;
extern bool qcms_supports_avx;

#ifdef __cplusplus
}
#endif

#endif
