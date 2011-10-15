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

import java.io.*;
import java.util.*;

import org.mozilla.gecko.*;

import android.os.*;
import android.content.*;
import android.app.*;
import android.text.*;
import android.util.*;
import android.widget.*;
import android.database.sqlite.*;
import android.database.*;
import android.view.*;
import android.view.View.*;
import android.net.Uri;
import android.graphics.*;

public class ShowTabs extends ListActivity {
    private static final String LOG_FILE_NAME = "ShowTabs";
    public static final String ID = "id";
    public static final String TYPE = "type";
    private ArrayList <HashMap<String, String>> tabsList = null;
    public static enum Type { ADD, SWITCH };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.show_tabs);
        
        ListView list = (ListView) findViewById(android.R.id.list);
        Button addTab = new Button(this);
        addTab.setText("+ add tab");
        addTab.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
		Intent resultIntent = new Intent();
		resultIntent.putExtra(TYPE, Type.ADD.name());
		setResult(Activity.RESULT_OK, resultIntent);
                finish();
            }
        });

        list.addHeaderView(addTab);

        HashMap<Integer, Tab> tabs = Tabs.getInstance().getTabs();
        tabsList = new ArrayList<HashMap<String, String>> ();

        if (tabs != null) {
            Iterator keys = tabs.keySet().iterator();
            HashMap<String, String> map;
            Tab tab;
            while (keys.hasNext()) {
                tab = tabs.get(keys.next());
                map = new HashMap<String, String>();
                map.put("id", "" + tab.getId());
                map.put("title", tab.getTitle());
                map.put("url", tab.getURL());
                tabsList.add(map);
            }
        }
        
        list.setAdapter(new SimpleAdapter(
            ShowTabs.this,
            tabsList,
            R.layout.awesomebar_row,
            new String[] { "title", "url" },
            new int[] { R.id.title, R.id.url }
        ));
    }

    @Override
    public void onListItemClick(ListView l, View v, int position, long id) {
        HashMap<String, String> map = tabsList.get((int) id); 
        Intent resultIntent = new Intent();
        resultIntent.putExtra(TYPE, Type.SWITCH.name());
        resultIntent.putExtra(ID, map.get("id"));
        setResult(Activity.RESULT_OK, resultIntent);
        finish();
    }
}
