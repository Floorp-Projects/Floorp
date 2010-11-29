/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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

import android.os.*;
import android.app.*;
import android.text.*;
import android.text.method.*;
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

        mWidth = 0;
        mHeight = 0;
        mBufferWidth = 0;
        mBufferHeight = 0;

        mSurfaceLock = new ReentrantLock();

        mEditableFactory = Editable.Factory.getInstance();
        setupEditable("");
        mIMEState = IME_STATE_DISABLED;
        mIMETypeHint = "";
        mIMEActionHint = "";
    }

    protected void finalize() throws Throwable {
        super.finalize();
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

            if (width == 0 || height == 0)
                mSoftwareBuffer = null;
            else if (mSoftwareBuffer == null ||
                     mSoftwareBuffer.capacity() < (width * height * 2) ||
                     mWidth != width || mHeight != height)
                mSoftwareBuffer = ByteBuffer.allocateDirect(width * height * 2);
            boolean doSyncDraw = m2DMode && mSoftwareBuffer != null &&
                GeckoApp.checkLaunchState(GeckoApp.LaunchState.GeckoRunning);
            mSyncDraw = doSyncDraw;

            mFormat = format;
            mWidth = width;
            mHeight = height;
            mSurfaceValid = true;

            Log.i("GeckoAppJava", "surfaceChanged: fmt: " + format + " dim: " + width + " " + height);

            GeckoEvent e = new GeckoEvent(GeckoEvent.SIZE_CHANGED, width, height, -1, -1);
            GeckoAppShell.sendEventToGecko(e);

            if (mSoftwareBuffer != null)
                GeckoAppShell.scheduleRedraw();

            if (!doSyncDraw) {
                Canvas c = holder.lockCanvas();
                c.drawARGB(255, 255, 255, 255);
                holder.unlockCanvasAndPost(c);
                return;
            }
        } finally {
            mSurfaceLock.unlock();
        }

        ByteBuffer bb = null;
        try {
            bb = mSyncBuf.take();
        } catch (InterruptedException ie) {
            Log.e("GeckoAppJava", "Threw exception while getting sync buf: " + ie);
        }
        if (bb != null && bb.capacity() == (width * height * 2)) {
            mSoftwareBitmap = Bitmap.createBitmap(mWidth, mHeight, Bitmap.Config.RGB_565);
            mSoftwareBitmap.copyPixelsFromBuffer(bb);
            Canvas c = holder.lockCanvas();
            c.drawBitmap(mSoftwareBitmap, 0, 0, null);
            holder.unlockCanvasAndPost(c);
        }
    }

    public void surfaceCreated(SurfaceHolder holder) {
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i("GeckoAppJava", "surface destroyed");
        mSurfaceValid = false;
        mSoftwareBuffer = null;
    }

    public ByteBuffer getSoftwareDrawBuffer() {
        m2DMode = true;
        return mSoftwareBuffer;
    }

    /*
     * Called on Gecko thread
     */

    public static final int DRAW_ERROR = 0;
    public static final int DRAW_GLES_2 = 1;

    public int beginDrawing() {
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

        mInDrawing = true;
        return DRAW_GLES_2;
    }

    public void endDrawing() {
        if (!mInDrawing) {
            Log.e("GeckoAppJava", "endDrawing without beginDrawing!");
            return;
        }

        try {
            if (!mSurfaceValid) {
                Log.e("GeckoAppJava", "endDrawing with false mSurfaceValid");
                return;
            }
        } finally {
            mInDrawing = false;

            if (!mSurfaceLock.isHeldByCurrentThread())
                Log.e("GeckoAppJava", "endDrawing while mSurfaceLock not held by current thread!");

            mSurfaceLock.unlock();
        }
    }

    /* How this works:
     * Whenever we want to draw, we want to be sure that we do not lock
     * the canvas unless we're sure we can draw. Locking the canvas clears
     * the canvas to black in most cases, causing a black flash.
     * At the same time, the surface can resize/disappear at any moment
     * unless the canvas is locked.
     * Draws originate from a different thread so the surface could change
     * at any moment while we try to draw until we lock the canvas.
     *
     * Also, never try to lock the canvas while holding the surface lock
     * unless you're in SurfaceChanged, in which case the canvas was already
     * locked. Surface lock -> Canvas lock will lead to AB-BA deadlocks.
     */
    public void draw2D(ByteBuffer buffer, int stride) {
        // mSurfaceLock ensures that we get mSyncDraw/mSoftwareBuffer/etc.
        // set correctly before determining whether we should do a sync draw
        mSurfaceLock.lock();
        try {
            if (mSyncDraw) {
                if (buffer != mSoftwareBuffer || stride != (mWidth * 2))
                    return;
                mSyncDraw = false;
                try {
                    mSyncBuf.put(buffer);
                } catch (InterruptedException ie) {
                    Log.e("GeckoAppJava", "Threw exception while getting sync buf: " + ie);
                }
                return;
            }
        } finally {
            mSurfaceLock.unlock();
        }

        if (buffer != mSoftwareBuffer || stride != (mWidth * 2))
            return;
        Canvas c = getHolder().lockCanvas();
        if (c == null)
            return;
        if (buffer != mSoftwareBuffer || stride != (mWidth * 2)) {
            /* We're screwed. Fill it with white and hope it isn't too noticable
             * This could potentially happen if this function is called
             * right before mSurfaceLock is locked in SurfaceChanged.
             * However, I've never actually seen this code get hit.
             */
            c.drawARGB(255, 255, 255, 255);
            getHolder().unlockCanvasAndPost(c);
            return;
        }
        if (mSoftwareBitmap == null ||
            mSoftwareBitmap.getHeight() != mHeight ||
            mSoftwareBitmap.getWidth() != mWidth) {
            mSoftwareBitmap = Bitmap.createBitmap(mWidth, mHeight, Bitmap.Config.RGB_565);
        }
        mSoftwareBitmap.copyPixelsFromBuffer(mSoftwareBuffer);
        c.drawBitmap(mSoftwareBitmap, 0, 0, null);
        getHolder().unlockCanvasAndPost(c);
    }

    @Override
    public boolean onCheckIsTextEditor () {
        return false;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        outAttrs.inputType = InputType.TYPE_CLASS_TEXT;
        outAttrs.imeOptions = EditorInfo.IME_ACTION_GO;
        mKeyListener = TextKeyListener.getInstance();

        if (mIMEState == IME_STATE_PASSWORD)
            outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_PASSWORD;
        else if (mIMETypeHint.equalsIgnoreCase("url"))
            outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_URI;
        else if (mIMETypeHint.equalsIgnoreCase("email"))
            outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
        else if (mIMETypeHint.equalsIgnoreCase("search"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_SEARCH;
        else if (mIMETypeHint.equalsIgnoreCase("tel"))
            outAttrs.inputType = InputType.TYPE_CLASS_PHONE;
        else if (mIMETypeHint.equalsIgnoreCase("number") ||
                 mIMETypeHint.equalsIgnoreCase("range"))
            outAttrs.inputType = InputType.TYPE_CLASS_NUMBER;
        else if (mIMETypeHint.equalsIgnoreCase("datetime") ||
                 mIMETypeHint.equalsIgnoreCase("datetime-local"))
            outAttrs.inputType = InputType.TYPE_CLASS_DATETIME |
                                 InputType.TYPE_DATETIME_VARIATION_NORMAL;
        else if (mIMETypeHint.equalsIgnoreCase("date"))
            outAttrs.inputType = InputType.TYPE_CLASS_DATETIME |
                                 InputType.TYPE_DATETIME_VARIATION_DATE;
        else if (mIMETypeHint.equalsIgnoreCase("time"))
            outAttrs.inputType = InputType.TYPE_CLASS_DATETIME |
                                 InputType.TYPE_DATETIME_VARIATION_TIME;

        if (mIMEActionHint.equalsIgnoreCase("go"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_GO;
        else if (mIMEActionHint.equalsIgnoreCase("done"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_DONE;
        else if (mIMEActionHint.equalsIgnoreCase("next"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_NEXT;
        else if (mIMEActionHint.equalsIgnoreCase("search"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_SEARCH;
        else if (mIMEActionHint.equalsIgnoreCase("send"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_SEND;
        else
            outAttrs.actionLabel = mIMEActionHint;
        inputConnection.reset();
        return inputConnection;
    }

    public void setupEditable(String contents)
    {
        mEditable = mEditableFactory.newEditable(contents);
        mEditable.setSpan(inputConnection, 0, contents.length(), Spanned.SPAN_INCLUSIVE_INCLUSIVE);
        Selection.setSelection(mEditable, contents.length());
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

    // Are we actively between beginDrawing/endDrawing?
    boolean mInDrawing;

    // Are we waiting for a buffer to draw in surfaceChanged?
    boolean mSyncDraw;

    // True if gecko requests a buffer
    boolean m2DMode;

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
    public static final int IME_STATE_DISABLED = 0;
    public static final int IME_STATE_ENABLED = 1;
    public static final int IME_STATE_PASSWORD = 2;

    GeckoInputConnection inputConnection;
    KeyListener mKeyListener;
    Editable mEditable;
    Editable.Factory mEditableFactory;
    boolean mIMEFocus;
    int mIMEState;
    String mIMETypeHint;
    String mIMEActionHint;

    // Software rendering
    ByteBuffer mSoftwareBuffer;
    Bitmap mSoftwareBitmap;

    final SynchronousQueue<ByteBuffer> mSyncBuf = new SynchronousQueue<ByteBuffer>();
}

