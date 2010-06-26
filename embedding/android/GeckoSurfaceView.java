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

        mWidth = 0;
        mHeight = 0;
        mBufferWidth = 0;
        mBufferHeight = 0;

        mSurfaceLock = new ReentrantLock();
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

            mFormat = format;
            mWidth = width;
            mHeight = height;
            mSurfaceValid = true;

            Log.i("GeckoAppJava", "surfaceChanged: fmt: " + format + " dim: " + width + " " + height);

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
