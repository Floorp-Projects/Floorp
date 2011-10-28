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
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *   Vivien Nicolas <vnicolas@mozilla.com>
 *   Lucas Rocha <lucasr@mozilla.com>
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
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ProgressBar;

public class BrowserToolbar extends LinearLayout {
    final private ProgressBar mProgressBar;
    final private Button mAwesomeBar;
    final private ImageButton mTabs;
    final private ImageButton mFavicon;

    public BrowserToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Load layout into the custom view
        LayoutInflater inflater =
                (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        inflater.inflate(R.layout.browser_toolbar, this);

        mProgressBar = (ProgressBar) findViewById(R.id.progress_bar);

        mAwesomeBar = (Button) findViewById(R.id.awesome_bar);
        mAwesomeBar.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                onAwesomeBarSearch();
            }
        });

        mTabs = (ImageButton) findViewById(R.id.tabs);
        mTabs.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                if (Tabs.getInstance().getCount() > 1)
                    showTabs();
                else
                    addTab();
            }
        });

        mFavicon = (ImageButton) findViewById(R.id.favicon);
    }

    private void onAwesomeBarSearch() {
        GeckoApp.mAppContext.onEditRequested();
    }

    private void addTab() {
        GeckoApp.mAppContext.addTab();
    }

    private void showTabs() {
        GeckoApp.mAppContext.showTabs();
    }
    
    public void updateTabs(int count) {
        if (count == 1)
            mTabs.setImageResource(R.drawable.tabs_plus);
        else
            mTabs.setImageResource(R.drawable.tabs_menu);
    }

    public void updateProgress(int progress, int total) {
        mProgressBar.setVisibility(View.VISIBLE);
        mFavicon.setVisibility(View.GONE);
    }

    public void setProgressVisibility(boolean visible) {
        mProgressBar.setVisibility(visible ? View.VISIBLE : View.GONE);
        mFavicon.setVisibility(visible ? View.GONE : View.VISIBLE);
    }

    public void setTitle(CharSequence title) {
        mAwesomeBar.setText(title);
    }

    public void setFavicon(Drawable image) {
        if (image != null)
            mFavicon.setImageDrawable(image);
        else
            mFavicon.setImageResource(R.drawable.favicon);
    }
}
