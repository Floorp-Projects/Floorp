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

import org.mozilla.fennec.gfx.BufferedCairoImage;
import org.mozilla.fennec.gfx.CairoUtils;
import org.mozilla.fennec.gfx.IntSize;
import org.mozilla.fennec.gfx.LayerClient;
import org.mozilla.fennec.gfx.SingleTileLayer;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Environment;
import android.util.Log;
import java.io.File;
import java.nio.ByteBuffer;

/**
 * A stand-in for Gecko that renders cached content of the previous page. We use this until Gecko
 * is up, then we hand off control to it.
 */
public class PlaceholderLayerClient extends LayerClient {
    private Context mContext;
    private IntSize mPageSize;
    private int mWidth, mHeight, mFormat;
    private ByteBuffer mBuffer;

    private PlaceholderLayerClient(Context context, Bitmap bitmap) {
        mContext = context;
        mPageSize = new IntSize(995, 1250); /* TODO */

        mWidth = bitmap.getWidth();
        mHeight = bitmap.getHeight();
        mFormat = CairoUtils.bitmapConfigToCairoFormat(bitmap.getConfig());
        mBuffer = ByteBuffer.allocateDirect(mWidth * mHeight * 4);
        bitmap.copyPixelsToBuffer(mBuffer.asIntBuffer());
    }

    public static PlaceholderLayerClient createInstance(Context context) {
        File path = new File(Environment.getExternalStorageDirectory(), "lastScreen.png");
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inScaled = false;
        Bitmap bitmap = BitmapFactory.decodeFile("" + path, options);
        if (bitmap == null)
            return null;

        return new PlaceholderLayerClient(context, bitmap);
    }

    public void init() {
        SingleTileLayer tileLayer = new SingleTileLayer();
        getLayerController().setRoot(tileLayer);
        tileLayer.paintImage(new BufferedCairoImage(mBuffer, mWidth, mHeight, mFormat));
    }

    @Override
    public void geometryChanged() { /* no-op */ }
    @Override
    public IntSize getPageSize() { return mPageSize; }
    @Override
    public void render() { /* no-op */ }

    /** Called whenever the page changes size. */
    @Override
    public void setPageSize(IntSize pageSize) { mPageSize = pageSize; }
}

