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

import java.util.ArrayList;

import android.util.Log;
import android.content.Context;
import android.widget.PopupWindow;
import android.widget.LinearLayout;

public class DoorHanger {
    private Context mContext;
    public ArrayList<DoorHangerPopup> mPopups;

    private final int POPUP_VERTICAL_SPACING = 10;
    private final int POPUP_VERTICAL_OFFSET = 10;

    public DoorHanger(Context aContext) {
        mContext = aContext;
        mPopups = new ArrayList<DoorHangerPopup>();
    }

    public DoorHangerPopup getPopup() {
        final DoorHangerPopup dhp = new DoorHangerPopup(mContext);
        mPopups.add(dhp);
        return dhp;
    }

    public void removeForTab(int tabId) {
        Log.i("DoorHanger", "removeForTab: " + tabId);
        for (final DoorHangerPopup dhp : mPopups) {
            if (dhp.mTabId == tabId) {
                mPopups.remove(dhp);
            }
        }
    }

    public void updateForTab(int tabId) {
        Log.i("DoorHanger", "updateForTab: " + tabId);
        int yOffset = POPUP_VERTICAL_OFFSET;
        for (final DoorHangerPopup dhp : mPopups) {
            if (dhp.mTabId == tabId) {
                dhp.setOnDismissListener(new PopupWindow.OnDismissListener() {
                    @Override
                    public void onDismiss() {
                        mPopups.remove(dhp);
                    }
                });
                dhp.showAtHeight(POPUP_VERTICAL_SPACING + yOffset);
                yOffset += dhp.getHeight() + POPUP_VERTICAL_SPACING;
            } else {
                dhp.setOnDismissListener(null);
                dhp.dismiss();
            }
        }
    }
}
