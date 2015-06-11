/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.TextSwitcher;
import android.widget.TextView;

public class FennecNativeElement implements Element {
    private final Activity mActivity;
    private final Integer mId;
    private final String mName;

    public FennecNativeElement(Integer id, Activity activity) {
        mId = id;
        mActivity = activity;
        mName = activity.getResources().getResourceName(id);
    }

    @Override
    public Integer getId() {
        return mId;
    }

    private boolean mClickSuccess;

    @Override
    public boolean click() {
        mClickSuccess = false;
        RobocopUtils.runOnUiThreadSync(mActivity,
            new Runnable() {
                @Override
                public void run() {
                    View view = mActivity.findViewById(mId);
                    if (view != null) {
                        if (view.performClick()) {
                            mClickSuccess = true;
                        } else {
                            FennecNativeDriver.log(FennecNativeDriver.LogLevel.WARN,
                                "Robocop called click on an element with no listener " + mId + " " + mName);
                        }
                    } else {
                        FennecNativeDriver.log(FennecNativeDriver.LogLevel.ERROR,
                            "click: unable to find view " + mId + " " + mName);
                    }
                }
            });
        return mClickSuccess;
    }

    private Object mText;

    @Override
    public String getText() {
        mText = null;
        RobocopUtils.runOnUiThreadSync(mActivity,
            new Runnable() {
                @Override
                public void run() {
                    View v = mActivity.findViewById(mId);
                    if (v instanceof EditText) {
                        EditText et = (EditText)v;
                        mText = et.getEditableText();
                    } else if (v instanceof TextSwitcher) {
                        TextSwitcher ts = (TextSwitcher)v;
                        mText = ((TextView)ts.getCurrentView()).getText();
                    } else if (v instanceof ViewGroup) {
                        ViewGroup vg = (ViewGroup)v;
                        for (int i = 0; i < vg.getChildCount(); i++) {
                            if (vg.getChildAt(i) instanceof TextView) {
                                mText = ((TextView)vg.getChildAt(i)).getText();
                            }
                        }
                    } else if (v instanceof TextView) {
                        mText = ((TextView)v).getText(); 
                    } else if (v == null) {
                        FennecNativeDriver.log(FennecNativeDriver.LogLevel.ERROR,
                            "getText: unable to find view " + mId + " " + mName);
                    } else {
                        FennecNativeDriver.log(FennecNativeDriver.LogLevel.ERROR,
                            "getText: unhandled type for view " + mId + " " + mName);
                    }
                } // end of run() method definition
            } // end of anonymous Runnable object instantiation
        );
        if (mText == null) {
            FennecNativeDriver.log(FennecNativeDriver.LogLevel.WARN,
                "getText: Text is null for view " + mId + " " + mName);
            return null;
        }
        return mText.toString();
    }

    private boolean mDisplayed;

    @Override
    public boolean isDisplayed() {
        mDisplayed = false;
        RobocopUtils.runOnUiThreadSync(mActivity,
            new Runnable() {
                @Override
                public void run() {
                    View view = mActivity.findViewById(mId);
                    if (view != null) {
                        mDisplayed = true;
                    }
                }
            });
        return mDisplayed;
    }
}
