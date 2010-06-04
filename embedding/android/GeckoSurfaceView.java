/* -*- Mode: Java; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import java.io.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.locks.*;
import java.util.concurrent.atomic.*;
import java.util.zip.*;
import java.nio.*;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGL11;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;
import javax.microedition.khronos.opengles.GL;
import javax.microedition.khronos.opengles.GL10;

import android.os.*;
import android.app.*;
import android.text.*;
import android.view.*;
import android.view.inputmethod.*;
import android.content.*;
import android.graphics.*;
import android.widget.*;
import android.hardware.*;
import android.location.*;

import android.util.*;

/*
 * GeckoSurfaceView implements a GL surface view,
 * similar to GLSurfaceView.  However, since we
 * already have a thread for Gecko, we don't really want
 * a separate renderer thread that GLSurfaceView provides.
 */
class GeckoSurfaceView
    extends SurfaceView
    implements SurfaceHolder.Callback, SensorEventListener, LocationListener
{
    public GeckoSurfaceView(Context context) {
        super(context);

        getHolder().addCallback(this);
        inputConnection = new GeckoInputConnection(this);
        setFocusable(true);
        setFocusableInTouchMode(true);

        if (!GeckoApp.useSoftwareDrawing)
            startEgl();

        mWidth = 0;
        mHeight = 0;
        mBufferWidth = 0;
        mBufferHeight = 0;

        mSurfaceLock = new ReentrantLock();
    }

    protected void finalize() throws Throwable {
        super.finalize();
        if (!GeckoApp.useSoftwareDrawing)
            finishEgl();
    }

    private static final int EGL_OPENGL_ES2_BIT = 4;
    private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;

    private void printConfig(EGL10 egl, EGLDisplay display,
                             EGLConfig config) {
        int[] attributes = {
            EGL10.EGL_BUFFER_SIZE,
            EGL10.EGL_ALPHA_SIZE,
            EGL10.EGL_BLUE_SIZE,
            EGL10.EGL_GREEN_SIZE,
            EGL10.EGL_RED_SIZE,
            EGL10.EGL_DEPTH_SIZE,
            EGL10.EGL_STENCIL_SIZE,
            EGL10.EGL_CONFIG_CAVEAT,
            EGL10.EGL_CONFIG_ID,
            EGL10.EGL_LEVEL,
            EGL10.EGL_MAX_PBUFFER_HEIGHT,
            EGL10.EGL_MAX_PBUFFER_PIXELS,
            EGL10.EGL_MAX_PBUFFER_WIDTH,
            EGL10.EGL_NATIVE_RENDERABLE,
            EGL10.EGL_NATIVE_VISUAL_ID,
            EGL10.EGL_NATIVE_VISUAL_TYPE,
            0x3030, // EGL10.EGL_PRESERVED_RESOURCES,
            EGL10.EGL_SAMPLES,
            EGL10.EGL_SAMPLE_BUFFERS,
            EGL10.EGL_SURFACE_TYPE,
            EGL10.EGL_TRANSPARENT_TYPE,
            EGL10.EGL_TRANSPARENT_RED_VALUE,
            EGL10.EGL_TRANSPARENT_GREEN_VALUE,
            EGL10.EGL_TRANSPARENT_BLUE_VALUE,
            0x3039, // EGL10.EGL_BIND_TO_TEXTURE_RGB,
            0x303A, // EGL10.EGL_BIND_TO_TEXTURE_RGBA,
            0x303B, // EGL10.EGL_MIN_SWAP_INTERVAL,
            0x303C, // EGL10.EGL_MAX_SWAP_INTERVAL,
            EGL10.EGL_LUMINANCE_SIZE,
            EGL10.EGL_ALPHA_MASK_SIZE,
            EGL10.EGL_COLOR_BUFFER_TYPE,
            EGL10.EGL_RENDERABLE_TYPE,
            0x3042 // EGL10.EGL_CONFORMANT
        };
        String[] names = {
            "EGL_BUFFER_SIZE",
            "EGL_ALPHA_SIZE",
            "EGL_BLUE_SIZE",
            "EGL_GREEN_SIZE",
            "EGL_RED_SIZE",
            "EGL_DEPTH_SIZE",
            "EGL_STENCIL_SIZE",
            "EGL_CONFIG_CAVEAT",
            "EGL_CONFIG_ID",
            "EGL_LEVEL",
            "EGL_MAX_PBUFFER_HEIGHT",
            "EGL_MAX_PBUFFER_PIXELS",
            "EGL_MAX_PBUFFER_WIDTH",
            "EGL_NATIVE_RENDERABLE",
            "EGL_NATIVE_VISUAL_ID",
            "EGL_NATIVE_VISUAL_TYPE",
            "EGL_PRESERVED_RESOURCES",
            "EGL_SAMPLES",
            "EGL_SAMPLE_BUFFERS",
            "EGL_SURFACE_TYPE",
            "EGL_TRANSPARENT_TYPE",
            "EGL_TRANSPARENT_RED_VALUE",
            "EGL_TRANSPARENT_GREEN_VALUE",
            "EGL_TRANSPARENT_BLUE_VALUE",
            "EGL_BIND_TO_TEXTURE_RGB",
            "EGL_BIND_TO_TEXTURE_RGBA",
            "EGL_MIN_SWAP_INTERVAL",
            "EGL_MAX_SWAP_INTERVAL",
            "EGL_LUMINANCE_SIZE",
            "EGL_ALPHA_MASK_SIZE",
            "EGL_COLOR_BUFFER_TYPE",
            "EGL_RENDERABLE_TYPE",
            "EGL_CONFORMANT"
        };
        int[] value = new int[1];
        for (int i = 0; i < attributes.length; i++) {
            int attribute = attributes[i];
            String name = names[i];
            if ( egl.eglGetConfigAttrib(display, config, attribute, value)) {
                Log.w("GeckoAppJava", String.format("  %s: %d\n", name, value[0]));
            } else {
                Log.w("GeckoAppJava", String.format("  %s: failed\n", name));
                // while (egl.eglGetError() != EGL10.EGL_SUCCESS);
            }
        }
    }

    public void startEgl() {
        if (mEgl != null)
            return;

        mEgl = (EGL10) EGLContext.getEGL();
        mEglDisplay = mEgl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);

        // initialize egl
        int[] version = new int[2];
        mEgl.eglInitialize(mEglDisplay, version);

        // flip this to true to dump all the EGL configs
        if (false) {
            int[] cs = { EGL10.EGL_NONE };
            int[] ptrnum = new int[1];

            mEgl.eglChooseConfig(mEglDisplay, cs, null, 0, ptrnum);

            int num = ptrnum[0];
            EGLConfig[] confs = new EGLConfig[num];

            mEgl.eglChooseConfig(mEglDisplay, cs, confs, num, ptrnum);

            for (int i = 0; i < num; ++i) {
                Log.w("GeckoAppJava", "===  EGL config " + i + " ===");
                printConfig(mEgl, mEglDisplay, confs[i]);
            }
        }
    }

    public void finishEgl() {
        if (mEglDisplay != null) {
            mEgl.eglMakeCurrent(mEglDisplay, EGL10.EGL_NO_SURFACE,
                                EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT);
        }

        if (mEglContext != null) {
            mEgl.eglDestroyContext(mEglDisplay, mEglContext);
            mEglContext = null;
        }

        if (mEglSurface != null) {
            mEgl.eglDestroySurface(mEglDisplay, mEglSurface);
            mEglSurface = null;
        }

        if (mEglDisplay != null) {
            mEgl.eglTerminate(mEglDisplay);
            mEglDisplay = null;
        }

        mEglConfig = null;
        mEgl = null;

        mSurfaceChanged = false;
    }

    public void chooseEglConfig() {
        int redBits, greenBits, blueBits, alphaBits;

        // There are some shenanigans here.
        // We're required to choose an exact EGL config to match
        // the surface format, or we get an error.  However,
        // on Droid at least, the format is -1, which is PixelFormat.OPAQUE.
        // That's not a valid format to be reported; that should just be a
        // valid value for *setFormat*.  We have a catch-all case that
        // assumes 565 for those cases, but it's pretty ugly.

        Log.i("GeckoAppJava", "GeckoView PixelFormat format is " + mFormat);
        if (mFormat == PixelFormat.RGB_565) {
            redBits = 5;
            greenBits = 6;
            blueBits = 5;
            alphaBits = 0;
        } else if (mFormat == PixelFormat.RGB_888 ||
                   mFormat == PixelFormat.RGBX_8888)
        {
            redBits = 8;
            greenBits = 8;
            blueBits = 8;
            alphaBits = 0;
        } else if (mFormat == PixelFormat.RGBA_8888) {
            redBits = 8;
            greenBits = 8;
            blueBits = 8;
            alphaBits = 8;
        } else {
            Log.w("GeckoAppJava", "Unknown PixelFormat for surface (format is " + mFormat + "), assuming 5650!");
            redBits = 5;
            greenBits = 6;
            blueBits = 5;
            alphaBits = 0;
        }

        // PowerVR SGX (Droid) seems to really want depth == 24 for
        // performance, even 0 is slower.  However, other platforms,
        // like Tegra, don't -have- a 24 bit depth config (have 16).
        // So that's not good.  We'll try with 24 first, and if
        // nothing, then we'll go to 0.  I'm not sure what the nexus
        // one chip wants.

        int[] confSpec = new int[] {
            // DEPTH_SIZE must be the first pair
            EGL10.EGL_DEPTH_SIZE, 24,
            EGL10.EGL_RED_SIZE, redBits,
            EGL10.EGL_GREEN_SIZE, greenBits,
            EGL10.EGL_BLUE_SIZE, blueBits,
            EGL10.EGL_ALPHA_SIZE, alphaBits,
            EGL10.EGL_STENCIL_SIZE, 0,
            EGL10.EGL_CONFIG_CAVEAT, EGL10.EGL_NONE,
            EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL10.EGL_NONE };

        // so tortured to pass an int as an out param...
        int[] ptrNumConfigs = new int[1];
        mEgl.eglChooseConfig(mEglDisplay, confSpec, null, 0, ptrNumConfigs);

        int numConfigs = ptrNumConfigs[0];

        if (numConfigs == 0) {
            // twiddle the DEPTH_SIZE value
            confSpec[1] = 0;
            Log.i("GeckoAppJava", "Couldn't find any valid EGL configs with 24 bit depth, trying 0.");

            mEgl.eglChooseConfig(mEglDisplay, confSpec, null, 0, ptrNumConfigs);
            numConfigs = ptrNumConfigs[0];
        }

        if (numConfigs <= 0) {
            // couldn't find a config?
            Log.w("GeckoAppJava", "Couldn't find any valid EGL configs, blindly trying them all!");

            int[] fallbackConfSpec = new int[] {
                EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL10.EGL_NONE };
            confSpec = fallbackConfSpec;

            mEgl.eglChooseConfig(mEglDisplay, confSpec, null, 0, ptrNumConfigs);
            numConfigs = ptrNumConfigs[0];

            if (numConfigs == 0) {
                Log.e("GeckoAppJava", "There aren't any EGL configs available on this system.");
                finishEgl();
                mSurfaceValid = false;
                return;
            }
        }

        EGLConfig[] configs = new EGLConfig[numConfigs];
        mEgl.eglChooseConfig(mEglDisplay, confSpec, configs, numConfigs, ptrNumConfigs);

        // Find the first config that has the exact RGB sizes that we
        // need for our window surface.
        int[] ptrVal = new int[1];
        for (int i = 0; i < configs.length; ++i) {
            int confRed = -1, confGreen = -1, confBlue = -1, confAlpha = -1;

            if (mEgl.eglGetConfigAttrib(mEglDisplay, configs[i], EGL10.EGL_RED_SIZE, ptrVal))
                confRed = ptrVal[0];
            if (mEgl.eglGetConfigAttrib(mEglDisplay, configs[i], EGL10.EGL_GREEN_SIZE, ptrVal))
                confGreen = ptrVal[0];
            if (mEgl.eglGetConfigAttrib(mEglDisplay, configs[i], EGL10.EGL_BLUE_SIZE, ptrVal))
                confBlue = ptrVal[0];
            if (mEgl.eglGetConfigAttrib(mEglDisplay, configs[i], EGL10.EGL_ALPHA_SIZE, ptrVal))
                confAlpha = ptrVal[0];

            if (confRed == redBits &&
                confGreen == greenBits &&
                confBlue == blueBits &&
                confAlpha == alphaBits)
            {
                mEglConfig = configs[i];
                break;
            }
        }

        if (mEglConfig == null) {
            Log.w("GeckoAppJava", "Couldn't find EGL config matching colors; using first, hope it works!");
            mEglConfig = configs[0];
        }

        Log.i("GeckoAppJava", "====== Chosen config: ======");
        printConfig(mEgl, mEglDisplay, mEglConfig);

        mEglContext = null;
        mEglSurface = null;
    }

    /*
     * Called on main thread
     */

    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        mSurfaceLock.lock();

        try {
            if (mInDrawing) {
                Log.w("GeckoAppJava", "surfaceChanged while mInDrawing is true!");
            }

            mFormat = format;
            mWidth = width;
            mHeight = height;
            mSurfaceValid = true;

            Log.i("GeckoAppJava", "surfaceChanged: fmt: " + format + " dim: " + width + " " + height);

            if (!GeckoApp.useSoftwareDrawing) {
                chooseEglConfig();
            }

            // XXX This code doesn't seem to actually get hit
            if (!GeckoAppShell.sGeckoRunning) {
                GeckoAppShell.setInitialSize(width, height);
                return;
            }

            GeckoEvent e = new GeckoEvent(GeckoEvent.SIZE_CHANGED, width, height, -1, -1);
            GeckoAppShell.sendEventToGecko(e);

            if (mSurfaceNeedsRedraw) {
                GeckoAppShell.scheduleRedraw();
                mSurfaceNeedsRedraw = false;
            }

            mSurfaceChanged = true;

            //Log.i("GeckoAppJava", "<< surfaceChanged");
        } finally {
            mSurfaceLock.unlock();
        }
    }
 
    public void surfaceCreated(SurfaceHolder holder) {
        if (GeckoAppShell.sGeckoRunning)
            mSurfaceNeedsRedraw = true;
    }
 
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i("GeckoAppJava", "surface destroyed");
        mSurfaceValid = false;
    }

    public ByteBuffer getSoftwareDrawBuffer() {
        //#ifdef DEBUG
        if (!mSurfaceLock.isHeldByCurrentThread())
            Log.e("GeckoAppJava", "getSoftwareDrawBuffer called outside of mSurfaceLock!");
        //#endif

        return mSoftwareBuffer;
    }

    /*
     * Called on Gecko thread
     */

    public static final int DRAW_ERROR = 0;
    public static final int DRAW_GLES_2 = 1;
    public static final int DRAW_SOFTWARE = 2;

    int innerBeginDrawing() {
        /*
         * Software (non-GL) rendering
         */
        if (GeckoApp.useSoftwareDrawing) {
            if (mWidth != mBufferWidth ||
                mHeight != mBufferHeight ||
                mSurfaceChanged)
            {
                if (mWidth*mHeight != mBufferWidth*mBufferHeight)
                    mSoftwareBuffer = ByteBuffer.allocateDirect(mWidth*mHeight*4);

                mSoftwareBitmap = Bitmap.createBitmap(mWidth, mHeight, Bitmap.Config.ARGB_8888);

                mBufferWidth = mWidth;
                mBufferHeight = mHeight;
                mSurfaceChanged = false;
            }

            mSoftwareCanvas = getHolder().lockCanvas(null);
            if (mSoftwareCanvas == null) {
                Log.e("GeckoAppJava", "lockCanvas failed! << beginDrawing");
                return DRAW_ERROR;
            }

            return DRAW_SOFTWARE;
        }

        /*
         * GL rendering
         */

        if (mEgl == null || mEglDisplay == null || mEglConfig == null) {
            Log.e("GeckoAppJava", "beginDrawing called, but EGL was never initialized!");

            mSurfaceLock.unlock();
            return DRAW_ERROR;
        }

        /*
         * If the surface doesn't exist, or if its dimensions or something else changed,
         * recreate it.
         */
        if (mEglSurface == null ||
            mWidth != mBufferWidth ||
            mHeight != mBufferHeight ||
            mSurfaceChanged)
        {
            if (mEglContext != null) {
                mEgl.eglDestroyContext(mEglDisplay, mEglContext);
                mEglContext = null;
            }

            if (mEglSurface != null)
                mEgl.eglDestroySurface(mEglDisplay, mEglSurface);

            mEglSurface = mEgl.eglCreateWindowSurface(mEglDisplay, mEglConfig, getHolder(), null);
            if (mEglSurface == EGL10.EGL_NO_SURFACE)
            {
                Log.e("GeckoAppJava", "eglCreateWindowSurface failed!");
                mSurfaceValid = false;
                return DRAW_ERROR;
            }

            mBufferWidth = mWidth;
            mBufferHeight = mHeight;

            mSurfaceChanged = false;
        }

        if (mEglContext == null) {
            int[] attrib_list = { EGL_CONTEXT_CLIENT_VERSION, 2,
                                  EGL10.EGL_NONE };
            mEglContext = mEgl.eglCreateContext(mEglDisplay, mEglConfig, EGL10.EGL_NO_CONTEXT, attrib_list);
            if (mEglContext == EGL10.EGL_NO_CONTEXT)
            {
                Log.e("GeckoAppJava", "eglCreateContext failed! " + mEgl.eglGetError());
                mSurfaceValid = false;
                return DRAW_ERROR;
            }

            Log.i("GeckoAppJava", "EglContext created");
        }

        // Hmm, should we issue this makecurrent for each frame?
        // We'll likely have to, as other bits might switch the context (e.g.
        // WebGL, video, etc.).

        if (!mEgl.eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext)) {
            int err = mEgl.eglGetError();
            Log.e("GeckoAppJava", "eglMakeCurrent failed! " + err);
            return DRAW_ERROR;
        }

        return DRAW_GLES_2;
    }

    public int beginDrawing() {
        //Log.i("GeckoAppJava", ">> beginDrawing");

        if (mInDrawing) {
            Log.e("GeckoAppJava", "Recursive beginDrawing call!");
            return DRAW_ERROR;
        }

        /* Grab the lock, which we'll hold while we're drawing.
         * It gets released in endDrawing(), and is also used in surfaceChanged
         * to make sure that we don't change our surface details while
         * we're in the middle of drawing (and especially in the middle of
         * executing beginDrawing/endDrawing).
         *
         * We might not need to hold this lock in between
         * beginDrawing/endDrawing, and might just be able to make
         * surfaceChanged, beginDrawing, and endDrawing synchronized,
         * but this way is safer for now.
         */
        mSurfaceLock.lock();

        if (!mSurfaceValid) {
            Log.e("GeckoAppJava", "Surface not valid");
            mSurfaceLock.unlock();
            return DRAW_ERROR;
        }

        // call the inner function to do the work, so we can sanely unlock on error
        int result = innerBeginDrawing();

        if (result == DRAW_ERROR) {
            mSurfaceLock.unlock();
            return DRAW_ERROR;
        }

        mInDrawing = true;
        return result;
    }

    public void endDrawing() {
        //Log.w("GeckoAppJava", ">> endDrawing");

        if (!mInDrawing) {
            Log.e("GeckoAppJava", "endDrawing without beginDrawing!");
            return;
        }

        try {
            if (!mSurfaceValid) {
                Log.e("GeckoAppJava", "endDrawing with false mSurfaceValid");
                return;
            }

            if (GeckoApp.useSoftwareDrawing) {
                if (!mSurfaceChanged) {
                    mSoftwareBitmap.copyPixelsFromBuffer(mSoftwareBuffer);
                    mSoftwareCanvas.drawBitmap(mSoftwareBitmap, 0, 0, null);

                    getHolder().unlockCanvasAndPost(mSoftwareCanvas);
                    mSoftwareCanvas = null;
                }
            } else {
                // If the surface was changed while we were drawing;
                // don't even bother trying to swap, it won't work,
                // and may cause further problems.  Note that
                // eglSwapBuffers might also throw an
                // IllegalArgumentException; we catch that as well.
                if (!mSurfaceChanged) {
                    mEgl.eglSwapBuffers(mEglDisplay, mEglSurface);
                    if (mEgl.eglGetError() == EGL11.EGL_CONTEXT_LOST)
                        mSurfaceChanged = true;
                }
            }
        } catch (java.lang.IllegalArgumentException ex) {
            mSurfaceChanged = true;
        } finally {
            mInDrawing = false;

            //#ifdef DEBUG
            if (!mSurfaceLock.isHeldByCurrentThread())
                Log.e("GeckoAppJava", "endDrawing while mSurfaceLock not held by current thread!");
            //#endif

            mSurfaceLock.unlock();
        }
    }

    @Override
    public boolean onCheckIsTextEditor () {
        return false;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        outAttrs.inputType = InputType.TYPE_CLASS_TEXT |
                             InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
        return inputConnection;
    }

    // accelerometer
    public void onAccuracyChanged(Sensor sensor, int accuracy)
    {
    }

    public void onSensorChanged(SensorEvent event)
    {
        GeckoAppShell.sendEventToGecko(new GeckoEvent(event));
    }

    // geolocation
    public void onLocationChanged(Location location)
    {
        GeckoAppShell.sendEventToGecko(new GeckoEvent(location));
    }

    public void onProviderDisabled(String provider)
    {
    }

    public void onProviderEnabled(String provider)
    {
    }

    public void onStatusChanged(String provider, int status, Bundle extras)
    {
    }

    // event stuff
    public boolean onTouchEvent(MotionEvent event) {
        GeckoAppShell.sendEventToGecko(new GeckoEvent(event));
        return true;
    }

    // Is this surface valid for drawing into?
    boolean mSurfaceValid;

    // Do we need to force a redraw on surfaceChanged?
    boolean mSurfaceNeedsRedraw;

    // Has this surface been changed?  (That is,
    // do we need to recreate buffers?)
    boolean mSurfaceChanged;

    // Are we actively between beginDrawing/endDrawing?
    boolean mInDrawing;

    // let's not change stuff around while we're in the middle of
    // starting drawing, ending drawing, or changing surface
    // characteristics
    ReentrantLock mSurfaceLock;

    // Surface format, from surfaceChanged.  Largely
    // useless.
    int mFormat;

    // the dimensions of the surface
    int mWidth;
    int mHeight;

    // the dimensions of the buffer we're using for drawing,
    // that is the software buffer or the EGLSurface
    int mBufferWidth;
    int mBufferHeight;

    // IME stuff
    GeckoInputConnection inputConnection;
    int mIMEState;

    // Software rendering
    ByteBuffer mSoftwareBuffer;
    Bitmap mSoftwareBitmap;
    Canvas mSoftwareCanvas;

    // GL rendering
    EGL10 mEgl;
    EGLDisplay mEglDisplay;
    EGLSurface mEglSurface;
    EGLConfig mEglConfig;
    EGLContext mEglContext;
}

class GeckoInputConnection
    extends BaseInputConnection
{
    public GeckoInputConnection (View targetView) {
        super(targetView, true);
        mQueryResult = new SynchronousQueue<String>();
        mExtractedText.partialStartOffset = -1;
        mExtractedText.partialEndOffset = -1;
    }

    @Override
    public Editable getEditable() {
        Log.i("GeckoAppJava", "getEditable");
        return null;
    }

    @Override
    public boolean beginBatchEdit() {
        if (mComposing)
            return true;
        GeckoAppShell.sendEventToGecko(new GeckoEvent(true, null));
        mComposing = true;
        return true;
    }
    @Override
    public boolean commitCompletion(CompletionInfo text) {
        Log.i("GeckoAppJava", "Stub: commitCompletion");
        return true;
    }
    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        GeckoAppShell.sendEventToGecko(new GeckoEvent(true, text.toString()));
        endBatchEdit();
        return true;
    }
    @Override
    public boolean deleteSurroundingText(int leftLength, int rightLength) {
        GeckoAppShell.sendEventToGecko(new GeckoEvent(leftLength, rightLength));
        updateExtractedText();
        return true;
    }
    @Override
    public boolean endBatchEdit() {
        updateExtractedText();
        if (!mComposing)
            return true;
        GeckoAppShell.sendEventToGecko(new GeckoEvent(false, null));
        mComposing = false;
        return true;
    }
    @Override
    public boolean finishComposingText() {
        endBatchEdit();
        return true;
    }
    @Override
    public int getCursorCapsMode(int reqModes) {
        return 0;
    }
    @Override
    public ExtractedText getExtractedText(ExtractedTextRequest req, int flags) {
        mExtractToken = req.token;
        GeckoAppShell.sendEventToGecko(new GeckoEvent(false, 0));
        try {
            mExtractedText.text = mQueryResult.take();
            mExtractedText.selectionStart = mSelectionStart;
            mExtractedText.selectionEnd = mSelectionEnd;
        } catch (InterruptedException e) {
            Log.i("GeckoAppJava", "getExtractedText: Interrupted!");
        }
        return mExtractedText;
    }
    @Override
    public CharSequence getTextAfterCursor(int length, int flags) {
        GeckoAppShell.sendEventToGecko(new GeckoEvent(true, length));
        try {
            String result = mQueryResult.take();
            return result;
        } catch (InterruptedException e) {
            Log.i("GeckoAppJava", "getTextAfterCursor: Interrupted!");
        }
        return null;
    }
    @Override
    public CharSequence getTextBeforeCursor(int length, int flags) {
        GeckoAppShell.sendEventToGecko(new GeckoEvent(false, length));
        try {
            String result = mQueryResult.take();
            return result;
        } catch (InterruptedException e) {
            Log.i("GeckoAppJava", "getTextBeforeCursor: Interrupted!");
        }
        return null;
    }
    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        beginBatchEdit();
        GeckoAppShell.sendEventToGecko(new GeckoEvent(true, text.toString()));
        return true;
    }
    @Override
    public boolean setSelection(int start, int end) {
        Log.i("GeckoAppJava", "Stub: setSelection " + start + " " + end);
        return true;
    }

    private void updateExtractedText() {
        GeckoAppShell.sendEventToGecko(new GeckoEvent(false, 0));
        try {
            mExtractedText.text = mQueryResult.take();
            mExtractedText.selectionStart = mSelectionStart;
            mExtractedText.selectionEnd = mSelectionEnd;
        } catch (InterruptedException e) {
            Log.i("GeckoAppJava", "getExtractedText: Interrupted!");
        }

        InputMethodManager imm = (InputMethodManager)
            GeckoApp.surfaceView.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.updateExtractedText(GeckoApp.surfaceView, mExtractToken, mExtractedText);
    }

    boolean mComposing;
    int mExtractToken;
    final ExtractedText mExtractedText = new ExtractedText();

    int mSelectionStart, mSelectionEnd;
    SynchronousQueue<String> mQueryResult;
}
