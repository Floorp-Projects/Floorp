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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Wes Johnston <wjohnston@mozilla.com>
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

package org.mozilla.gecko;

import android.view.GestureDetector;
import android.view.MotionEvent;
import android.content.Context;
import android.view.View;
import org.json.JSONArray;
import org.json.JSONObject;
import android.util.Log;

class GeckoGestureDetector implements GestureDetector.OnGestureListener {
    private GestureDetector mDetector;
    private static final String LOG_FILE_NAME = "GeckoGestureDetector";
    public GeckoGestureDetector(Context aContext) {
        mDetector = new GestureDetector(aContext, this);
    }

    public boolean onTouchEvent(MotionEvent event) {
        return mDetector.onTouchEvent(event);
    }

    @Override
    public boolean onDown(MotionEvent e) {
    	return true;
    }

    @Override
    public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
    	return true;
    }

    @Override
    public void onLongPress(MotionEvent motionEvent) {
        JSONObject ret = new JSONObject();
        try {
            ret.put("x", motionEvent.getX());
            ret.put("y", motionEvent.getY());
        } catch(Exception ex) {
            Log.w(LOG_FILE_NAME, "Error building return: " + ex);
        }

        GeckoEvent e = new GeckoEvent("Gesture:LongPress", ret.toString());
        GeckoAppShell.sendEventToGecko(e);
    }

    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
    	return true;
    }

    @Override
    public void onShowPress(MotionEvent e) {
    }
    
    @Override
    public boolean onSingleTapUp(MotionEvent e) {
    	return true;
    }
}
