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

import org.mozilla.fennec.gfx.IntPoint;
import org.json.JSONException;
import org.json.JSONObject;

public class IntRect implements Cloneable {
    public final int x, y, width, height;

    public IntRect(int inX, int inY, int inWidth, int inHeight) {
        x = inX; y = inY; width = inWidth; height = inHeight;
    }

    public IntRect(JSONObject json) {
        try {
            x = json.getInt("x");
            y = json.getInt("y");
            width = json.getInt("width");
            height = json.getInt("height");
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public Object clone() { return new IntRect(x, y, width, height); }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof IntRect))
            return false;
        IntRect otherRect = (IntRect)other;
        return x == otherRect.x && y == otherRect.y && width == otherRect.width &&
            height == otherRect.height;
    }

    @Override
    public String toString() { return "(" + x + "," + y + "," + width + "," + height + ")"; }

    public IntPoint getOrigin() { return new IntPoint(x, y); }
    public IntPoint getCenter() { return new IntPoint(x + width / 2, y + height / 2); }

    public int getRight() { return x + width; }
    public int getBottom() { return y + height; }

    /** Scales all four dimensions of this rectangle by the given factor. */
    public IntRect scaleAll(float factor) {
        return new IntRect((int)Math.round(x * factor),
                           (int)Math.round(y * factor),
                           (int)Math.round(width * factor),
                           (int)Math.round(height * factor));
    }

    /** Returns true if and only if the given rectangle is fully enclosed within this one. */
    public boolean contains(IntRect other) {
        return x <= other.x && y <= other.y &&
               getRight() >= other.getRight() &&
               getBottom() >= other.getBottom();
    }

    /** Returns the intersection of this rectangle with another rectangle. */
    public IntRect intersect(IntRect other) {
        int left = Math.max(x, other.x);
        int top = Math.max(y, other.y);
        int right = Math.min(getRight(), other.getRight());
        int bottom = Math.min(getBottom(), other.getBottom());
        return new IntRect(left, top, Math.max(right - left, 0), Math.max(bottom - top, 0));
    }
}


