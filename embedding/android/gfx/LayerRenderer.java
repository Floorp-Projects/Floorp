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

import org.mozilla.gecko.gfx.BufferedCairoImage;
import org.mozilla.gecko.gfx.FloatRect;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.NinePatchTileLayer;
import org.mozilla.gecko.gfx.SingleTileLayer;
import org.mozilla.gecko.gfx.TextureReaper;
import org.mozilla.gecko.gfx.TextLayer;
import org.mozilla.gecko.gfx.TileLayer;
import android.content.Context;
import android.graphics.Rect;
import android.opengl.GLSurfaceView;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.WindowManager;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import java.nio.ByteBuffer;

/**
 * The layer renderer implements the rendering logic for a layer view.
 */
public class LayerRenderer implements GLSurfaceView.Renderer {
    private static final float BACKGROUND_COLOR_R = 0.81f;
    private static final float BACKGROUND_COLOR_G = 0.81f;
    private static final float BACKGROUND_COLOR_B = 0.81f;

    private LayerView mView;
    private SingleTileLayer mCheckerboardLayer;
    private NinePatchTileLayer mShadowLayer;
    private TextLayer mFPSLayer;

    // FPS display
    private long mFrameCountTimestamp;
    private int mFrameCount;            // number of frames since last timestamp

    public LayerRenderer(LayerView view) {
        mView = view;

        /* FIXME: Layers should not be directly connected to the layer controller. */
        LayerController controller = view.getController();
        mCheckerboardLayer = new SingleTileLayer(true);
        mCheckerboardLayer.paintImage(new BufferedCairoImage(controller.getCheckerboardPattern()));
        mShadowLayer = new NinePatchTileLayer(controller);
        mShadowLayer.paintImage(new BufferedCairoImage(controller.getShadowPattern()));
        mFPSLayer = new TextLayer(new IntSize(64, 32));
        mFPSLayer.setText("-- FPS");

        mFrameCountTimestamp = System.currentTimeMillis();
        mFrameCount = 0;
    }

    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        gl.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl.glClearDepthf(1.0f);             /* FIXME: Is this needed? */
        gl.glHint(GL10.GL_PERSPECTIVE_CORRECTION_HINT, GL10.GL_FASTEST);
        gl.glShadeModel(GL10.GL_SMOOTH);    /* FIXME: Is this needed? */
        gl.glDisable(GL10.GL_DITHER);
        gl.glEnable(GL10.GL_TEXTURE_2D);
    }

    public void onDrawFrame(GL10 gl) {
        checkFPS();
        TextureReaper.get().reap(gl);

        LayerController controller = mView.getController();

        /* Draw the background. */
        gl.glClearColor(BACKGROUND_COLOR_R, BACKGROUND_COLOR_G, BACKGROUND_COLOR_B, 1.0f);
        gl.glClear(GL10.GL_COLOR_BUFFER_BIT);

        /* Draw the drop shadow. */
        setupPageTransform(gl);
        mShadowLayer.draw(gl);

        /* Draw the checkerboard. */
        Rect pageRect = clampToScreen(getPageRect());
        IntSize screenSize = controller.getScreenSize();
        gl.glEnable(GL10.GL_SCISSOR_TEST);
        gl.glScissor(pageRect.left, screenSize.height - pageRect.bottom,
                     pageRect.width(), pageRect.height());

        gl.glLoadIdentity();
        mCheckerboardLayer.draw(gl);

        /* Draw the layer the client added to us. */
        setupPageTransform(gl);

        Layer rootLayer = controller.getRoot();
        if (rootLayer != null)
            rootLayer.draw(gl);

        gl.glDisable(GL10.GL_SCISSOR_TEST);

        /* Draw the FPS. */
        gl.glLoadIdentity();
        gl.glEnable(GL10.GL_BLEND);
        mFPSLayer.draw(gl);
        gl.glDisable(GL10.GL_BLEND);
    }

    public void pageSizeChanged() {
        mShadowLayer.recreateVertexBuffers();
    }

    private void setupPageTransform(GL10 gl) {
        LayerController controller = mView.getController();
        FloatRect visibleRect = controller.getVisibleRect();
        float zoomFactor = controller.getZoomFactor();

        gl.glLoadIdentity();
        gl.glScalef(zoomFactor, zoomFactor, 1.0f);
        gl.glTranslatef(-visibleRect.x, -visibleRect.y, 0.0f);
    }

    private Rect getPageRect() {
        LayerController controller = mView.getController();
        float zoomFactor = controller.getZoomFactor();
        FloatRect visibleRect = controller.getVisibleRect();
        IntSize pageSize = controller.getPageSize();

        int x = (int)Math.round(-zoomFactor * visibleRect.x);
        int y = (int)Math.round(-zoomFactor * visibleRect.y);
        return new Rect(x, y,
                        x + (int)Math.round(zoomFactor * pageSize.width),
                        y + (int)Math.round(zoomFactor * pageSize.height));
    }

    private Rect clampToScreen(Rect rect) {
        LayerController controller = mView.getController();
        IntSize screenSize = controller.getScreenSize();

        int left = Math.max(0, rect.left);
        int top = Math.max(0, rect.top);
        int right = Math.min(screenSize.width, rect.right);
        int bottom = Math.min(screenSize.height, rect.bottom);
        return new Rect(left, top, right, bottom);
    }

    public void onSurfaceChanged(GL10 gl, int width, int height) {
        gl.glViewport(0, 0, width, height);
        gl.glMatrixMode(GL10.GL_PROJECTION);
        gl.glLoadIdentity();
        gl.glOrthof(0.0f, (float)width, (float)height, 0.0f, -10.0f, 10.0f);
        gl.glMatrixMode(GL10.GL_MODELVIEW);
        gl.glLoadIdentity();

        mView.setScreenSize(width, height);

        /* TODO: Throw away tile images? */
    }

    private void checkFPS() {
        if (System.currentTimeMillis() >= mFrameCountTimestamp + 1000) {
            mFrameCountTimestamp = System.currentTimeMillis();
            mFPSLayer.setText(mFrameCount + " FPS");
            mFrameCount = 0;
        } else {
            mFrameCount++;
        }
    }
}

