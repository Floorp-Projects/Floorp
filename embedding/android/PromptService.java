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

import java.io.*;
import java.util.List;
import java.util.concurrent.SynchronousQueue;
import android.util.Log;
import java.lang.String;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.view.View;
import android.view.LayoutInflater;
import android.view.ViewGroup.LayoutParams;
import android.widget.TextView;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.ArrayAdapter;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface.OnCancelListener;
import org.json.JSONArray;
import org.json.JSONObject;
import android.text.method.PasswordTransformationMethod;
import android.graphics.Color;
import android.text.InputType;
import android.app.AlertDialog;

public class PromptService implements OnClickListener, OnCancelListener {
    private PromptInput[] mInputs;
    private AlertDialog mDialog = null;
    private String[] mMenuItems = null;

    private class PromptButton {
        public String label = "";
        PromptButton(JSONObject aJSONButton) {
            try {
                label = aJSONButton.getString("label");
            } catch(Exception ex) { }
        }
    }

    private class PromptInput {
        private String label = "";
        private String type  = "";
        private String hint  = "";
        private JSONObject mJSONInput = null;
        private View view = null;

        public PromptInput(JSONObject aJSONInput) {
            mJSONInput = aJSONInput;
            try {
                label = aJSONInput.getString("label");
            } catch(Exception ex) { }
            try {
                type  = aJSONInput.getString("type");
            } catch(Exception ex) { }
            try {
                hint  = aJSONInput.getString("hint");
            } catch(Exception ex) { }
        }

        public View getView() {
            if (type.equals("checkbox")) {
                CheckBox checkbox = new CheckBox(GeckoApp.mAppContext);
                checkbox.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT));
                checkbox.setText(label);
                try {
                    Boolean value = mJSONInput.getBoolean("checked");
                    checkbox.setChecked(value);
                } catch(Exception ex) { }
                view = (View)checkbox;
            } else if (type.equals("textbox") || this.type.equals("password")) {
                EditText input = new EditText(GeckoApp.mAppContext);
                int inputtype = InputType.TYPE_CLASS_TEXT;
                if (type.equals("password")) {
                    inputtype |= InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
                }
                input.setInputType(inputtype);

                try {
                    String value = mJSONInput.getString("value");
                    input.setText(value);
                } catch(Exception ex) { }

                if (!hint.equals("")) {
                    input.setHint(hint);
                }
                view = (View)input;
            } else if (type.equals("menulist")) {
                Spinner spinner = new Spinner(GeckoApp.mAppContext);
                try {
                    String[] listitems = getStringArray(mJSONInput, "values");
                    if (listitems.length > 0) {
                        ArrayAdapter<String> adapter = new ArrayAdapter<String>(GeckoApp.mAppContext, android.R.layout.simple_dropdown_item_1line, listitems);
                        spinner.setAdapter(adapter);
                    }
                } catch(Exception ex) { }
                view = (View)spinner;
            }
            return view;
        }

        public String getName() {
            return type;
        }
    
        public String getValue() {
            if (this.type.equals("checkbox")) {
                CheckBox checkbox = (CheckBox)view;
                return checkbox.isChecked() ? "true" : "false";
            } else if (type.equals("textbox") || type.equals("password")) {
                EditText edit = (EditText)view;
                return edit.getText().toString();
            } else if (type.equals("menulist")) {
                Spinner spinner = (Spinner)view;
                return Integer.toString(spinner.getSelectedItemPosition());
            }
            return "";
        }
    }

    public void Show(String aTitle, String aText, PromptButton[] aButtons) {
        AlertDialog.Builder builder = new AlertDialog.Builder(GeckoApp.mAppContext);
        if (!aTitle.equals("")) {
            builder.setTitle(aTitle);
        }

        if (mMenuItems.length > 0) {
            builder.setItems(mMenuItems, this);
        } else {
            if (!aText.equals("")) {
                builder.setMessage(aText);
            }
        }

        int length = mInputs.length;
        if (length == 1) {
            builder.setView(mInputs[0].getView());
        } else if (length > 1) {
            LinearLayout linearLayout = new LinearLayout(GeckoApp.mAppContext);
            linearLayout.setOrientation(LinearLayout.VERTICAL);
            for (int i = 0; i < length; i++) {
                View content = mInputs[i].getView();
                linearLayout.addView(content);
            }
            builder.setView((View)linearLayout);
        }

        length = aButtons.length;
        if (length > 0) {
            builder.setPositiveButton(aButtons[0].label, this);
        }
        if (length > 1) {
            builder.setNeutralButton(aButtons[1].label, this);
        }
        if (length > 2) {
            builder.setNegativeButton(aButtons[2].label, this);
        }

        mDialog = builder.create();
        mDialog.setOnCancelListener(this);
        mDialog.show();
    }

    public void onDestroy() {
        if (mDialog != null)
            mDialog.dismiss();
    }

    public void onResume() {
        if (mDialog != null)
            mDialog.show();
    }

    public void onClick(DialogInterface aDialog, int aWhich) {
        JSONObject ret = new JSONObject();
        try {
            int button = -1;
            if (mMenuItems.length > 0) {
                button = aWhich;
            } else {
                switch(aWhich) {
                    case DialogInterface.BUTTON_POSITIVE : button = 0; break;
                    case DialogInterface.BUTTON_NEUTRAL  : button = 1; break;
                    case DialogInterface.BUTTON_NEGATIVE : button = 2; break;
                }
            }
            ret.put("button", button);
            if (mInputs != null) {
                for (int i = 0; i < mInputs.length; i++) {
                    ret.put(mInputs[i].getName(), mInputs[i].getValue());
                }
            }
        } catch(Exception ex) {
            Log.i("GeckoShell", "Error building return: " + ex);
        }
        finishDialog(ret.toString());
    }

    public void onCancel(DialogInterface aDialog) {
        JSONObject ret = new JSONObject();
        try {
            int button = -1;
            ret.put("button", button);
        } catch(Exception ex) {
            Log.i("GeckoShell", "Error building return: " + ex);
        }
        finishDialog(ret.toString());
    }

    public void finishDialog(String aReturn) {
        mInputs = null;
        mDialog = null;
        mMenuItems = null;
        Log.i("GeckoShell", "finish " + aReturn);
        try {
            GeckoAppShell.sPromptQueue.put(aReturn);
        } catch(Exception ex) { }
    }

    public void processMessage(JSONObject geckoObject) {
        String title = "";
        try {
            title = geckoObject.getString("title");
        } catch(Exception ex) { }
        String text = "";
        try {
            text = geckoObject.getString("text");
        } catch(Exception ex) { }

        JSONArray buttons = new JSONArray();
        try {
            buttons = geckoObject.getJSONArray("buttons");
        } catch(Exception ex) { }
        int length = buttons.length();
        PromptButton[] promptbuttons = new PromptButton[length];
        for (int i = 0; i < length; i++) {
            try {
                promptbuttons[i] = new PromptButton(buttons.getJSONObject(i));
            } catch(Exception ex) { }
        }

        JSONArray inputs = new JSONArray();
        try {
            inputs = geckoObject.getJSONArray("inputs");
        } catch(Exception ex) { }
        length = inputs.length();
        mInputs = new PromptInput[length];
        for (int i = 0; i < length; i++) {
            try {
                mInputs[i] = new PromptInput(inputs.getJSONObject(i));
            } catch(Exception ex) { }
        }

        mMenuItems = getStringArray(geckoObject, "listitems");
        this.Show(title, text, promptbuttons);
    }

    private String[] getStringArray(JSONObject aObject, String aName) {
        JSONArray items = new JSONArray();
        try {
            items = aObject.getJSONArray(aName);
        } catch(Exception ex) {
        }
        int length = items.length();
        String[] list = new String[length];
        for (int i = 0; i < length; i++) {
            try {
                list[i] = items.getString(i);
            } catch(Exception ex) { }
        }
        return list;
    }
}
