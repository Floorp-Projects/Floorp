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

import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.TileLayer;
import javax.microedition.khronos.opengles.GL10;
import java.nio.FloatBuffer;

/**
 * Encapsulates the logic needed to draw a nine-patch bitmap using OpenGL ES.
 *
 * For more information on nine-patch bitmaps, see the following document:
 *   http://developer.android.com/guide/topics/graphics/2d-graphics.html#nine-patch
 */
public class NinePatchTileLayer extends TileLayer {
    private FloatBuffer mSideTexCoordBuffer, mSideVertexBuffer;
    private FloatBuffer mTopTexCoordBuffer, mTopVertexBuffer;
    private LayerController mLayerController;

    private static final int PATCH_SIZE = 16;
    private static final int TEXTURE_SIZE = 48;

    /*
     * We divide the nine-patch bitmap up into the "sides" and the "tops":
     *
     *        Top
     *         |
     *         v
     *   +---+---+---+
     *   |   |   |   |
     *   |   +---+   |
     *   |   |XXX|   | <-- Side
     *   |   +---+   |
     *   |   |   |   |
     *   +---+---+---+
     */

    private static final float[] SIDE_TEX_COORDS = {
        0.0f,   0.0f,
        0.25f,  0.0f,
        0.0f,   0.25f,
        0.25f,  0.25f,
        0.0f,   0.50f,
        0.25f,  0.50f,
        0.0f,   0.75f,
        0.25f,  0.75f,
    };

    private static final float[] TOP_TEX_COORDS = {
        0.25f,  0.0f,
        0.50f,  0.0f,
        0.25f,  0.25f,
        0.50f,  0.25f,
    };

    public NinePatchTileLayer(LayerController layerController) {
        super(false);

        mLayerController = layerController;

        mSideTexCoordBuffer = createBuffer(SIDE_TEX_COORDS);
        mTopTexCoordBuffer = createBuffer(TOP_TEX_COORDS);

        recreateVertexBuffers();
    }

    public void recreateVertexBuffers() {
        IntSize pageSize = mLayerController.getPageSize();

        float[] sideVertices = {
            -PATCH_SIZE,    -PATCH_SIZE,                    0.0f,
            0.0f,           -PATCH_SIZE,                    0.0f,
            -PATCH_SIZE,    0.0f,                           0.0f,
            0.0f,           0.0f,                           0.0f,
            -PATCH_SIZE,    pageSize.height,                0.0f,
            0.0f,           pageSize.height,                0.0f,
            -PATCH_SIZE,    PATCH_SIZE + pageSize.height,   0.0f,
            0.0f,           PATCH_SIZE + pageSize.height,   0.0f
        };

        float[] topVertices = {
            0.0f,           -PATCH_SIZE,    0.0f,
            pageSize.width, -PATCH_SIZE,    0.0f,
            0.0f,           0.0f,           0.0f,
            pageSize.width, 0.0f,           0.0f
        };

        mSideVertexBuffer = createBuffer(sideVertices);
        mTopVertexBuffer = createBuffer(topVertices);
    }

    @Override
    protected void onTileDraw(GL10 gl) {
        IntSize pageSize = mLayerController.getPageSize();

        gl.glBlendFunc(GL10.GL_SRC_ALPHA, GL10.GL_ONE_MINUS_SRC_ALPHA);
        gl.glEnable(GL10.GL_BLEND);

        gl.glBindTexture(GL10.GL_TEXTURE_2D, getTextureID());

        /* Left side */
        drawTriangles(gl, mSideVertexBuffer, mSideTexCoordBuffer, 8);

        /* Top */
        drawTriangles(gl, mTopVertexBuffer, mTopTexCoordBuffer, 4);

        /* Right side */
        gl.glMatrixMode(GL10.GL_MODELVIEW);
        gl.glPushMatrix();
        gl.glTranslatef(pageSize.width + PATCH_SIZE, 0.0f, 0.0f);
        gl.glMatrixMode(GL10.GL_TEXTURE);
        gl.glPushMatrix();
        gl.glTranslatef(0.50f, 0.0f, 0.0f);

        drawTriangles(gl, mSideVertexBuffer, mSideTexCoordBuffer, 8);

        gl.glMatrixMode(GL10.GL_TEXTURE);
        gl.glPopMatrix();
        gl.glMatrixMode(GL10.GL_MODELVIEW);     /* Not strictly necessary, but here for clarity. */
        gl.glPopMatrix();

        /* Bottom */
        gl.glMatrixMode(GL10.GL_MODELVIEW);
        gl.glPushMatrix();
        gl.glTranslatef(0.0f, pageSize.height + PATCH_SIZE, 0.0f);
        gl.glMatrixMode(GL10.GL_TEXTURE);
        gl.glPushMatrix();
        gl.glTranslatef(0.0f, 0.50f, 0.0f);

        drawTriangles(gl, mTopVertexBuffer, mTopTexCoordBuffer, 4);

        gl.glMatrixMode(GL10.GL_TEXTURE);
        gl.glPopMatrix();
        gl.glMatrixMode(GL10.GL_MODELVIEW);
        gl.glPopMatrix();

        gl.glDisable(GL10.GL_BLEND);
    }
}
