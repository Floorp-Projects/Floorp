/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ImageBitmapColorUtils_h
#define mozilla_dom_ImageBitmapColorUtils_h

#include "mozilla/UniquePtr.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {
namespace dom {

/*
 * RGB family -> RGBA family.
 */
int
RGB24ToRGBA32(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight);

int
BGR24ToRGBA32(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight);
int
RGB24ToBGRA32(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight);

int
BGR24ToBGRA32(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight);

/*
 * RGBA family -> RGB family.
 */
int
RGBA32ToRGB24(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight);

int
BGRA32ToRGB24(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight);
int
RGBA32ToBGR24(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight);

int
BGRA32ToBGR24(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight);

/*
 * Among RGB family.
 */
int
RGB24ToBGR24(const uint8_t* aSrcBuffer, int aSrcStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight);

#define BGR24ToRGB24 RGB24ToBGR24

/*
 * YUV family -> RGB family.
 */
int
YUV444PToRGB24(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight);

int
YUV422PToRGB24(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight);

int
YUV420PToRGB24(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight);

int
NV12ToRGB24(const uint8_t* aYBuffer, int aYStride,
            const uint8_t* aUVBuffer, int aUVStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

int
NV21ToRGB24(const uint8_t* aYBuffer, int aYStride,
            const uint8_t* aVUBuffer, int aVUStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

int
YUV444PToBGR24(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight);

int
YUV422PToBGR24(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight);

int
YUV420PToBGR24(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight);

int
NV12ToBGR24(const uint8_t* aYBuffer, int aYStride,
            const uint8_t* aUVBuffer, int aUVStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

int
NV21ToBGR24(const uint8_t* aYBuffer, int aYStride,
            const uint8_t* aVUBuffer, int aVUStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

/*
 * YUV family -> RGBA family.
 */
int
YUV444PToRGBA32(const uint8_t* aYBuffer, int aYStride,
                const uint8_t* aUBuffer, int aUStride,
                const uint8_t* aVBuffer, int aVStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight);

int
YUV422PToRGBA32(const uint8_t* aYBuffer, int aYStride,
                const uint8_t* aUBuffer, int aUStride,
                const uint8_t* aVBuffer, int aVStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight);

int
YUV420PToRGBA32(const uint8_t* aYBuffer, int aYStride,
                const uint8_t* aUBuffer, int aUStride,
                const uint8_t* aVBuffer, int aVStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight);

int
NV12ToRGBA32(const uint8_t* aYBuffer, int aYStride,
             const uint8_t* aUVBuffer, int aUVStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight);

int
NV21ToRGBA32(const uint8_t* aYBuffer, int aYStride,
             const uint8_t* aVUBuffer, int aVUStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight);

int
YUV444PToBGRA32(const uint8_t* aYBuffer, int aYStride,
                const uint8_t* aUBuffer, int aUStride,
                const uint8_t* aVBuffer, int aVStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight);

int
YUV422PToBGRA32(const uint8_t* aYBuffer, int aYStride,
                const uint8_t* aUBuffer, int aUStride,
                const uint8_t* aVBuffer, int aVStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight);

int
YUV420PToBGRA32(const uint8_t* aYBuffer, int aYStride,
                const uint8_t* aUBuffer, int aUStride,
                const uint8_t* aVBuffer, int aVStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight);

int
NV12ToBGRA32(const uint8_t* aYBuffer, int aYStride,
             const uint8_t* aUVBuffer, int aUVStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight);

int
NV21ToBGRA32(const uint8_t* aYBuffer, int aYStride,
             const uint8_t* aVUBuffer, int aVUStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight);

/*
 * RGB family -> YUV family.
 */
int
RGB24ToYUV444P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight);

int
RGB24ToYUV422P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight);

int
RGB24ToYUV420P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight);
int
RGB24ToNV12(const uint8_t* aSrcBuffer, int aSrcStride,
            uint8_t* aYBuffer, int aYStride,
            uint8_t* aUVBuffer, int aUVStride,
            int aWidth, int aHeight);

int
RGB24ToNV21(const uint8_t* aSrcBuffer, int aSrcStride,
            uint8_t* aYBuffer, int aYStride,
            uint8_t* aVUBuffer, int aVUStride,
            int aWidth, int aHeight);

int
BGR24ToYUV444P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight);

int
BGR24ToYUV422P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight);

int
BGR24ToYUV420P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight);

int
BGR24ToNV12(const uint8_t* aSrcBuffer, int aSrcStride,
            uint8_t* aYBuffer, int aYStride,
            uint8_t* aUVBuffer, int aUVStride,
            int aWidth, int aHeight);

int
BGR24ToNV21(const uint8_t* aSrcBuffer, int aSrcStride,
            uint8_t* aYBuffer, int aYStride,
            uint8_t* aUVBuffer, int aUVStride,
            int aWidth, int aHeight);

/*
 * RGBA family -> YUV family.
 */
int
RGBA32ToYUV444P(const uint8_t* aSrcBuffer, int aSrcStride,
                uint8_t* aYBuffer, int aYStride,
                uint8_t* aUBuffer, int aUStride,
                uint8_t* aVBuffer, int aVStride,
                int aWidth, int aHeight);

int
RGBA32ToYUV422P(const uint8_t* aSrcBuffer, int aSrcStride,
                uint8_t* aYBuffer, int aYStride,
                uint8_t* aUBuffer, int aUStride,
                uint8_t* aVBuffer, int aVStride,
                int aWidth, int aHeight);

int
RGBA32ToYUV420P(const uint8_t* aSrcBuffer, int aSrcStride,
                uint8_t* aYBuffer, int aYStride,
                uint8_t* aUBuffer, int aUStride,
                uint8_t* aVBuffer, int aVStride,
                int aWidth, int aHeight);
int
RGBA32ToNV12(const uint8_t* aSrcBuffer, int aSrcStride,
             uint8_t* aYBuffer, int aYStride,
             uint8_t* aUVBuffer, int aUVStride,
             int aWidth, int aHeight);

int
RGBA32ToNV21(const uint8_t* aSrcBuffer, int aSrcStride,
             uint8_t* aYBuffer, int aYStride,
             uint8_t* aVUBuffer, int aVUStride,
             int aWidth, int aHeight);

int
BGRA32ToYUV444P(const uint8_t* aSrcBuffer, int aSrcStride,
                uint8_t* aYBuffer, int aYStride,
                uint8_t* aUBuffer, int aUStride,
                uint8_t* aVBuffer, int aVStride,
                int aWidth, int aHeight);

int
BGRA32ToYUV422P(const uint8_t* aSrcBuffer, int aSrcStride,
                uint8_t* aYBuffer, int aYStride,
                uint8_t* aUBuffer, int aUStride,
                uint8_t* aVBuffer, int aVStride,
                int aWidth, int aHeight);

int
BGRA32ToYUV420P(const uint8_t* aSrcBuffer, int aSrcStride,
                uint8_t* aYBuffer, int aYStride,
                uint8_t* aUBuffer, int aUStride,
                uint8_t* aVBuffer, int aVStride,
                int aWidth, int aHeight);


int
BGRA32ToNV12(const uint8_t* aSrcBuffer, int aSrcStride,
             uint8_t* aYBuffer, int aYStride,
             uint8_t* aUVBuffer, int aUVStride,
             int aWidth, int aHeight);

int
BGRA32ToNV21(const uint8_t* aSrcBuffer, int aSrcStride,
             uint8_t* aYBuffer, int aYStride,
             uint8_t* aUVBuffer, int aUVStride,
             int aWidth, int aHeight);

/*
 * RGBA/RGB family <-> HSV family.
 */
int
RGBA32ToHSV(const uint8_t* aSrcBuffer, int aSrcStride,
            float* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

int
BGRA32ToHSV(const uint8_t* aSrcBuffer, int aSrcStride,
            float* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

int
RGB24ToHSV(const uint8_t* aSrcBuffer, int aSrcStride,
           float* aDstBuffer, int aDstStride,
           int aWidth, int aHeight);

int
BGR24ToHSV(const uint8_t* aSrcBuffer, int aSrcStride,
           float* aDstBuffer, int aDstStride,
           int aWidth, int aHeight);

int
HSVToRGBA32(const float* aSrcBuffer, int aSrcStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

int
HSVToBGRA32(const float* aSrcBuffer, int aSrcStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

int
HSVToRGB24(const float* aSrcBuffer, int aSrcStride,
           uint8_t* aDstBuffer, int aDstStride,
           int aWidth, int aHeight);

int
HSVToBGR24(const float* aSrcBuffer, int aSrcStride,
           uint8_t* aDstBuffer, int aDstStride,
           int aWidth, int aHeight);

/*
 * RGBA/RGB family <-> Lab family.
 */
int
RGBA32ToLab(const uint8_t* aSrcBuffer, int aSrcStride,
            float* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

int
BGRA32ToLab(const uint8_t* aSrcBuffer, int aSrcStride,
            float* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

int
RGB24ToLab(const uint8_t* aSrcBuffer, int aSrcStride,
           float* aDstBuffer, int aDstStride,
           int aWidth, int aHeight);

int
BGR24ToLab(const uint8_t* aSrcBuffer, int aSrcStride,
           float* aDstBuffer, int aDstStride,
           int aWidth, int aHeight);

int
LabToRGBA32(const float* aSrcBuffer, int aSrcStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

int
LabToBGRA32(const float* aSrcBuffer, int aSrcStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

int
LabToRGB24(const float* aSrcBuffer, int aSrcStride,
           uint8_t* aDstBuffer, int aDstStride,
           int aWidth, int aHeight);

int
LabToBGR24(const float* aSrcBuffer, int aSrcStride,
           uint8_t* aDstBuffer, int aDstStride,
           int aWidth, int aHeight);

/*
 * RGBA/RGB family -> Gray8.
 */
int
RGB24ToGray8(const uint8_t* aSrcBuffer, int aSrcStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight);

int
BGR24ToGray8(const uint8_t* aSrcBuffer, int aSrcStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight);

int
RGBA32ToGray8(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight);

int
BGRA32ToGray8(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight);

/*
 * YUV family -> Gray8.
 */
int
YUV444PToGray8(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight);

int
YUV422PToGray8(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight);

int
YUV420PToGray8(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight);

int
NV12ToGray8(const uint8_t* aYBuffer, int aYStride,
            const uint8_t* aUBuffer, int aUStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

int
NV21ToGray8(const uint8_t* aYBuffer, int aYStride,
            const uint8_t* aUBuffer, int aUStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight);

} // namespace dom
} // namespace mozilla
#endif // mozilla_dom_ImageBitmapColorUtils_h
