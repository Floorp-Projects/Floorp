/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2009-2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <blassey@mozilla.com>
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

import android.app.ListActivity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.util.Log;
import android.view.View;
import android.widget.ListAdapter;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;

public class GeckoBookmarks extends ListActivity {
    private static final String LOG_NAME = "GeckoBookmarks";
    private static final String TITLE_KEY = "title";
    private static final String kBookmarksWhereClause = Browser.BookmarkColumns.BOOKMARK + " = 1";

    private Cursor mCursor;
    private Uri mUri;
    private String mTitle;

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.bookmarks);
        mCursor = managedQuery(Browser.BOOKMARKS_URI,
                               null, kBookmarksWhereClause, null, null);
        startManagingCursor(mCursor);

        ListAdapter adapter =
          new SimpleCursorAdapter(this, R.layout.bookmark_list_row, mCursor,
        		      new String[] {Browser.BookmarkColumns.TITLE,
        				    Browser.BookmarkColumns.URL},
        		      new int[] {R.id.bookmark_title, R.id.bookmark_url});
        setListAdapter(adapter);
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        mCursor.moveToPosition(position);
        String spec = mCursor.getString(mCursor.getColumnIndex(Browser.BookmarkColumns.URL));
        Log.i(LOG_NAME, "clicked: " + spec);
        Intent intent = new Intent(this, GeckoApp.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(spec));
        startActivity(intent);
    }

    public void addBookmark(View v) {
        Browser.saveBookmark(this, mTitle, mUri.toString());
        finish();
    }

    @Override
    protected void onNewIntent(Intent intent) {
      // just save the uri from the intent
      mUri = intent.getData();
      mTitle = intent.getStringExtra(TITLE_KEY);
    }
}
