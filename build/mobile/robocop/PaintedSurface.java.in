#filter substitution
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package @ANDROID_PACKAGE_NAME@;

import java.io.BufferedWriter;
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
        int b1 = mPixelBuffer.get(index) & 0xFF;
        int b2 = mPixelBuffer.get(index + 1) & 0xFF;
        int b3 = mPixelBuffer.get(index + 2) & 0xFF;
        int b4 = mPixelBuffer.get(index + 3) & 0xFF;
        int value = (b1 << 24) + (b2 << 16) + (b3 << 8) + (b4 << 0);
        return value;
    }

    public void close() {
        try {
            mPixelFile.close();
        } catch (Exception e) {
            FennecNativeDriver.log(FennecNativeDriver.LogLevel.DEBUG, e);
        }
    }
}
