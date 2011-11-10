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
import org.mozilla.gecko.gfx.Layer;
import org.mozilla.gecko.gfx.TextureReaper;
import android.util.Log;
import javax.microedition.khronos.opengles.GL10;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

/**
 * Base class for tile layers, which encapsulate the logic needed to draw textured tiles in OpenGL
 * ES.
 */
public abstract class TileLayer extends Layer {
    private CairoImage mImage;
    private boolean mRepeat;
    private IntSize mSize;
    private int[] mTextureIDs;

    private IntRect mTextureUploadRect;
    /* The rect that needs to be uploaded to the texture. */

    public TileLayer(boolean repeat) {
        super();
        mRepeat = repeat;
        mTextureUploadRect = null;
    }

    public IntSize getSize() { return mSize; }

    protected boolean repeats() { return mRepeat; }
    protected int getTextureID() { return mTextureIDs[0]; }

    @Override
    protected void finalize() throws Throwable {
        if (mTextureIDs != null)
            TextureReaper.get().add(mTextureIDs);
    }

    /**
     * Subclasses implement this method to perform tile drawing.
     *
     * Invariant: The current matrix mode must be GL_MODELVIEW both before and after this call.
     */
    protected abstract void onTileDraw(GL10 gl);

    @Override
    protected void onDraw(GL10 gl) {
        if (mImage == null)
            return;
        if (mTextureUploadRect != null)
            uploadTexture(gl);

        gl.glEnableClientState(GL10.GL_VERTEX_ARRAY);
        gl.glEnableClientState(GL10.GL_TEXTURE_COORD_ARRAY);
        gl.glPushMatrix();

        onTileDraw(gl);

        gl.glPopMatrix();
        gl.glDisableClientState(GL10.GL_VERTEX_ARRAY);
    }

    public void paintSubimage(CairoImage image, IntRect rect) {
        mImage = image;
        mTextureUploadRect = rect;

        /*
         * Assert that the image has a power-of-two size. OpenGL ES < 2.0 doesn't support NPOT
         * textures and OpenGL ES doesn't seem to let us efficiently slice up a NPOT bitmap.
         */
        int width = mImage.getWidth(), height = mImage.getHeight();
        assert (width & (width - 1)) == 0;
        assert (height & (height - 1)) == 0;
    }

    public void paintImage(CairoImage image) {
        paintSubimage(image, new IntRect(0, 0, image.getWidth(), image.getHeight()));
    }

    private void uploadTexture(GL10 gl) {
        boolean newTexture = mTextureIDs == null;
        if (newTexture) {
            mTextureIDs = new int[1];
            gl.glGenTextures(mTextureIDs.length, mTextureIDs, 0);
        }

        int width = mImage.getWidth(), height = mImage.getHeight();
        mSize = new IntSize(width, height);

        int cairoFormat = mImage.getFormat();
        int internalFormat = CairoUtils.cairoFormatToGLInternalFormat(cairoFormat);
        int format = CairoUtils.cairoFormatToGLFormat(cairoFormat);
        int type = CairoUtils.cairoFormatToGLType(cairoFormat);

        gl.glBindTexture(GL10.GL_TEXTURE_2D, mTextureIDs[0]);
        gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MIN_FILTER, GL10.GL_NEAREST);
        gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MAG_FILTER, GL10.GL_LINEAR);

        int repeatMode = mRepeat ? GL10.GL_REPEAT : GL10.GL_CLAMP_TO_EDGE;
        gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_S, repeatMode);
        gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_T, repeatMode);

        ByteBuffer buffer = mImage.lockBuffer();
        try {
            if (newTexture) {
                /* The texture is new; we have to upload the whole image. */
                gl.glTexImage2D(gl.GL_TEXTURE_2D, 0, internalFormat, mSize.width, mSize.height, 0,
                                format, type, buffer);
            } else {
                /*
                 * The texture is already existing, so upload only the changed rect. We have to
                 * widen to the full width of the texture because we can't count on the device
                 * having support for GL_EXT_unpack_subimage, and going line-by-line is too slow.
                 */
                Buffer viewBuffer = buffer.slice();
                int bpp = CairoUtils.bitsPerPixelForCairoFormat(cairoFormat) / 8;
                viewBuffer.position(mTextureUploadRect.y * width * bpp);

                gl.glTexSubImage2D(gl.GL_TEXTURE_2D,
                                   0, 0, mTextureUploadRect.y, width, mTextureUploadRect.height,
                                   format, type, viewBuffer);
            }
        } finally {
            mImage.unlockBuffer();
        }

        mTextureUploadRect = null;
    }

    protected static FloatBuffer createBuffer(float[] values) {
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(values.length * 4);
        byteBuffer.order(ByteOrder.nativeOrder());

        FloatBuffer floatBuffer = byteBuffer.asFloatBuffer();
        floatBuffer.put(values);
        floatBuffer.position(0);
        return floatBuffer;
    }

    protected static void drawTriangles(GL10 gl, FloatBuffer vertexBuffer,
                                        FloatBuffer texCoordBuffer, int count) {
        gl.glVertexPointer(3, GL10.GL_FLOAT, 0, vertexBuffer);
        gl.glTexCoordPointer(2, GL10.GL_FLOAT, 0, texCoordBuffer);
        gl.glDrawArrays(GL10.GL_TRIANGLE_STRIP, 0, count);
    }
}

