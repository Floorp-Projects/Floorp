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

package org.mozilla.fennec.gfx;

import org.mozilla.fennec.gfx.IntRect;
import org.mozilla.fennec.gfx.IntSize;
import org.mozilla.fennec.gfx.Layer;
import org.mozilla.fennec.gfx.LayerClient;
import org.mozilla.fennec.gfx.LayerView;
import org.mozilla.fennec.ui.PanZoomController;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Log;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View.OnTouchListener;
import java.util.ArrayList;

/**
 * The layer controller manages a tile that represents the visible page. It does panning and
 * zooming natively by delegating to a panning/zooming controller. Touch events can be dispatched
 * to a higher-level view.
 */
public class LayerController implements ScaleGestureDetector.OnScaleGestureListener {
    private Layer mRootLayer;                   /* The root layer. */
    private LayerView mView;                    /* The main rendering view. */
    private Context mContext;                   /* The current context. */
    private IntRect mVisibleRect;               /* The current visible region. */
    private IntSize mScreenSize;                /* The screen size of the viewport. */
    private IntSize mPageSize;                  /* The current page size. */

    private PanZoomController mPanZoomController;
    /*
     * The panning and zooming controller, which interprets pan and zoom gestures for us and
     * updates our visible rect appropriately.
     */

    private OnTouchListener mOnTouchListener;   /* The touch listener. */
    private LayerClient mLayerClient;           /* The layer client. */

    private ArrayList<OnGeometryChangeListener> mOnGeometryChangeListeners;
    /* A list of listeners that will be notified whenever the geometry changes. */
    private ArrayList<OnPageSizeChangeListener> mOnPageSizeChangeListeners;
    /* A list of listeners that will be notified whenever the page size changes. */

    /* NB: These must be powers of two due to the OpenGL ES 1.x restriction on NPOT textures. */
    public static final int TILE_WIDTH = 1024;
    public static final int TILE_HEIGHT = 2048;

    public LayerController(Context context, LayerClient layerClient) {
        mContext = context;

        mOnGeometryChangeListeners = new ArrayList<OnGeometryChangeListener>();
        mOnPageSizeChangeListeners = new ArrayList<OnPageSizeChangeListener>();

        mVisibleRect = new IntRect(0, 0, 1, 1);     /* Gets filled in when the surface changes. */
        mScreenSize = new IntSize(1, 1);

        if (layerClient != null)
            setLayerClient(layerClient);
        else
            mPageSize = new IntSize(LayerController.TILE_WIDTH, LayerController.TILE_HEIGHT);

        mView = new LayerView(context, this);
        mPanZoomController = new PanZoomController(this);
    }

    public void setRoot(Layer layer) { mRootLayer = layer; }

    public void setLayerClient(LayerClient layerClient) {
        mLayerClient = layerClient;
        mPageSize = layerClient.getPageSize();
        layerClient.setLayerController(this);
    }

    public Layer getRoot()          { return mRootLayer; }
    public LayerView getView()      { return mView; }
    public Context getContext()     { return mContext; }
    public IntRect getVisibleRect() { return mVisibleRect; }
    public IntSize getScreenSize()  { return mScreenSize; }
    public IntSize getPageSize()    { return mPageSize; }

    public Bitmap getBackgroundPattern()    { return getDrawable("pattern"); }
    public Bitmap getCheckerboardPattern()  { return getDrawable("checkerboard"); }
    public Bitmap getShadowPattern()        { return getDrawable("shadow"); }

    private Bitmap getDrawable(String name) {
        Resources resources = mContext.getResources();
        int resourceID = resources.getIdentifier(name, "drawable", mContext.getPackageName());
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inScaled = false;
        return BitmapFactory.decodeResource(mContext.getResources(), resourceID, options);
    }

    /*
     * Note that the zoom factor of the layer controller differs from the zoom factor of the layer
     * client (i.e. the page).
     */
    public float getZoomFactor() { return (float)mScreenSize.width / (float)mVisibleRect.width; }

    /**
     * The view calls this to indicate that the screen changed size.
     *
     * TODO: Refactor this to use an interface. Expose that interface only to the view and not
     * to the layer client. That way, the layer client won't be tempted to call this, which might
     * result in an infinite loop.
     */
    public void setScreenSize(int width, int height) {
        float zoomFactor = getZoomFactor();     /* Must come first. */

        mScreenSize = new IntSize(width, height);
        setVisibleRect(mVisibleRect.x, mVisibleRect.y,
                       (int)Math.round((float)width / zoomFactor),
                       (int)Math.round((float)height / zoomFactor));

        notifyLayerClientOfGeometryChange();
    }

    public void setNeedsDisplay() {
        // TODO
    }

    public void scrollTo(int x, int y) {
        setVisibleRect(x, y, mVisibleRect.width, mVisibleRect.height);
    }

    public void setVisibleRect(int x, int y, int width, int height) {
        mVisibleRect = new IntRect(x, y, width, height);
        setNeedsDisplay();
    }

    /**
     * Sets the zoom factor to 1, adjusting the visible rect accordingly. The Gecko layer client
     * calls this function after a zoom has completed and Gecko is done rendering the new visible
     * region.
     */
    public void unzoom() {
        float zoomFactor = getZoomFactor();
        mVisibleRect = new IntRect((int)Math.round(mVisibleRect.x * zoomFactor),
                                   (int)Math.round(mVisibleRect.y * zoomFactor),
                                   mScreenSize.width, mScreenSize.height);
        mPageSize = mPageSize.scale(zoomFactor);
        setNeedsDisplay();
    }

    public void setPageSize(IntSize size) {
        mPageSize = size.scale(getZoomFactor());
        mView.notifyRendererOfPageSizeChange();
    }

    public boolean post(Runnable action) { return mView.post(action); }

    public void setOnTouchListener(OnTouchListener onTouchListener) {
        mOnTouchListener = onTouchListener;
    }

    /**
     * The view as well as the controller itself use this method to notify the layer client that
     * the geometry changed.
     */
    public void notifyLayerClientOfGeometryChange() {
        if (mLayerClient != null)
            mLayerClient.geometryChanged();
    }

    // Informs the view and the panning and zooming controller that the geometry changed.
    public void notifyViewOfGeometryChange() {
        mView.geometryChanged();
        mPanZoomController.geometryChanged();
    }

    /**
     * Returns true if this controller is fine with performing a redraw operation or false if it
     * would prefer that the action didn't take place.
     */
    public boolean getRedrawHint() {
        if (checkerboarding() || mPanZoomController.getRedrawHint()) {
            Log.e("Fennec", "### checkerboarding? " + checkerboarding() + " pan/zoom? " +
                  mPanZoomController.getRedrawHint());
        }
        return checkerboarding() || mPanZoomController.getRedrawHint();
    }

    private IntRect getTileRect() {
        return new IntRect(mRootLayer.origin.x, mRootLayer.origin.y, TILE_WIDTH, TILE_HEIGHT);
    }

    // Returns true if a checkerboard is visible.
    private boolean checkerboarding() {
        return !getTileRect().contains(mVisibleRect);
    }

    /*
     * Gesture detection. This is handled only at a high level in this class; we dispatch to the
     * pan/zoom controller to do the dirty work.
     */

    public boolean onTouchEvent(MotionEvent event) {
        boolean result = mPanZoomController.onTouchEvent(event);
        if (mOnTouchListener != null)
            result = mOnTouchListener.onTouch(mView, event) || result;
        return result;
    }

    @Override
    public boolean onScale(ScaleGestureDetector detector) {
        return mPanZoomController.onScale(detector);
    }

    @Override
    public boolean onScaleBegin(ScaleGestureDetector detector) {
        return mPanZoomController.onScaleBegin(detector);
    }

    @Override
    public void onScaleEnd(ScaleGestureDetector detector) {
        mPanZoomController.onScaleEnd(detector);
    }

    /**
     * Objects that wish to listen for changes in the layer geometry (visible rect or screen size)
     * should implement this interface and register themselves with addOnGeometryChangeListener().
     */
    public static interface OnGeometryChangeListener {
        public void onGeometryChange(LayerController sender);
    }

    /**
     * Objects that wish to listen for changes in the page size should implement this interface and
     * register themselves with addOnPageSizeChangeListener().
     */
    public static interface OnPageSizeChangeListener {
        public void onPageSizeChange(LayerController sender);
    }
}

