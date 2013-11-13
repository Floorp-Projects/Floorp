/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.List;
import android.app.Activity;

public interface Driver {
    /**
     * Find the first Element using the given method.
     * 
     * @param activity The activity the element belongs to
     * @param name The name of the element
     * @return The first matching element on the current context, or null if not found.
     */
    Element findElement(Activity activity, String name);

    /**
     * Sets up scroll handling so that data is received from the extension.
     */
    void setupScrollHandling();

    int getPageHeight();
    int getScrollHeight();
    int getHeight();
    int getGeckoTop();
    int getGeckoLeft();
    int getGeckoWidth();
    int getGeckoHeight();

    void startFrameRecording();
    int stopFrameRecording();

    void startCheckerboardRecording();
    float stopCheckerboardRecording();

    /**
     * Get a copy of the painted content region.
     * @return A 2-D array of pixels (indexed by y, then x). The pixels
     * are in ARGB-8888 format.
     */
    PaintedSurface getPaintedSurface();
}
