/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*  Defines the android-specific types and functions as part of npapi

    In particular, defines the window and event types that are passed to
    NPN_GetValue, NPP_SetWindow and NPP_HandleEvent.

    To minimize what native libraries the plugin links against, some
    functionality is provided via function-ptrs (e.g. time, sound)
 */

#ifndef android_npapi_H
#define android_npapi_H

#include <stdint.h>
#include "npapi.h"
#include <jni.h>

///////////////////////////////////////////////////////////////////////////////
// General types

enum ANPBitmapFormats {
    kUnknown_ANPBitmapFormat    = 0,
    kRGBA_8888_ANPBitmapFormat  = 1,
    kRGB_565_ANPBitmapFormat    = 2
};
typedef int32_t ANPBitmapFormat;

struct ANPPixelPacking {
    uint8_t AShift;
    uint8_t ABits;
    uint8_t RShift;
    uint8_t RBits;
    uint8_t GShift;
    uint8_t GBits;
    uint8_t BShift;
    uint8_t BBits;
};

struct ANPBitmap {
    void*           baseAddr;
    ANPBitmapFormat format;
    int32_t         width;
    int32_t         height;
    int32_t         rowBytes;
};

struct ANPRectF {
    float   left;
    float   top;
    float   right;
    float   bottom;
};

struct ANPRectI {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
};

struct ANPCanvas;
struct ANPMatrix;
struct ANPPaint;
struct ANPPath;
struct ANPRegion;
struct ANPTypeface;

enum ANPMatrixFlags {
    kIdentity_ANPMatrixFlag     = 0,
    kTranslate_ANPMatrixFlag    = 0x01,
    kScale_ANPMatrixFlag        = 0x02,
    kAffine_ANPMatrixFlag       = 0x04,
    kPerspective_ANPMatrixFlag  = 0x08,
};
typedef uint32_t ANPMatrixFlag;

///////////////////////////////////////////////////////////////////////////////
// NPN_GetValue

/** queries for a specific ANPInterface.

    Maybe called with NULL for the NPP instance

    NPN_GetValue(inst, interface_enum, ANPInterface*)
 */
#define kLogInterfaceV0_ANPGetValue         ((NPNVariable)1000)
#define kAudioTrackInterfaceV0_ANPGetValue  ((NPNVariable)1001)
#define kCanvasInterfaceV0_ANPGetValue      ((NPNVariable)1002)
#define kMatrixInterfaceV0_ANPGetValue      ((NPNVariable)1003)
#define kPaintInterfaceV0_ANPGetValue       ((NPNVariable)1004)
#define kPathInterfaceV0_ANPGetValue        ((NPNVariable)1005)
#define kTypefaceInterfaceV0_ANPGetValue    ((NPNVariable)1006)
#define kWindowInterfaceV0_ANPGetValue      ((NPNVariable)1007)
#define kBitmapInterfaceV0_ANPGetValue      ((NPNVariable)1008)
#define kSurfaceInterfaceV0_ANPGetValue     ((NPNVariable)1009)
#define kSystemInterfaceV0_ANPGetValue      ((NPNVariable)1010)
#define kEventInterfaceV0_ANPGetValue       ((NPNVariable)1011)

/** queries for the drawing models supported on this device.

    NPN_GetValue(inst, kSupportedDrawingModel_ANPGetValue, uint32_t* bits)
 */
#define kSupportedDrawingModel_ANPGetValue  ((NPNVariable)2000)

/** queries for the context (android.content.Context) of the plugin. If no
    instance is specified the application's context is returned. If the instance
    is given then the context returned is identical to the context used to
    create the webview in which that instance resides.

    NOTE: Holding onto a non-application context after your instance has been
    destroyed will cause a memory leak.  Refer to the android documentation to
    determine what context is best suited for your particular scenario.

    NPN_GetValue(inst, kJavaContext_ANPGetValue, jobject context)
 */
#define kJavaContext_ANPGetValue            ((NPNVariable)2001)

///////////////////////////////////////////////////////////////////////////////
// NPN_SetValue

/** Request to set the drawing model. SetValue will return false if the drawing
    model is not supported or has insufficient information for configuration.

    NPN_SetValue(inst, kRequestDrawingModel_ANPSetValue, (void*)foo_ANPDrawingModel)
 */
#define kRequestDrawingModel_ANPSetValue    ((NPPVariable)1000)

/** These are used as bitfields in ANPSupportedDrawingModels_EnumValue,
    and as-is in ANPRequestDrawingModel_EnumValue. The drawing model determines
    how to interpret the ANPDrawingContext provided in the Draw event and how
    to interpret the NPWindow->window field.
 */
enum ANPDrawingModels {
    /** Draw into a bitmap from the browser thread in response to a Draw event.
        NPWindow->window is reserved (ignore)
     */
    kBitmap_ANPDrawingModel  = 1 << 0,
    /** Draw into a surface (e.g. raster, openGL, etc.) using the Java surface
        interface. When this model is used the browser will invoke the Java
        class specified in the plugin's apk manifest. From that class the browser
        will invoke the appropriate method to return an an instance of a android
        Java View. The instance is then embedded in the html. The plugin can then
        manipulate the view as it would any normal Java View in android.

        Unlike the bitmap model, a surface model is opaque so no html content
        behind the plugin will be  visible. Unless the plugin needs to be
        transparent the surface model should be chosen over the bitmap model as
        it will have better performance.

        Further, a plugin can manipulate some surfaces in native code using the
        ANPSurfaceInterface.  This interface can be used to manipulate Java
        objects that extend Surface.class by allowing them to access the
        surface's underlying bitmap in native code.  For instance, if a raster
        surface is used the plugin can lock, draw directly into the bitmap, and
        unlock the surface in native code without making JNI calls to the Java
        surface object.
     */
    kSurface_ANPDrawingModel = 1 << 1,
};
typedef int32_t ANPDrawingModel;

/** Request to receive/disable events. If the pointer is NULL then all flags will
    be disabled. Otherwise, the event type will be enabled iff its corresponding
    bit in the EventFlags bit field is set.

    NPN_SetValue(inst, ANPAcceptEvents, (void*)EventFlags)
 */
#define kAcceptEvents_ANPSetValue           ((NPPVariable)1001)

/** The EventFlags are a set of bits used to determine which types of events the
    plugin wishes to receive. For example, if the value is 0x03 then both key
    and touch events will be provided to the plugin.
 */
enum ANPEventFlag {
    kKey_ANPEventFlag               = 0x01,
    kTouch_ANPEventFlag             = 0x02,
};
typedef uint32_t ANPEventFlags;

///////////////////////////////////////////////////////////////////////////////
// NPP_GetValue

/** Requests that the plugin return a java surface to be displayed. This will
    only be used if the plugin has choosen the kSurface_ANPDrawingModel.

    NPP_GetValue(inst, kJavaSurface_ANPGetValue, jobject surface)
 */
#define kJavaSurface_ANPGetValue            ((NPPVariable)2000)


///////////////////////////////////////////////////////////////////////////////
// ANDROID INTERFACE DEFINITIONS

/** Interfaces provide additional functionality to the plugin via function ptrs.
    Once an interface is retrieved, it is valid for the lifetime of the plugin
    (just like browserfuncs).

    All ANPInterfaces begin with an inSize field, which must be set by the
    caller (plugin) with the number of bytes allocated for the interface.
    e.g. SomeInterface si; si.inSize = sizeof(si); browser->getvalue(..., &si);
 */
struct ANPInterface {
    uint32_t    inSize;     // size (in bytes) of this struct
};

enum ANPLogTypes {
    kError_ANPLogType   = 0,    // error
    kWarning_ANPLogType = 1,    // warning
    kDebug_ANPLogType   = 2     // debug only (informational)
};
typedef int32_t ANPLogType;

struct ANPLogInterfaceV0 : ANPInterface {
    /** dumps printf messages to the log file
        e.g. interface->log(instance, kWarning_ANPLogType, "value is %d", value);
     */
    void (*log)(ANPLogType, const char format[], ...);
};

struct ANPBitmapInterfaceV0 : ANPInterface {
    /** Returns true if the specified bitmap format is supported, and if packing
        is non-null, sets it to the packing info for that format.
     */
    bool (*getPixelPacking)(ANPBitmapFormat, ANPPixelPacking* packing);
};

struct ANPMatrixInterfaceV0 : ANPInterface {
    /** Return a new identity matrix
     */
    ANPMatrix*  (*newMatrix)();
    /** Delete a matrix previously allocated by newMatrix()
     */
    void        (*deleteMatrix)(ANPMatrix*);

    ANPMatrixFlag (*getFlags)(const ANPMatrix*);

    void        (*copy)(ANPMatrix* dst, const ANPMatrix* src);

    /** Return the matrix values in a float array (allcoated by the caller),
        where the values are treated as follows:
        w  = x * [6] + y * [7] + [8];
        x' = (x * [0] + y * [1] + [2]) / w;
        y' = (x * [3] + y * [4] + [5]) / w;
     */
    void        (*get3x3)(const ANPMatrix*, float[9]);
    /** Initialize the matrix from values in a float array,
        where the values are treated as follows:
         w  = x * [6] + y * [7] + [8];
         x' = (x * [0] + y * [1] + [2]) / w;
         y' = (x * [3] + y * [4] + [5]) / w;
     */
    void        (*set3x3)(ANPMatrix*, const float[9]);

    void        (*setIdentity)(ANPMatrix*);
    void        (*preTranslate)(ANPMatrix*, float tx, float ty);
    void        (*postTranslate)(ANPMatrix*, float tx, float ty);
    void        (*preScale)(ANPMatrix*, float sx, float sy);
    void        (*postScale)(ANPMatrix*, float sx, float sy);
    void        (*preSkew)(ANPMatrix*, float kx, float ky);
    void        (*postSkew)(ANPMatrix*, float kx, float ky);
    void        (*preRotate)(ANPMatrix*, float degrees);
    void        (*postRotate)(ANPMatrix*, float degrees);
    void        (*preConcat)(ANPMatrix*, const ANPMatrix*);
    void        (*postConcat)(ANPMatrix*, const ANPMatrix*);

    /** Return true if src is invertible, and if so, return its inverse in dst.
        If src is not invertible, return false and ignore dst.
     */
    bool        (*invert)(ANPMatrix* dst, const ANPMatrix* src);

    /** Transform the x,y pairs in src[] by this matrix, and store the results
        in dst[]. The count parameter is treated as the number of pairs in the
        array. It is legal for src and dst to point to the same memory, but
        illegal for the two arrays to partially overlap.
     */
    void        (*mapPoints)(ANPMatrix*, float dst[], const float src[],
                             int32_t count);
};

struct ANPPathInterfaceV0 : ANPInterface {
    /** Return a new path */
    ANPPath* (*newPath)();

    /** Delete a path previously allocated by ANPPath() */
    void (*deletePath)(ANPPath*);

    /** Make a deep copy of the src path, into the dst path (already allocated
        by the caller).
     */
    void (*copy)(ANPPath* dst, const ANPPath* src);

    /** Returns true if the two paths are the same (i.e. have the same points)
     */
    bool (*equal)(const ANPPath* path0, const ANPPath* path1);

    /** Remove any previous points, initializing the path back to empty. */
    void (*reset)(ANPPath*);

    /** Return true if the path is empty (has no lines, quads or cubics). */
    bool (*isEmpty)(const ANPPath*);

    /** Return the path's bounds in bounds. */
    void (*getBounds)(const ANPPath*, ANPRectF* bounds);

    void (*moveTo)(ANPPath*, float x, float y);
    void (*lineTo)(ANPPath*, float x, float y);
    void (*quadTo)(ANPPath*, float x0, float y0, float x1, float y1);
    void (*cubicTo)(ANPPath*, float x0, float y0, float x1, float y1,
                    float x2, float y2);
    void (*close)(ANPPath*);

    /** Offset the src path by [dx, dy]. If dst is null, apply the
        change directly to the src path. If dst is not null, write the
        changed path into dst, and leave the src path unchanged. In that case
        dst must have been previously allocated by the caller.
     */
    void (*offset)(ANPPath* src, float dx, float dy, ANPPath* dst);

    /** Transform the path by the matrix. If dst is null, apply the
        change directly to the src path. If dst is not null, write the
        changed path into dst, and leave the src path unchanged. In that case
        dst must have been previously allocated by the caller.
     */
    void (*transform)(ANPPath* src, const ANPMatrix*, ANPPath* dst);
};

/** ANPColor is always defined to have the same packing on all platforms, and
    it is always unpremultiplied.

    This is in contrast to 32bit format(s) in bitmaps, which are premultiplied,
    and their packing may vary depending on the platform, hence the need for
    ANPBitmapInterface::getPixelPacking()
 */
typedef uint32_t ANPColor;
#define ANPColor_ASHIFT     24
#define ANPColor_RSHIFT     16
#define ANPColor_GSHIFT     8
#define ANPColor_BSHIFT     0
#define ANP_MAKE_COLOR(a, r, g, b)  \
                   (((a) << ANPColor_ASHIFT) |  \
                    ((r) << ANPColor_RSHIFT) |  \
                    ((g) << ANPColor_GSHIFT) |  \
                    ((b) << ANPColor_BSHIFT))

enum ANPPaintFlag {
    kAntiAlias_ANPPaintFlag         = 1 << 0,
    kFilterBitmap_ANPPaintFlag      = 1 << 1,
    kDither_ANPPaintFlag            = 1 << 2,
    kUnderlineText_ANPPaintFlag     = 1 << 3,
    kStrikeThruText_ANPPaintFlag    = 1 << 4,
    kFakeBoldText_ANPPaintFlag      = 1 << 5,
};
typedef uint32_t ANPPaintFlags;

enum ANPPaintStyles {
    kFill_ANPPaintStyle             = 0,
    kStroke_ANPPaintStyle           = 1,
    kFillAndStroke_ANPPaintStyle    = 2
};
typedef int32_t ANPPaintStyle;

enum ANPPaintCaps {
    kButt_ANPPaintCap   = 0,
    kRound_ANPPaintCap  = 1,
    kSquare_ANPPaintCap = 2
};
typedef int32_t ANPPaintCap;

enum ANPPaintJoins {
    kMiter_ANPPaintJoin = 0,
    kRound_ANPPaintJoin = 1,
    kBevel_ANPPaintJoin = 2
};
typedef int32_t ANPPaintJoin;

enum ANPPaintAligns {
    kLeft_ANPPaintAlign     = 0,
    kCenter_ANPPaintAlign   = 1,
    kRight_ANPPaintAlign    = 2
};
typedef int32_t ANPPaintAlign;

enum ANPTextEncodings {
    kUTF8_ANPTextEncoding   = 0,
    kUTF16_ANPTextEncoding  = 1,
};
typedef int32_t ANPTextEncoding;

enum ANPTypefaceStyles {
    kBold_ANPTypefaceStyle      = 1 << 0,
    kItalic_ANPTypefaceStyle    = 1 << 1
};
typedef uint32_t ANPTypefaceStyle;

typedef uint32_t ANPFontTableTag;

struct ANPFontMetrics {
    /** The greatest distance above the baseline for any glyph (will be <= 0) */
    float   fTop;
    /** The recommended distance above the baseline (will be <= 0) */
    float   fAscent;
    /** The recommended distance below the baseline (will be >= 0) */
    float   fDescent;
    /** The greatest distance below the baseline for any glyph (will be >= 0) */
    float   fBottom;
    /** The recommended distance to add between lines of text (will be >= 0) */
    float   fLeading;
};

struct ANPTypefaceInterfaceV0 : ANPInterface {
    /** Return a new reference to the typeface that most closely matches the
        requested name and style. Pass null as the name to return
        the default font for the requested style. Will never return null

        The 5 generic font names "serif", "sans-serif", "monospace", "cursive",
        "fantasy" are recognized, and will be mapped to their logical font
        automatically by this call.

        @param name     May be NULL. The name of the font family.
        @param style    The style (normal, bold, italic) of the typeface.
        @return reference to the closest-matching typeface. Caller must call
                unref() when they are done with the typeface.
     */
    ANPTypeface* (*createFromName)(const char name[], ANPTypefaceStyle);

    /** Return a new reference to the typeface that most closely matches the
        requested typeface and specified Style. Use this call if you want to
        pick a new style from the same family of the existing typeface.
        If family is NULL, this selects from the default font's family.

        @param family  May be NULL. The name of the existing type face.
        @param s       The style (normal, bold, italic) of the type face.
        @return reference to the closest-matching typeface. Call must call
                unref() when they are done.
     */
    ANPTypeface* (*createFromTypeface)(const ANPTypeface* family,
                                       ANPTypefaceStyle);

    /** Return the owner count of the typeface. A newly created typeface has an
        owner count of 1. When the owner count is reaches 0, the typeface is
        deleted.
     */
    int32_t (*getRefCount)(const ANPTypeface*);

    /** Increment the owner count on the typeface
     */
    void (*ref)(ANPTypeface*);

    /** Decrement the owner count on the typeface. When the count goes to 0,
        the typeface is deleted.
     */
    void (*unref)(ANPTypeface*);

    /** Return the style bits for the specified typeface
     */
    ANPTypefaceStyle (*getStyle)(const ANPTypeface*);

    /** Some fonts are stored in files. If that is true for the fontID, then
        this returns the byte length of the full file path. If path is not null,
        then the full path is copied into path (allocated by the caller), up to
        length bytes. If index is not null, then it is set to the truetype
        collection index for this font, or 0 if the font is not in a collection.

        Note: getFontPath does not assume that path is a null-terminated string,
        so when it succeeds, it only copies the bytes of the file name and
        nothing else (i.e. it copies exactly the number of bytes returned by the
        function. If the caller wants to treat path[] as a C string, it must be
        sure that it is allocated at least 1 byte larger than the returned size,
        and it must copy in the terminating 0.

        If the fontID does not correspond to a file, then the function returns
        0, and the path and index parameters are ignored.

        @param fontID  The font whose file name is being queried
        @param path    Either NULL, or storage for receiving up to length bytes
                       of the font's file name. Allocated by the caller.
        @param length  The maximum space allocated in path (by the caller).
                       Ignored if path is NULL.
        @param index   Either NULL, or receives the TTC index for this font.
                       If the font is not a TTC, then will be set to 0.
        @return The byte length of th font's file name, or 0 if the font is not
                baked by a file.
     */
    int32_t (*getFontPath)(const ANPTypeface*, char path[], int32_t length,
                           int32_t* index);

    /** Return a UTF8 encoded path name for the font directory, or NULL if not
        supported. If returned, this string address will be valid for the life
        of the plugin instance. It will always end with a '/' character.
     */
    const char* (*getFontDirectoryPath)();
};

struct ANPPaintInterfaceV0 : ANPInterface {
    /** Return a new paint object, which holds all of the color and style
        attributes that affect how things (geometry, text, bitmaps) are drawn
        in a ANPCanvas.

        The paint that is returned is not tied to any particular plugin
        instance, but it must only be accessed from one thread at a time.
     */
    ANPPaint*   (*newPaint)();
    void        (*deletePaint)(ANPPaint*);

    ANPPaintFlags (*getFlags)(const ANPPaint*);
    void        (*setFlags)(ANPPaint*, ANPPaintFlags);

    ANPColor    (*getColor)(const ANPPaint*);
    void        (*setColor)(ANPPaint*, ANPColor);

    ANPPaintStyle (*getStyle)(const ANPPaint*);
    void        (*setStyle)(ANPPaint*, ANPPaintStyle);

    float       (*getStrokeWidth)(const ANPPaint*);
    float       (*getStrokeMiter)(const ANPPaint*);
    ANPPaintCap (*getStrokeCap)(const ANPPaint*);
    ANPPaintJoin (*getStrokeJoin)(const ANPPaint*);
    void        (*setStrokeWidth)(ANPPaint*, float);
    void        (*setStrokeMiter)(ANPPaint*, float);
    void        (*setStrokeCap)(ANPPaint*, ANPPaintCap);
    void        (*setStrokeJoin)(ANPPaint*, ANPPaintJoin);

    ANPTextEncoding (*getTextEncoding)(const ANPPaint*);
    ANPPaintAlign (*getTextAlign)(const ANPPaint*);
    float       (*getTextSize)(const ANPPaint*);
    float       (*getTextScaleX)(const ANPPaint*);
    float       (*getTextSkewX)(const ANPPaint*);
    void        (*setTextEncoding)(ANPPaint*, ANPTextEncoding);
    void        (*setTextAlign)(ANPPaint*, ANPPaintAlign);
    void        (*setTextSize)(ANPPaint*, float);
    void        (*setTextScaleX)(ANPPaint*, float);
    void        (*setTextSkewX)(ANPPaint*, float);

    /** Return the typeface ine paint, or null if there is none. This does not
        modify the owner count of the returned typeface.
     */
    ANPTypeface* (*getTypeface)(const ANPPaint*);

    /** Set the paint's typeface. If the paint already had a non-null typeface,
        its owner count is decremented. If the new typeface is non-null, its
        owner count is incremented.
     */
    void (*setTypeface)(ANPPaint*, ANPTypeface*);

    /** Return the width of the text. If bounds is not null, return the bounds
        of the text in that rectangle.
     */
    float (*measureText)(ANPPaint*, const void* text, uint32_t byteLength,
                         ANPRectF* bounds);

    /** Return the number of unichars specifed by the text.
        If widths is not null, returns the array of advance widths for each
            unichar.
        If bounds is not null, returns the array of bounds for each unichar.
     */
    int (*getTextWidths)(ANPPaint*, const void* text, uint32_t byteLength,
                         float widths[], ANPRectF bounds[]);

    /** Return in metrics the spacing values for text, respecting the paint's
        typeface and pointsize, and return the spacing between lines
        (descent - ascent + leading). If metrics is NULL, it will be ignored.
     */
    float (*getFontMetrics)(ANPPaint*, ANPFontMetrics* metrics);
};

struct ANPCanvasInterfaceV0 : ANPInterface {
    /** Return a canvas that will draw into the specified bitmap. Note: the
        canvas copies the fields of the bitmap, so it need not persist after
        this call, but the canvas DOES point to the same pixel memory that the
        bitmap did, so the canvas should not be used after that pixel memory
        goes out of scope. In the case of creating a canvas to draw into the
        pixels provided by kDraw_ANPEventType, those pixels are only while
        handling that event.

        The canvas that is returned is not tied to any particular plugin
        instance, but it must only be accessed from one thread at a time.
     */
    ANPCanvas*  (*newCanvas)(const ANPBitmap*);
    void        (*deleteCanvas)(ANPCanvas*);

    void        (*save)(ANPCanvas*);
    void        (*restore)(ANPCanvas*);
    void        (*translate)(ANPCanvas*, float tx, float ty);
    void        (*scale)(ANPCanvas*, float sx, float sy);
    void        (*rotate)(ANPCanvas*, float degrees);
    void        (*skew)(ANPCanvas*, float kx, float ky);
    void        (*concat)(ANPCanvas*, const ANPMatrix*);
    void        (*clipRect)(ANPCanvas*, const ANPRectF*);
    void        (*clipPath)(ANPCanvas*, const ANPPath*);

    /** Return the current matrix on the canvas
     */
    void        (*getTotalMatrix)(ANPCanvas*, ANPMatrix*);
    /** Return the current clip bounds in local coordinates, expanding it to
        account for antialiasing edge effects if aa is true. If the
        current clip is empty, return false and ignore the bounds argument.
     */
    bool        (*getLocalClipBounds)(ANPCanvas*, ANPRectF* bounds, bool aa);
    /** Return the current clip bounds in device coordinates in bounds. If the
        current clip is empty, return false and ignore the bounds argument.
     */
    bool        (*getDeviceClipBounds)(ANPCanvas*, ANPRectI* bounds);

    void        (*drawColor)(ANPCanvas*, ANPColor);
    void        (*drawPaint)(ANPCanvas*, const ANPPaint*);
    void        (*drawLine)(ANPCanvas*, float x0, float y0, float x1, float y1,
                            const ANPPaint*);
    void        (*drawRect)(ANPCanvas*, const ANPRectF*, const ANPPaint*);
    void        (*drawOval)(ANPCanvas*, const ANPRectF*, const ANPPaint*);
    void        (*drawPath)(ANPCanvas*, const ANPPath*, const ANPPaint*);
    void        (*drawText)(ANPCanvas*, const void* text, uint32_t byteLength,
                            float x, float y, const ANPPaint*);
    void       (*drawPosText)(ANPCanvas*, const void* text, uint32_t byteLength,
                               const float xy[], const ANPPaint*);
    void        (*drawBitmap)(ANPCanvas*, const ANPBitmap*, float x, float y,
                              const ANPPaint*);
    void        (*drawBitmapRect)(ANPCanvas*, const ANPBitmap*,
                                  const ANPRectI* src, const ANPRectF* dst,
                                  const ANPPaint*);
};

struct ANPWindowInterfaceV0 : ANPInterface {
    /** Registers a set of rectangles that the plugin would like to keep on
        screen. The rectangles are listed in order of priority with the highest
        priority rectangle in location rects[0].  The browser will attempt to keep
        as many of the rectangles on screen as possible and will scroll them into
        view in response to the invocation of this method and other various events.
        The count specifies how many rectangles are in the array. If the count is
        zero it signals the browser that any existing rectangles should be cleared
        and no rectangles will be tracked.
     */
    void (*setVisibleRects)(NPP instance, const ANPRectI rects[], int32_t count);
    /** Clears any rectangles that are being tracked as a result of a call to
        setVisibleRects. This call is equivalent to setVisibleRect(inst, NULL, 0).
     */
    void    (*clearVisibleRects)(NPP instance);
    /** Given a boolean value of true the device will be requested to provide
        a keyboard. A value of false will result in a request to hide the
        keyboard. Further, the on-screen keyboard will not be displayed if a
        physical keyboard is active.
     */
    void    (*showKeyboard)(NPP instance, bool value);
    /** Called when a plugin wishes to enter into full screen mode. The plugin's
        Java class (defined in the plugin's apk manifest) will be called
        asynchronously to provide a View object to be displayed full screen.
     */
    void    (*requestFullScreen)(NPP instance);
    /** Called when a plugin wishes to exit from full screen mode. As a result,
        the plugin's full screen view will be discarded by the view system.
     */
    void    (*exitFullScreen)(NPP instance);
    /** Called when a plugin wishes to be zoomed and centered in the current view.
     */
    void    (*requestCenterFitZoom)(NPP instance);
};

///////////////////////////////////////////////////////////////////////////////

enum ANPSampleFormats {
    kUnknown_ANPSamleFormat     = 0,
    kPCM16Bit_ANPSampleFormat   = 1,
    kPCM8Bit_ANPSampleFormat    = 2
};
typedef int32_t ANPSampleFormat;

/** The audio buffer is passed to the callback proc to request more samples.
    It is owned by the system, and the callback may read it, but should not
    maintain a pointer to it outside of the scope of the callback proc.
 */
struct ANPAudioBuffer {
    // RO - repeat what was specified in newTrack()
    int32_t     channelCount;
    // RO - repeat what was specified in newTrack()
    ANPSampleFormat  format;
    /** This buffer is owned by the caller. Inside the callback proc, up to
        "size" bytes of sample data should be written into this buffer. The
        address is only valid for the scope of a single invocation of the
        callback proc.
     */
    void*       bufferData;
    /** On input, specifies the maximum number of bytes that can be written
        to "bufferData". On output, specifies the actual number of bytes that
        the callback proc wrote into "bufferData".
     */
    uint32_t    size;
};

enum ANPAudioEvents {
    /** This event is passed to the callback proc when the audio-track needs
        more sample data written to the provided buffer parameter.
     */
    kMoreData_ANPAudioEvent = 0,
    /** This event is passed to the callback proc if the audio system runs out
        of sample data. In this event, no buffer parameter will be specified
        (i.e. NULL will be passed to the 3rd parameter).
     */
    kUnderRun_ANPAudioEvent = 1
};
typedef int32_t ANPAudioEvent;

/** Called to feed sample data to the track. This will be called in a separate
    thread. However, you may call trackStop() from the callback (but you
    cannot delete the track).

    For example, when you have written the last chunk of sample data, you can
    immediately call trackStop(). This will take effect after the current
    buffer has been played.

    The "user" parameter is the same value that was passed to newTrack()
 */
typedef void (*ANPAudioCallbackProc)(ANPAudioEvent event, void* user,
                                     ANPAudioBuffer* buffer);

struct ANPAudioTrack;   // abstract type for audio tracks

struct ANPAudioTrackInterfaceV0 : ANPInterface {
    /** Create a new audio track, or NULL on failure. The track is initially in
        the stopped state and therefore ANPAudioCallbackProc will not be called
        until the track is started.
     */
    ANPAudioTrack*  (*newTrack)(uint32_t sampleRate,    // sampling rate in Hz
                                ANPSampleFormat,
                                int channelCount,       // MONO=1, STEREO=2
                                ANPAudioCallbackProc,
                                void* user);
    /** Deletes a track that was created using newTrack.  The track can be
        deleted in any state and it waits for the ANPAudioCallbackProc thread
        to exit before returning.
     */
    void (*deleteTrack)(ANPAudioTrack*);

    void (*start)(ANPAudioTrack*);
    void (*pause)(ANPAudioTrack*);
    void (*stop)(ANPAudioTrack*);
    /** Returns true if the track is not playing (e.g. pause or stop was called,
        or start was never called.
     */
    bool (*isStopped)(ANPAudioTrack*);
};

///////////////////////////////////////////////////////////////////////////////
// DEFINITION OF VALUES PASSED THROUGH NPP_HandleEvent

enum ANPEventTypes {
    kNull_ANPEventType          = 0,
    kKey_ANPEventType           = 1,
    /** Mouse events are triggered by either clicking with the navigational pad
        or by tapping the touchscreen (if the kDown_ANPTouchAction is handled by
        the plugin then no mouse event is generated).  The kKey_ANPEventFlag has
        to be set to true in order to receive these events.
     */
    kMouse_ANPEventType         = 2,
    /** Touch events are generated when the user touches on the screen. The
        kTouch_ANPEventFlag has to be set to true in order to receive these
        events.
     */
    kTouch_ANPEventType         = 3,
    /** Only triggered by a plugin using the kBitmap_ANPDrawingModel. This event
        signals that the plugin needs to redraw itself into the provided bitmap.
     */
    kDraw_ANPEventType          = 4,
    kLifecycle_ANPEventType     = 5,

    /** This event type is completely defined by the plugin.
        When creating an event, the caller must always set the first
        two fields, the remaining data is optional.
            ANPEvent evt;
            evt.inSize = sizeof(ANPEvent);
            evt.eventType = kCustom_ANPEventType
            // other data slots are optional
            evt.other[] = ...;
        To post a copy of the event, call
            eventInterface->postEvent(myNPPInstance, &evt);
        That call makes a copy of the event struct, and post that on the event
        queue for the plugin.
     */
    kCustom_ANPEventType   = 6,
};
typedef int32_t ANPEventType;

enum ANPKeyActions {
    kDown_ANPKeyAction  = 0,
    kUp_ANPKeyAction    = 1,
};
typedef int32_t ANPKeyAction;

#include "ANPKeyCodes.h"
typedef int32_t ANPKeyCode;

enum ANPKeyModifiers {
    kAlt_ANPKeyModifier     = 1 << 0,
    kShift_ANPKeyModifier   = 1 << 1,
};
// bit-field containing some number of ANPKeyModifier bits
typedef uint32_t ANPKeyModifier;

enum ANPMouseActions {
    kDown_ANPMouseAction  = 0,
    kUp_ANPMouseAction    = 1,
};
typedef int32_t ANPMouseAction;

enum ANPTouchActions {
    /** This occurs when the user first touches on the screen. As such, this
        action will always occur prior to any of the other touch actions. If
        the plugin chooses to not handle this action then no other events
        related to that particular touch gesture will be generated.
     */
    kDown_ANPTouchAction        = 0,
    kUp_ANPTouchAction          = 1,
    kMove_ANPTouchAction        = 2,
    kCancel_ANPTouchAction      = 3,
    // The web view will ignore the return value from the following actions
    kLongPress_ANPTouchAction   = 4,
    kDoubleTap_ANPTouchAction   = 5,
};
typedef int32_t ANPTouchAction;

enum ANPLifecycleActions {
    /** The web view containing this plugin has been paused.  See documentation
        on the android activity lifecycle for more information.
     */
    kPause_ANPLifecycleAction           = 0,
    /** The web view containing this plugin has been resumed. See documentation
        on the android activity lifecycle for more information.
     */
    kResume_ANPLifecycleAction          = 1,
    /** The plugin has focus and is now the recipient of input events (e.g. key,
        touch, etc.)
     */
    kGainFocus_ANPLifecycleAction       = 2,
    /** The plugin has lost focus and will not receive any input events until it
        regains focus. This event is always preceded by a GainFocus action.
     */
    kLoseFocus_ANPLifecycleAction       = 3,
    /** The browser is running low on available memory and is requesting that
        the plugin free any unused/inactive resources to prevent a performance
        degradation.
     */
    kFreeMemory_ANPLifecycleAction      = 4,
    /** The page has finished loading. This happens when the page's top level
        frame reports that it has completed loading.
     */
    kOnLoad_ANPLifecycleAction          = 5,
    /** The browser is honoring the plugin's request to go full screen. Upon
        returning from this event the browser will resize the plugin's java
        surface to full-screen coordinates.
     */
    kEnterFullScreen_ANPLifecycleAction = 6,
    /** The browser has exited from full screen mode. Immediately prior to
        sending this event the browser has resized the plugin's java surface to
        its original coordinates.
     */
    kExitFullScreen_ANPLifecycleAction  = 7,
    /** The plugin is visible to the user on the screen. This event will always
        occur after a kOffScreen_ANPLifecycleAction event.
     */
    kOnScreen_ANPLifecycleAction        = 8,
    /** The plugin is no longer visible to the user on the screen. This event
        will always occur prior to an kOnScreen_ANPLifecycleAction event.
     */
    kOffScreen_ANPLifecycleAction       = 9,
};
typedef uint32_t ANPLifecycleAction;

/* This is what is passed to NPP_HandleEvent() */
struct ANPEvent {
    uint32_t        inSize;  // size of this struct in bytes
    ANPEventType    eventType;
    // use based on the value in eventType
    union {
        struct {
            ANPKeyAction    action;
            ANPKeyCode      nativeCode;
            int32_t         virtualCode;    // windows virtual key code
            ANPKeyModifier  modifiers;
            int32_t         repeatCount;    // 0 for initial down (or up)
            int32_t         unichar;        // 0 if there is no value
        } key;
        struct {
            ANPMouseAction  action;
            int32_t         x;  // relative to your "window" (0...width)
            int32_t         y;  // relative to your "window" (0...height)
        } mouse;
        struct {
            ANPTouchAction  action;
            ANPKeyModifier  modifiers;
            int32_t         x;  // relative to your "window" (0...width)
            int32_t         y;  // relative to your "window" (0...height)
        } touch;
        struct {
            ANPLifecycleAction  action;
        } lifecycle;
        struct {
            ANPDrawingModel model;
            // relative to (0,0) in top-left of your plugin
            ANPRectI        clip;
            // use based on the value in model
            union {
                ANPBitmap   bitmap;
            } data;
        } draw;
        int32_t     other[8];
    } data;
};

struct ANPEventInterfaceV0 : ANPInterface {
    /** Post a copy of the specified event to the plugin. The event will be
        delivered to the plugin in its main thread (the thread that receives
        other ANPEvents). If, after posting before delivery, the NPP instance
        is torn down, the event will be discarded.
     */
    void (*postEvent)(NPP inst, const ANPEvent* event);
};

struct ANPSystemInterfaceV0 : ANPInterface {
    /** Return the path name for the current Application's plugin data directory,
        or NULL if not supported
     */
    const char* (*getApplicationDataDirectory)();

    /** A helper function to load java classes from the plugin's apk.  The
        function looks for a class given the fully qualified and null terminated
        string representing the className. For example,

        const char* className = "com.android.mypackage.MyClass";

        If the class cannot be found or there is a problem loading the class
        NULL will be returned.
     */
    jclass (*loadJavaClass)(NPP instance, const char* className);
};

struct ANPSurfaceInterfaceV0 : ANPInterface {
  /** Locks the surface from manipulation by other threads and provides a bitmap
        to be written to.  The dirtyRect param specifies which portion of the
        bitmap will be written to.  If the dirtyRect is NULL then the entire
        surface will be considered dirty.  If the lock was successful the function
        will return true and the bitmap will be set to point to a valid bitmap.
        If not the function will return false and the bitmap will be set to NULL.
  */
  bool (*lock)(JNIEnv* env, jobject surface, ANPBitmap* bitmap, ANPRectI* dirtyRect);
  /** Given a locked surface handle (i.e. result of a successful call to lock)
        the surface is unlocked and the contents of the bitmap, specifically
        those inside the dirtyRect are written to the screen.
  */
  void (*unlock)(JNIEnv* env, jobject surface);
};

typedef int32_t   int32;
typedef uint32_t uint32;
typedef int16_t   int16;
typedef uint16_t uint16;

#endif
