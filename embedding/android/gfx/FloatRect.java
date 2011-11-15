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

import android.graphics.PointF;
import org.mozilla.gecko.gfx.IntRect;

public class FloatRect {
    public final float x, y, width, height;

    public FloatRect(float inX, float inY, float inWidth, float inHeight) {
        x = inX; y = inY; width = inWidth; height = inHeight;
    }

    public FloatRect(IntRect intRect) {
        x = intRect.x; y = intRect.y; width = intRect.width; height = intRect.height;
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof FloatRect))
            return false;
        FloatRect otherRect = (FloatRect)other;
        return x == otherRect.x && y == otherRect.y &&
            width == otherRect.width && height == otherRect.height;
    }

    public float getRight() { return x + width; }
    public float getBottom() { return y + height; }

    public PointF getOrigin() { return new PointF(x, y); }
    public PointF getCenter() { return new PointF(x + width / 2, y + height / 2); }

    /** Returns the intersection of this rectangle with another rectangle. */
    public FloatRect intersect(FloatRect other) {
        float left = Math.max(x, other.x);
        float top = Math.max(y, other.y);
        float right = Math.min(getRight(), other.getRight());
        float bottom = Math.min(getBottom(), other.getBottom());
        return new FloatRect(left, top, Math.max(right - left, 0), Math.max(bottom - top, 0));
    }

    /** Returns true if and only if the given rectangle is fully enclosed within this one. */
    public boolean contains(FloatRect other) {
        return x <= other.x && y <= other.y &&
               getRight() >= other.getRight() &&
               getBottom() >= other.getBottom();
    }

    /** Contracts a rectangle by the given number of units in each direction, from the center. */
    public FloatRect contract(float lessWidth, float lessHeight) {
        float halfWidth = width / 2.0f - lessWidth, halfHeight = height / 2.0f - lessHeight;
        PointF center = getCenter();
        return new FloatRect(center.x - halfWidth, center.y - halfHeight,
                             halfWidth * 2.0f, halfHeight * 2.0f);
    }

    /** Scales all four dimensions of this rectangle by the given factor. */
    public FloatRect scaleAll(float factor) {
        return new FloatRect(x * factor, y * factor, width * factor, height * factor);
    }
}

