/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTexelConversions.h"

namespace mozilla {

using namespace WebGLTexelConversions;

namespace {

/** @class WebGLImageConverter
 *
 * This class is just a helper to implement WebGLContext::ConvertImage below.
 *
 * Design comments:
 * 
 * WebGLContext::ConvertImage has to handle hundreds of format conversion paths.
 * It is important to minimize executable code size here. Instead of passing around
 * a large number of function parameters hundreds of times, we create a
 * WebGLImageConverter object once, storing these parameters, and then we call
 * the run() method on it.
 */
class WebGLImageConverter
{
    const size_t mWidth, mHeight;
    const void* const mSrcStart;
    void* const mDstStart;
    const ptrdiff_t mSrcStride, mDstStride;
    bool mAlreadyRun;
    bool mSuccess;

    /*
     * Returns sizeof(texel)/sizeof(type). The point is that we will iterate over
     * texels with typed pointers and this value will tell us by how much we need
     * to increment these pointers to advance to the next texel.
     */
    template<int Format>
    static size_t NumElementsPerTexelForFormat() {
        switch (Format) {
            case R8:
            case A8:
            case R32F:
            case A32F:
            case RGBA5551:
            case RGBA4444:
            case RGB565:
                return 1;
            case RA8:
            case RA32F:
                return 2;
            case RGB8:
            case RGB32F:
                return 3;
            case RGBA8:
            case BGRA8:
            case BGRX8:
            case RGBA32F:
                return 4;
            default:
                NS_ABORT_IF_FALSE(false, "Unknown texel format. Coding mistake?");
                return 0;
        }
    }

    /*
     * This is the completely format-specific templatized conversion function,
     * that will be instantiated hundreds of times for all different combinations.
     * It is important to avoid generating useless code here. In particular, many
     * instantiations of this function template will never be called, so we try
     * to return immediately in these cases to allow the compiler to avoid generating
     * useless code.
     */
    template<WebGLTexelFormat SrcFormat,
             WebGLTexelFormat DstFormat,
             WebGLTexelPremultiplicationOp PremultiplicationOp>
    void run()
    {
        // check for never-called cases. We early-return to allow the compiler
        // to avoid generating this code. It would be tempting to abort() instead,
        // as returning early does leave the destination surface with uninitialized
        // data, but that would not allow the compiler to avoid generating this code.
        // So instead, we return early, so Success() will return false, and the caller
        // must check that and abort in that case. See WebGLContext::ConvertImage.

        if (SrcFormat == DstFormat &&
            PremultiplicationOp == NoPremultiplicationOp)
        {
            // Should have used a fast exit path earlier, rather than entering this function.
            // we explicitly return here to allow the compiler to avoid generating this code
            return;
        }

        // Only textures uploaded from DOM elements or ImageData can allow DstFormat != SrcFormat.
        // DOM elements can only give BGRA8, BGRX8, A8, RGB565 formats. See DOMElementToImageSurface.
        // ImageData is always RGBA8. So all other SrcFormat will always satisfy DstFormat==SrcFormat,
        // so we can avoid compiling the code for all the unreachable paths.
        const bool CanSrcFormatComeFromDOMElementOrImageData
            = SrcFormat == BGRA8 ||
              SrcFormat == BGRX8 ||
              SrcFormat == A8 ||
              SrcFormat == RGB565 ||
              SrcFormat == RGBA8;
        if (!CanSrcFormatComeFromDOMElementOrImageData &&
            SrcFormat != DstFormat)
        {
            return;
        }

        // Likewise, only textures uploaded from DOM elements or ImageData can possibly have to be unpremultiplied.
        if (!CanSrcFormatComeFromDOMElementOrImageData &&
            PremultiplicationOp == Unpremultiply)
        {
            return;
        }

        // there is no point in premultiplication/unpremultiplication
        // in the following cases:
        //  - the source format has no alpha
        //  - the source format has no color
        //  - the destination format has no color
        if (!HasAlpha(SrcFormat) ||
            !HasColor(SrcFormat) ||
            !HasColor(DstFormat))
        {

            if (PremultiplicationOp != NoPremultiplicationOp)
            {
                return;
            }
        }

        // end of early return cases.

        NS_ABORT_IF_FALSE(!mAlreadyRun, "converter should be run only once!");
        mAlreadyRun = true;

        // gather some compile-time meta-data about the formats at hand.

        typedef
            typename DataTypeForFormat<SrcFormat>::Type
            SrcType;
        typedef
            typename DataTypeForFormat<DstFormat>::Type
            DstType;

        const int IntermediateSrcFormat
            = IntermediateFormat<SrcFormat>::Value;
        const int IntermediateDstFormat
            = IntermediateFormat<DstFormat>::Value;
        typedef
            typename DataTypeForFormat<IntermediateSrcFormat>::Type
            IntermediateSrcType;
        typedef
            typename DataTypeForFormat<IntermediateDstFormat>::Type
            IntermediateDstType;

        const size_t NumElementsPerSrcTexel = NumElementsPerTexelForFormat<SrcFormat>();
        const size_t NumElementsPerDstTexel = NumElementsPerTexelForFormat<DstFormat>();
        const size_t MaxElementsPerTexel = 4;
        NS_ABORT_IF_FALSE(NumElementsPerSrcTexel <= MaxElementsPerTexel, "unhandled format");
        NS_ABORT_IF_FALSE(NumElementsPerDstTexel <= MaxElementsPerTexel, "unhandled format");

        // we assume that the strides are multiples of the sizeof of respective types.
        // this assumption will allow us to iterate over src and dst images using typed
        // pointers, e.g. uint8_t* or uint16_t* or float*, instead of untyped pointers.
        // So this assumption allows us to write cleaner and safer code, but it might
        // not be true forever and if it eventually becomes wrong, we'll have to revert
        // to always iterating using uint8_t* pointers regardless of the types at hand.
        NS_ABORT_IF_FALSE(mSrcStride % sizeof(SrcType) == 0 &&
                          mDstStride % sizeof(DstType) == 0,
                          "Unsupported: texture stride is not a multiple of sizeof(type)");
        const ptrdiff_t srcStrideInElements = mSrcStride / sizeof(SrcType);
        const ptrdiff_t dstStrideInElements = mDstStride / sizeof(DstType);

        const SrcType *srcRowStart = static_cast<const SrcType*>(mSrcStart);
        DstType *dstRowStart = static_cast<DstType*>(mDstStart);

        // the loop performing the texture format conversion
        for (size_t i = 0; i < mHeight; ++i) {
            const SrcType *srcRowEnd = srcRowStart + mWidth * NumElementsPerSrcTexel;
            const SrcType *srcPtr = srcRowStart;
            DstType *dstPtr = dstRowStart;
            while (srcPtr != srcRowEnd) {
                // convert a single texel. We proceed in 3 steps: unpack the source texel
                // so the corresponding interchange format (e.g. unpack RGB565 to RGBA8),
                // convert the resulting data type to the destination type (e.g. convert
                // from RGBA8 to RGBA32F), and finally pack the destination texel
                // (e.g. pack RGBA32F to RGB32F).
                IntermediateSrcType unpackedSrc[MaxElementsPerTexel];
                IntermediateDstType unpackedDst[MaxElementsPerTexel];

                // unpack a src texel to corresponding intermediate src format.
                // for example, unpack RGB565 to RGBA8
                unpack<SrcFormat>(srcPtr, unpackedSrc);
                // convert the data type to the destination type, if needed.
                // for example, convert RGBA8 to RGBA32F
                convertType(unpackedSrc, unpackedDst);
                // pack the destination texel.
                // for example, pack RGBA32F to RGB32F
                pack<DstFormat, PremultiplicationOp>(unpackedDst, dstPtr);

                srcPtr += NumElementsPerSrcTexel;
                dstPtr += NumElementsPerDstTexel;
            }
            srcRowStart += srcStrideInElements;
            dstRowStart += dstStrideInElements;
        }

        mSuccess = true;
        return;
    }

    template<WebGLTexelFormat SrcFormat, WebGLTexelFormat DstFormat>
    void run(WebGLTexelPremultiplicationOp premultiplicationOp)
    {
        #define WEBGLIMAGECONVERTER_CASE_PREMULTIPLICATIONOP(PremultiplicationOp) \
            case PremultiplicationOp: \
                return run<SrcFormat, DstFormat, PremultiplicationOp>();

        switch (premultiplicationOp) {
            WEBGLIMAGECONVERTER_CASE_PREMULTIPLICATIONOP(NoPremultiplicationOp)
            WEBGLIMAGECONVERTER_CASE_PREMULTIPLICATIONOP(Premultiply)
            WEBGLIMAGECONVERTER_CASE_PREMULTIPLICATIONOP(Unpremultiply)
            default:
                NS_ABORT_IF_FALSE(false, "unhandled case. Coding mistake?");
        }

        #undef WEBGLIMAGECONVERTER_CASE_PREMULTIPLICATIONOP
    }

    template<WebGLTexelFormat SrcFormat>
    void run(WebGLTexelFormat dstFormat,
             WebGLTexelPremultiplicationOp premultiplicationOp)
    {
        #define WEBGLIMAGECONVERTER_CASE_DSTFORMAT(DstFormat) \
            case DstFormat: \
                return run<SrcFormat, DstFormat>(premultiplicationOp);

        switch (dstFormat) {
            WEBGLIMAGECONVERTER_CASE_DSTFORMAT(R8)
            WEBGLIMAGECONVERTER_CASE_DSTFORMAT(A8)
            WEBGLIMAGECONVERTER_CASE_DSTFORMAT(R32F)
            WEBGLIMAGECONVERTER_CASE_DSTFORMAT(A32F)
            WEBGLIMAGECONVERTER_CASE_DSTFORMAT(RA8)
            WEBGLIMAGECONVERTER_CASE_DSTFORMAT(RA32F)
            WEBGLIMAGECONVERTER_CASE_DSTFORMAT(RGB8)
            WEBGLIMAGECONVERTER_CASE_DSTFORMAT(RGB565)
            WEBGLIMAGECONVERTER_CASE_DSTFORMAT(RGB32F)
            WEBGLIMAGECONVERTER_CASE_DSTFORMAT(RGBA8)
            WEBGLIMAGECONVERTER_CASE_DSTFORMAT(RGBA5551)
            WEBGLIMAGECONVERTER_CASE_DSTFORMAT(RGBA4444)
            WEBGLIMAGECONVERTER_CASE_DSTFORMAT(RGBA32F)
            default:
                NS_ABORT_IF_FALSE(false, "unhandled case. Coding mistake?");
        }

        #undef WEBGLIMAGECONVERTER_CASE_DSTFORMAT
    }

public:

    void run(WebGLTexelFormat srcFormat,
             WebGLTexelFormat dstFormat,
             WebGLTexelPremultiplicationOp premultiplicationOp)
    {
        #define WEBGLIMAGECONVERTER_CASE_SRCFORMAT(SrcFormat) \
            case SrcFormat: \
                return run<SrcFormat>(dstFormat, premultiplicationOp);

        switch (srcFormat) {
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(R8)
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(A8)
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(R32F)
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(A32F)
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(RA8)
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(RA32F)
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(RGB8)
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(BGRX8) // source format only
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(RGB565)
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(RGB32F)
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(RGBA8)
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(BGRA8)
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(RGBA5551)
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(RGBA4444)
            WEBGLIMAGECONVERTER_CASE_SRCFORMAT(RGBA32F)
            default:
                NS_ABORT_IF_FALSE(false, "unhandled case. Coding mistake?");
        }

        #undef WEBGLIMAGECONVERTER_CASE_SRCFORMAT
    }

    WebGLImageConverter(size_t width, size_t height,
                        const void* srcStart, void* dstStart,
                        ptrdiff_t srcStride, ptrdiff_t dstStride)
        : mWidth(width), mHeight(height),
          mSrcStart(srcStart), mDstStart(dstStart),
          mSrcStride(srcStride), mDstStride(dstStride),
          mAlreadyRun(false), mSuccess(false)
    {}

    bool Success() const {
        return mSuccess;
    }
};

} // end anonymous namespace

void
WebGLContext::ConvertImage(size_t width, size_t height, size_t srcStride, size_t dstStride,
                           const uint8_t* src, uint8_t *dst,
                           WebGLTexelFormat srcFormat, bool srcPremultiplied,
                           WebGLTexelFormat dstFormat, bool dstPremultiplied,
                           size_t dstTexelSize)
{
    if (width <= 0 || height <= 0)
        return;

    const bool FormatsRequireNoPremultiplicationOp =
        !HasAlpha(srcFormat) ||
        !HasColor(srcFormat) ||
        !HasColor(dstFormat);

    if (srcFormat == dstFormat &&
        (FormatsRequireNoPremultiplicationOp || srcPremultiplied == dstPremultiplied))
    {
        // fast exit path: we just have to memcpy all the rows.
        //
        // The case where absolutely nothing needs to be done is supposed to have
        // been handled earlier (in TexImage2D_base, etc).
        //
        // So the case we're handling here is when even though no format conversion is needed,
        // we still might have to flip vertically and/or to adjust to a different stride.

        NS_ABORT_IF_FALSE(mPixelStoreFlipY || srcStride != dstStride, "Performance trap -- should handle this case earlier, to avoid memcpy");

        size_t row_size = width * dstTexelSize; // doesn't matter, src and dst formats agree
        const uint8_t* ptr = src;
        const uint8_t* src_end = src + height * srcStride;

        uint8_t* dst_row = mPixelStoreFlipY
                           ? dst + (height-1) * dstStride
                           : dst;
        ptrdiff_t dstStrideSigned(dstStride);
        ptrdiff_t dst_delta = mPixelStoreFlipY ? -dstStrideSigned : dstStrideSigned;

        while(ptr != src_end) {
            memcpy(dst_row, ptr, row_size);
            ptr += srcStride;
            dst_row += dst_delta;
        }
        return;
    }

    uint8_t* dstStart = dst;
    ptrdiff_t signedDstStride = dstStride;
    if (mPixelStoreFlipY) {
        dstStart = dst + (height - 1) * dstStride;
        signedDstStride = -dstStride;
    }

    WebGLImageConverter converter(width, height, src, dstStart, srcStride, signedDstStride);

    const WebGLTexelPremultiplicationOp premultiplicationOp
        = FormatsRequireNoPremultiplicationOp     ? NoPremultiplicationOp
        : (!srcPremultiplied && dstPremultiplied) ? Premultiply
        : (srcPremultiplied && !dstPremultiplied) ? Unpremultiply
                                                  : NoPremultiplicationOp;

    converter.run(srcFormat, dstFormat, premultiplicationOp);

    if (!converter.Success()) {
        // the dst image may be left uninitialized, so we better not try to
        // continue even in release builds. This should never happen anyway,
        // and would be a bug in our code.
        NS_RUNTIMEABORT("programming mistake in WebGL texture conversions");
    }
}

} // end namespace mozilla 
