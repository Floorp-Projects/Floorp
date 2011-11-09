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
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
 *   Sriram Ramasubramanian <sriram@mozilla.com>
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

import java.util.HashMap;
import java.util.Iterator;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.PopupWindow;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.RelativeLayout;

public class DoorHangerPopup extends PopupWindow {
    private Context mContext;
    private LinearLayout mContent;

    public DoorHangerPopup(Context aContext) {
        super(aContext);
        mContext = aContext;

        setBackgroundDrawable(new BitmapDrawable());
        setWindowLayoutMode(ViewGroup.LayoutParams.FILL_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

        LayoutInflater inflater = (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        RelativeLayout layout = (RelativeLayout) inflater.inflate(R.layout.doorhangerpopup, null);
        mContent = (LinearLayout) layout.findViewById(R.id.doorhanger_container);
        
        setContentView(layout);
    }

    public DoorHanger addDoorHanger(Tab tab, String value) {
        Log.i("DoorHangerPopup", "Adding a DoorHanger to Tab: " + tab.getId());

        DoorHanger dh = tab.getDoorHanger(value);
        if (dh != null) {
            dh.hidePopup();
            tab.removeDoorHanger(value);
        }

        dh = new DoorHanger(mContent.getContext(), value);
        dh.setTab(tab);
        tab.addDoorHanger(value, dh);
        mContent.addView(dh);
        
        return dh;
    }

    public void removeDoorHanger(Tab tab, String value) {
        Log.i("DoorHangerPopup", "Removing a DoorHanger from Tab: " + tab.getId());
        tab.removeDoorHanger(value);

        if (tab.getDoorHangers().size() == 0)
            hide();
        else
            fixBackgroundForFirst(); 
    }

    public void showDoorHanger(DoorHanger dh) {
        if (dh == null)
            return;

        dh.showPopup();
        show();
    }

    public void hideDoorHanger(DoorHanger dh) {
        if (dh == null)
            return;

        dh.hidePopup();
        show();
    }

    public void hideAllDoorHangers() {
        hideAllDoorHangersExcept(null);
    }

    public void hideAllDoorHangersExcept(Tab tab) {
        for (int i=0; i < mContent.getChildCount(); i++) {
            DoorHanger dh = (DoorHanger) mContent.getChildAt(i);
            if (dh.getTab() != tab)
                dh.hidePopup();
        }

        if (tab == null)
            hide();
    }
    
    public void hide() {
        if (isShowing()) {
            Log.i("DoorHangerPopup", "Dismissing the DoorHangerPopup");
            dismiss();
        }
    }

    public void show() {
        Log.i("DoorHangerPopup", "Showing the DoorHangerPopup");
        fixBackgroundForFirst();

        if (isShowing())
            update();
        else
            showAsDropDown(GeckoApp.mBrowserToolbar.mFavicon);
    }

    public void removeForTab(Tab tab) {
        Log.i("DoorHangerPopup", "Removing all doorhangers for tab: " + tab.getId());
        tab.removeAllDoorHangers();
    }

    public void showForTab(Tab tab) {
        Log.i("DoorHangerPopup", "Showing all doorhangers for tab: " + tab.getId());
        HashMap<String, DoorHanger> doorHangers = tab.getDoorHangers();

        if (doorHangers == null) {
            hide();
            return;
        }

        hideAllDoorHangersExcept(tab);

        Iterator keys = doorHangers.keySet().iterator();
        while (keys.hasNext()) {
            ((DoorHanger) doorHangers.get(keys.next())).showPopup();
        }

        if (doorHangers.size() > 0)
            show();
        else
            hide();
    }

    public void hideForTab(Tab tab) {
        Log.i("DoorHangerPopup", "Hiding all doorhangers for tab: " + tab.getId());
        HashMap<String, DoorHanger> doorHangers = tab.getDoorHangers();

        if (doorHangers == null)
            return;

        Iterator keys = doorHangers.keySet().iterator();
        while (keys.hasNext()) {
            ((DoorHanger) doorHangers.get(keys.next())).hidePopup();
        }
    }

    private void fixBackgroundForFirst() {
        for (int i=0; i < mContent.getChildCount(); i++) {
            DoorHanger dh = (DoorHanger) mContent.getChildAt(i);
            if (dh.isVisible()) {
                dh.setBackgroundResource(R.drawable.doorhanger_bg);
                break;
            }
        }
    }
}
