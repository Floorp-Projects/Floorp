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

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.provider.Browser;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.FilterQueryProvider;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;
import android.widget.TabHost;
import android.widget.TextView;

import java.util.Date;

public class AwesomeBarTabs extends TabHost {
    private static final String ALL_PAGES_TAB = "all";
    private static final String BOOKMARKS_TAB = "bookmarks";
    private static final String HISTORY_TAB = "history";

    private static final String LOG_NAME = "AwesomeBarTabs";

    private Context mContext;
    private OnUrlOpenListener mUrlOpenListener;

    private Cursor mAllPagesCursor;
    private SimpleCursorAdapter mAllPagesAdapter;

    private SimpleCursorAdapter mBookmarksAdapter;

    public interface OnUrlOpenListener {
        public abstract void onUrlOpen(AwesomeBarTabs tabs, String url);
    }

    private class BookmarksQueryTask extends AsyncTask<Void, Void, Cursor> {
        protected Cursor doInBackground(Void... arg0) {
            ContentResolver resolver = mContext.getContentResolver();

            return resolver.query(Browser.BOOKMARKS_URI,
                                  null,
                                  Browser.BookmarkColumns.BOOKMARK + " = 1",
                                  null,
                                  Browser.BookmarkColumns.TITLE);
        }

        protected void onPostExecute(Cursor cursor) {
            // Load the list using a custom adapter so we can create the bitmaps
            mBookmarksAdapter = new SimpleCursorAdapter(
                mContext,
                R.layout.awesomebar_row,
                cursor,
                new String[] { AwesomeBar.TITLE_KEY, AwesomeBar.URL_KEY },
                new int[] { R.id.title, R.id.url }
            );

            final ListView bookmarksList = (ListView) findViewById(R.id.bookmarks_list);

            bookmarksList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
                public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                    handleItemClick(bookmarksList, position);
                }
            });

            bookmarksList.setAdapter(mBookmarksAdapter);
        }
    }

    public AwesomeBarTabs(Context context, AttributeSet attrs) {
        super(context, attrs);

        Log.d(LOG_NAME, "Creating AwesomeBarTabs");

        mContext = context;

        // Load layout into the custom view
        LayoutInflater inflater =
                (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        inflater.inflate(R.layout.awesomebar_tabs, this);

        // This should be called before adding any tabs
        // to the TabHost.
        setup();

        addAllPagesTab();
        addBookmarksTab();
        addHistoryTab();

        // Initialize "App Pages" list with no filter
        filter("");
    }

    private TabSpec addAwesomeTab(String id, int titleId, int contentId) {
        TabSpec tab = newTabSpec(id);

        Resources resources = mContext.getResources();
        tab.setIndicator(resources.getString(titleId));

        tab.setContent(contentId);

        addTab(tab);

        return tab;
    }

    private void addAllPagesTab() {
        Log.d(LOG_NAME, "Creating All Pages tab");

        addAwesomeTab(ALL_PAGES_TAB,
                      R.string.awesomebar_all_pages_title,
                      R.id.all_pages_list);

        // Load the list using a custom adapter so we can create the bitmaps
        mAllPagesAdapter = new SimpleCursorAdapter(
            mContext,
            R.layout.awesomebar_row,
            null,
            new String[] { AwesomeBar.TITLE_KEY, AwesomeBar.URL_KEY },
            new int[] { R.id.title, R.id.url }
        );

        mAllPagesAdapter.setFilterQueryProvider(new FilterQueryProvider() {
            public Cursor runQuery(CharSequence constraint) {
                ContentResolver resolver = mContext.getContentResolver();

                mAllPagesCursor =
                    resolver.query(Browser.BOOKMARKS_URI,
                                   null, Browser.BookmarkColumns.URL + " LIKE ? OR title LIKE ?", 
                                   new String[] {"%" + constraint.toString() + "%", "%" + constraint.toString() + "%",},
                                   // ORDER BY is number of visits times a multiplier from 1 - 120 of how recently the site 
                                   // was accessed with a site accessed today getting 120 and a site accessed 119 or more 
                                   // days ago getting 1
                                   Browser.BookmarkColumns.VISITS + " * MAX(1, (" +
                                   Browser.BookmarkColumns.DATE + " - " + new Date().getTime() + ") / 86400000 + 120) DESC");   

                return mAllPagesCursor;
            }
        });

        final ListView allPagesList = (ListView) findViewById(R.id.all_pages_list);

        allPagesList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                handleItemClick(allPagesList, position);
            }
        });

        allPagesList.setAdapter(mAllPagesAdapter);
    }

    private void addBookmarksTab() {
        Log.d(LOG_NAME, "Creating Bookmarks tab");

        addAwesomeTab(BOOKMARKS_TAB,
                      R.string.awesomebar_bookmarks_title,
                      R.id.bookmarks_list);

        new BookmarksQueryTask().execute();
    }

    private void addHistoryTab() {
        Log.d(LOG_NAME, "Creating History tab");

        addAwesomeTab(HISTORY_TAB,
                      R.string.awesomebar_history_title,
                      R.id.history_list);
    }

    private void handleItemClick(ListView list, int position) {
        Cursor cursor = (Cursor) list.getItemAtPosition(position);
        String url = cursor.getString(cursor.getColumnIndexOrThrow(AwesomeBar.URL_KEY));

        if (mUrlOpenListener != null)
            mUrlOpenListener.onUrlOpen(this, url);
    }

    public void setOnUrlOpenListener(OnUrlOpenListener listener) {
        mUrlOpenListener = listener;
    }

    public void destroy() {
        if (mAllPagesCursor != null) mAllPagesCursor.close();

        Cursor bookmarksCursor = mBookmarksAdapter.getCursor();
        if (bookmarksCursor != null)
            bookmarksCursor.close();
    }

    public void filter(String searchTerm) {
        // Ensure the 'All Pages' tab is selected
        setCurrentTabByTag(ALL_PAGES_TAB);

        // The tabs should only be visible if there's no on-going search
        int tabsVisibility = (searchTerm.isEmpty() ? View.VISIBLE : View.GONE);
        getTabWidget().setVisibility(tabsVisibility);

        // Perform the actual search
        mAllPagesAdapter.getFilter().filter(searchTerm);
    }
}
