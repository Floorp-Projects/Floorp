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
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Wes Johnston <wjohnston@mozilla.com>
 *   Mark Finkle <mfinkle@mozilla.com>
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

public class AwesomeBar extends ListActivity {
    public static final String URL_KEY = "url";
    public static final String TITLE_KEY = "title";
    public static final String CURRENT_URL_KEY = "currenturl";

    public class AwesomeBarCursorAdapter extends SimpleCursorAdapter {
        private Cursor mAdapterCursor;
        private Context mContext;

        public AwesomeBarCursorAdapter(Context context, int layout, Cursor c, String[] from, int[] to) {
            // Using the older, deprecated constructor so we can work on API < 11
            super(context, layout, c, from, to);
            mAdapterCursor = c;
            mContext = context;
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            super.bindView(view, context, cursor);

            // ImageView imageView = (ImageView) view.findViewById(R.id.favicon);
            // byte[] raw = cursor.getBlob(cursor.getColumnIndexOrThrow("favicon"));
            // Bitmap bitmap = Bitmap.createScaledBitmap(BitmapFactory.decodeByteArray(raw, 0, raw.length), 48, 48, true);
            // imageView.setImageBitmap(bitmap);
        }
    }
    
    private Cursor mCursor;
    private SQLiteDatabase mDb;
    private AwesomeBarCursorAdapter adapter;

    private String getProfilePath() {
        File home = new File(getFilesDir(), "mozilla");
        if (!home.exists())
            return null;

        File profile = null;
        String[] files = home.list();
        for (int i = 0; i < files.length; i++) {
            if (files[i].endsWith(".default")) {
                profile = new File(home, files[i]);
                break;
            }
        }

        if (profile == null)
            return null;

        File webapps = new File(profile, "places.sqlite");
        if (!webapps.exists())
            return null;

        return webapps.getPath();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.d("AwesomeBar", "creating awesomebar");

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.awesomebar_search);

        // Load the list using a custom adapter so we can create the bitmaps
        adapter = new AwesomeBarCursorAdapter(
            this,
            R.layout.awesomebar_row,
            null,
            new String[] { TITLE_KEY, URL_KEY },
            new int[] { R.id.title, R.id.url }
        );
        setListAdapter(adapter);

        final EditText text = (EditText)findViewById(R.id.awesomebar_text);

        String currentUrl = getIntent().getStringExtra(CURRENT_URL_KEY);
        if (currentUrl != null) {
            text.setText(currentUrl);
            text.selectAll();
        }

        text.addTextChangedListener(new TextWatcher() {
                
            public void afterTextChanged(Editable s) {
                // do nothing
            }
            
            public void beforeTextChanged(CharSequence s, int start, int count,
                                          int after) {
                // do nothing
            }
            
            public void onTextChanged(CharSequence s, int start, int before,
                                      int count) {
                adapter.getFilter().filter(s.toString());
            }
                
        });

        text.setOnKeyListener(new View.OnKeyListener() {
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if (keyCode == KeyEvent.KEYCODE_ENTER) {
                    Intent resultIntent = new Intent();
                    resultIntent.putExtra(URL_KEY, text.getText().toString());
                    setResult(Activity.RESULT_OK, resultIntent);
                    finish();
                    return true;
                } else {
                    return false;
                }
            }
        });

        DatabaseHelper dbHelper = new DatabaseHelper(this);
        mDb = dbHelper.getReadableDatabase();

        adapter.setFilterQueryProvider(new FilterQueryProvider() {
            public Cursor runQuery(CharSequence constraint) {

                // _id column required for CursorAdapter; provide a dummy here
                mCursor = mDb.rawQuery(
                        "SELECT 0 AS _id, title, url "
                          + "FROM moz_places "
                          + "WHERE (url LIKE ? OR title LIKE ?) "
                          + "LIMIT 12",
                        new String[] {"%" + constraint.toString() + "%", "%" + constraint.toString() + "%",});

                return mCursor;
            }
        });

        // show unfiltered results initially
        adapter.getFilter().filter("");

    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mCursor != null) mCursor.close();
        if (mDb != null) mDb.close();
    }

    @Override
    public void onListItemClick(ListView l, View v, int position, long id) {
        Cursor cursor = (Cursor)l.getItemAtPosition(position);
        String url = cursor.getString(cursor.getColumnIndexOrThrow(URL_KEY));
        Intent resultIntent = new Intent();
        resultIntent.putExtra(URL_KEY, url);
        setResult(Activity.RESULT_OK, resultIntent);
        finish();
    }
}
