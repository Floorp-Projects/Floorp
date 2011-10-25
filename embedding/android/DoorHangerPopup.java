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

import android.content.Context;
import android.widget.TextView;
import android.widget.Button;
import android.widget.PopupWindow;
import android.view.Display;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

public class DoorHangerPopup extends PopupWindow {
    private Context mContext;
    private LinearLayout mChoicesLayout;
    private TextView mTextView;
    private Button mButton;
    private LayoutParams mLayoutParams;
    private View popupView;
    public int mTabId;
    private final int POPUP_VERTICAL_SIZE = 100;

    public DoorHangerPopup(Context aContext) {
        super(aContext);
        mContext = aContext;

        LayoutInflater inflater =
                (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        popupView = (View) inflater.inflate(R.layout.doorhangerpopup, null);
        setContentView(popupView);

        mChoicesLayout = (LinearLayout) popupView.findViewById(R.id.doorhanger_choices);
        mTextView = (TextView) popupView.findViewById(R.id.doorhanger_title);

        mLayoutParams = new LayoutParams(LayoutParams.WRAP_CONTENT,
                                         LayoutParams.WRAP_CONTENT);
    }

    public void addButton(String aText, int aCallback) {
        final String sCallback = Integer.toString(aCallback);

        Button mButton = new Button(mContext);
        mButton.setText(aText);
        mButton.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                GeckoEvent e = new GeckoEvent("Doorhanger:Reply", sCallback);
                GeckoAppShell.sendEventToGecko(e);
                dismiss();
            }
        });
        mChoicesLayout.addView(mButton, mLayoutParams);
    }

    public void setText(String aText) {
        mTextView.setText(aText);
    }

    public void setTab(int tabId) {
        mTabId = tabId;
    }

    public void showAtHeight(int y) {
        Display display = ((WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();

        int width = display.getWidth();
        int height = display.getHeight();
        showAsDropDown(popupView);
        update(0, height - POPUP_VERTICAL_SIZE - y,
               width, POPUP_VERTICAL_SIZE);
    }
}
