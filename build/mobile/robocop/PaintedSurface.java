/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.graphics.Bitmap;
import android.util.Base64;
import android.util.Base64OutputStream;

import java.io.BufferedWriter;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;

public class PaintedSurface {
    private String mFileName;
    private int mWidth;
    private int mHeight;
    private FileInputStream mPixelFile;
    private MappedByteBuffer mPixelBuffer;

    public PaintedSurface(String filename, int width, int height) {
        mFileName = filename;
        mWidth = width;
        mHeight = height;

        try {
            File f = new File(filename);
            int pixelSize = (int)f.length();

            mPixelFile = new FileInputStream(filename);
            mPixelBuffer = mPixelFile.getChannel().map(FileChannel.MapMode.READ_ONLY, 0, pixelSize);
        } catch (java.io.FileNotFoundException e) {
            FennecNativeDriver.log(FennecNativeDriver.LogLevel.ERROR, e);
        } catch (java.io.IOException e) {
            FennecNativeDriver.log(FennecNativeDriver.LogLevel.ERROR, e);
        }
    }

    public final int getWidth() {
        return mWidth;
    }

    public final int getHeight() {
        return mHeight;
    }

    private int pixelAtIndex(int index) {
        int b1 = mPixelBuffer.get(index) & 0xFF;
        int b2 = mPixelBuffer.get(index + 1) & 0xFF;
        int b3 = mPixelBuffer.get(index + 2) & 0xFF;
        int b4 = mPixelBuffer.get(index + 3) & 0xFF;
        int value = (b1 << 24) + (b2 << 16) + (b3 << 8) + (b4 << 0);
        return value;
    }

    public final int getPixelAt(int x, int y) {
        if (mPixelBuffer == null) {
            throw new RoboCopException("Trying to access PaintedSurface with no active PixelBuffer");
        }

        if (x >= mWidth || x < 0) {
            throw new RoboCopException("Trying to access PaintedSurface with invalid x value");
        }

        if (y >= mHeight || y < 0) {
            throw new RoboCopException("Trying to access PaintedSurface with invalid y value");
        }

        // The rows are reversed so row 0 is at the end and we start with the last row.
        // This is why we do mHeight-y;
        int index = (x + ((mHeight - y - 1) * mWidth)) * 4;
        return pixelAtIndex(index);
    }

    public final String asDataUri() {
        try {
            Bitmap bm = Bitmap.createBitmap(mWidth, mHeight, Bitmap.Config.ARGB_8888);
            for (int y = 0; y < mHeight; y++) {
                for (int x = 0; x < mWidth; x++) {
                    int index = (x + ((mHeight - y - 1) * mWidth)) * 4;
                    bm.setPixel(x, y, pixelAtIndex(index));
                }
            }
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            out.write("data:image/png;base64,".getBytes());
            Base64OutputStream b64 = new Base64OutputStream(out, Base64.NO_WRAP);
            bm.compress(Bitmap.CompressFormat.PNG, 100, b64);
            return new String(out.toByteArray());
        } catch (Exception e) {
            FennecNativeDriver.log(FennecNativeDriver.LogLevel.ERROR, e);
            throw new RoboCopException("Unable to convert surface to a PNG data:uri");
        }
    }

    public void close() {
        try {
            mPixelFile.close();
        } catch (Exception e) {
            FennecNativeDriver.log(FennecNativeDriver.LogLevel.DEBUG, e);
        }
    }
}
