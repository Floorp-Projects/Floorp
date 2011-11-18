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
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
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

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.gfx.CairoImage;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerClient;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.LayerRenderer;
import org.mozilla.gecko.gfx.SingleTileLayer;
import org.mozilla.gecko.ui.ViewportController;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import android.content.Context;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.Log;
import java.nio.ByteBuffer;
import java.util.concurrent.Semaphore;
import java.util.Timer;
import java.util.TimerTask;

/**
 * Transfers a software-rendered Gecko to an ImageLayer so that it can be rendered by our
 * compositor.
 *
 * TODO: Throttle down Gecko's priority when we pan and zoom.
 */
public class GeckoSoftwareLayerClient extends LayerClient {
    private Context mContext;
    private int mWidth, mHeight, mFormat;
    private ByteBuffer mBuffer;
    private Semaphore mBufferSemaphore;
    private SingleTileLayer mTileLayer;
    private ViewportController mViewportController;

    private RectF mGeckoVisibleRect;
    /* The viewport rect that Gecko is currently displaying. */

    private Rect mJSPanningToRect;
    /* The rect that we just told chrome JavaScript to pan to. */

    private boolean mWaitingForJSPanZoom;
    /* This will be set to true if we are waiting on the chrome JavaScript to finish panning or
     * zooming before we can render. */

    private CairoImage mCairoImage;

    /* The initial page width and height that we use before a page is loaded. */
    private static final int PAGE_WIDTH = 980;      /* Matches MobileSafari. */
    private static final int PAGE_HEIGHT = 1500;

    public GeckoSoftwareLayerClient(Context context) {
        mContext = context;

        mViewportController = new ViewportController(new IntSize(PAGE_WIDTH, PAGE_HEIGHT),
                                                     new RectF(0, 0, 1, 1));

        mWidth = LayerController.TILE_WIDTH;
        mHeight = LayerController.TILE_HEIGHT;
        mFormat = CairoImage.FORMAT_RGB16_565;

        mBuffer = ByteBuffer.allocateDirect(mWidth * mHeight * 2);
        mBufferSemaphore = new Semaphore(1);

        mWaitingForJSPanZoom = false;

        mCairoImage = new CairoImage() {
            @Override
            public ByteBuffer lockBuffer() {
                try {
                    mBufferSemaphore.acquire();
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
                return mBuffer;
            }
            @Override
            public void unlockBuffer() {
                mBufferSemaphore.release();
            }
            @Override
            public int getWidth() { return mWidth; }
            @Override
            public int getHeight() { return mHeight; }
            @Override
            public int getFormat() { return mFormat; }
        };

        mTileLayer = new SingleTileLayer();
    }

    /** Attaches the root layer to the layer controller so that Gecko appears. */
    @Override
    public void init() {
        getLayerController().setRoot(mTileLayer);
        render();
    }

    public void beginDrawing() {
        /* no-op */
    }

    /*
     * TODO: Would be cleaner if this took an android.graphics.Rect instead, but that would require
     * a little more JNI magic.
     */
    public void endDrawing(int x, int y, int width, int height) {
        LayerController controller = getLayerController();
        if (controller == null)
            return;
        //controller.unzoom();  /* FIXME */
        controller.notifyViewOfGeometryChange();

        mViewportController.setVisibleRect(mGeckoVisibleRect);

        if (mGeckoVisibleRect != null) {
            RectF layerRect = mViewportController.untransformVisibleRect(mGeckoVisibleRect,
                                                                         getPageSize());
            mTileLayer.origin = new PointF(layerRect.left, layerRect.top);
        }

        repaint(new Rect(x, y, x + width, y + height));
    }

    /*
     * Temporary fix to allow both old and new widget APIs to access this object. See bug 703421.
     */
    public void endDrawing(int x, int y, int width, int height, String metadata) {
        endDrawing(x, y, width, height);
    }

    private void repaint(Rect rect) {
        mTileLayer.paintSubimage(mCairoImage, rect);
    }

    /** Called whenever the chrome JS finishes panning or zooming to some location. */
    public void jsPanZoomCompleted(Rect rect) {
        mGeckoVisibleRect = new RectF(rect);
        if (mWaitingForJSPanZoom)
            render();
    }

    /**
     * Acquires a lock on the back buffer and returns it, blocking until it's unlocked. This
     * function is for Gecko to use.
     */
    public ByteBuffer lockBuffer() {
        try {
            mBufferSemaphore.acquire();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
        return mBuffer;
    }

    /**
     * Releases the lock on the back buffer. After this call, it is forbidden for Gecko to touch
     * the buffer. This function is, again, for Gecko to use.
     */
    public void unlockBuffer() {
        mBufferSemaphore.release();
    }

    /** Called whenever the page changes size. */
    public void setPageSize(IntSize pageSize) {
        mViewportController.setPageSize(pageSize);
        getLayerController().setPageSize(pageSize);
    }

    @Override
    public void geometryChanged() {
        mViewportController.setVisibleRect(getTransformedVisibleRect());
        render();
    }

    @Override
    public IntSize getPageSize() { return mViewportController.getPageSize(); }

    @Override
    public void render() {
        LayerController layerController = getLayerController();
        RectF visibleRect = layerController.getVisibleRect();
        RectF tileRect = mViewportController.widenRect(visibleRect);
        tileRect = mViewportController.clampRect(tileRect);

        IntSize pageSize = layerController.getPageSize();
        RectF viewportRect = mViewportController.transformVisibleRect(tileRect, pageSize);

        /* Prevent null pointer exceptions at the start. */
        if (mGeckoVisibleRect == null)
            mGeckoVisibleRect = viewportRect;

        if (!getLayerController().getRedrawHint())
            return;

        /* If Gecko's visible rect is the same as our visible rect, then we can actually kick off a
         * draw event. */
        if (mGeckoVisibleRect.equals(viewportRect)) {
            mWaitingForJSPanZoom = false;
            mJSPanningToRect = null;
            GeckoAppShell.scheduleRedraw();
            return;
        }

        /* Otherwise, we need to get Gecko's visible rect equal to our visible rect before we can
         * safely draw. If we're just waiting for chrome JavaScript to catch up, we do nothing.
         * This check avoids bombarding the chrome JavaScript with messages. */
        int viewportRectX = (int)Math.round(viewportRect.left);
        int viewportRectY = (int)Math.round(viewportRect.top);
        Rect panToRect = new Rect(viewportRectX, viewportRectY,
                                  viewportRectX + LayerController.TILE_WIDTH,
                                  viewportRectY + LayerController.TILE_HEIGHT);

        if (mWaitingForJSPanZoom && mJSPanningToRect != null &&
                mJSPanningToRect.equals(panToRect)) {
            return;
        }

        /* We send Gecko a message telling it to move its visible rect to the appropriate spot and
         * set a flag to remind us to try the redraw again. */

        GeckoAppShell.sendEventToGecko(new GeckoEvent("PanZoom:PanZoom",
            "{\"x\": " + panToRect.left + ", \"y\": " + panToRect.top +
            ", \"width\": " + panToRect.width() + ", \"height\": " + panToRect.height() +
            ", \"zoomFactor\": " + getZoomFactor() + "}"));

        mJSPanningToRect = panToRect;
        mWaitingForJSPanZoom = true;
    }

    /* Returns the dimensions of the box in page coordinates that the user is viewing. */
    private RectF getTransformedVisibleRect() {
        LayerController layerController = getLayerController();
        return mViewportController.transformVisibleRect(layerController.getVisibleRect(),
                                                        layerController.getPageSize());
    }

    private float getZoomFactor() {
        return 1.0f;    // FIXME
        /*LayerController layerController = getLayerController();
        return mViewportController.getZoomFactor(layerController.getVisibleRect(),
                                                 layerController.getPageSize(),
                                                 layerController.getScreenSize());*/
    }
}

