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

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.provider.Browser;
import android.util.Log;

import java.util.Stack;

public class Tab {

    private static final String LOG_FILE_NAME = "Tab";
    private int id;
    private String url, title;
    private Drawable favicon, thumbnail;
    private Stack<HistoryEntry> history;
    private boolean loading;
    private boolean bookmark;

    static class HistoryEntry {
        public final String mUri;
        public final String mTitle;

        public HistoryEntry(String uri, String title) {
            mUri = uri;
            mTitle = title;
        }
    }

    public Tab() {
        this.id = -1;
        this.url = new String();
        this.title = new String();
        this.favicon = null;
        this.thumbnail = null;
        this.history = new Stack<HistoryEntry>();
        this.bookmark = false;
    }

    public Tab(int id, String url) {
        this.id = id;
        this.url = new String(url);
        this.title = new String();
        this.favicon = null;
        this.thumbnail = null;
        this.history = new Stack<HistoryEntry>();
        this.bookmark = false;
    }

    public int getId() {
        return id;
    }

    public String getURL() {
        return url;
    }

    public String getTitle() {
        return title;
    }

    public Drawable getFavicon() {
        return favicon;
    }

    public boolean isLoading() {
        return loading;
    }

    public boolean isBookmark() {
        return bookmark;
    }

    public Stack<HistoryEntry> getHistory() {
        return history;
    }

    public void updateURL(String url) {

        if(url != null && url.length() > 0) {
            this.url = new String(url);
            Log.i(LOG_FILE_NAME, "Updated url: " + url + " for tab with id: " + this.id);
            updateBookmark();
        }
    }

    public void updateTitle(String title) {
        if(title != null && title.length() > 0) {
            this.title = new String(title);
            Log.i(LOG_FILE_NAME, "Updated title: " + title + " for tab with id: " + this.id);
        }
    }

    public void setLoading(boolean loading) {
        this.loading = loading;
    }

    private void setBookmark(boolean bookmark) {
        this.bookmark = bookmark;
    }

    public void addHistory(HistoryEntry entry) {
       if (history.empty() || !history.peek().mUri.equals(entry.mUri)) {
           history.push(entry);
           new HistoryEntryTask().execute(entry);
       }
    }

    public HistoryEntry getLastHistoryEntry() {
       if (history.empty())
           return null;
       return history.peek();
    }

    public void updateFavicon(Drawable favicon) {
        this.favicon = favicon;
        Log.i(LOG_FILE_NAME, "Updated favicon for tab with id: " + this.id);
    }
 
    private void updateBookmark() {
        new CheckBookmarkTask().execute();
    }

    public void addBookmark() {
        new AddBookmarkTask().execute();
    }

    public void removeBookmark() {
        new RemoveBookmarkTask().execute();
    }

    public boolean doReload() {
        if (history.empty())
            return false;
        GeckoEvent e = new GeckoEvent("session-reload", "");
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    public boolean doBack() {
        if (history.size() <= 1) {
            return false;
        }
        history.pop();
        GeckoEvent e = new GeckoEvent("session-back", "");
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    private class HistoryEntryTask extends AsyncTask<HistoryEntry, Void, Void> {
        protected Void doInBackground(HistoryEntry... entries) {
            HistoryEntry entry = entries[0];
            GlobalHistory.getInstance().add(entry.mUri);
            return null;
        }
    }

    private class CheckBookmarkTask extends AsyncTask<Void, Void, Boolean> {
        @Override
        protected Boolean doInBackground(Void... unused) {
            ContentResolver resolver = Tabs.getInstance().getContentResolver();
            Cursor cursor = resolver.query(Browser.BOOKMARKS_URI,
                                           null,
                                           Browser.BookmarkColumns.URL + " = ? and " + Browser.BookmarkColumns.BOOKMARK + " = ?",
                                           new String[] { getURL(), "1" },
                                           Browser.BookmarkColumns.URL);
            if (cursor.getCount() == 1)
                return true;
            else
                return false;
        }

        @Override
        protected void onPostExecute(Boolean isBookmark) {
            setBookmark(isBookmark.booleanValue());
        }
    }

    private class AddBookmarkTask extends AsyncTask<Void, Void, Void> {
        @Override
        protected Void doInBackground(Void... unused) {
            ContentResolver resolver = Tabs.getInstance().getContentResolver();
            Cursor cursor = resolver.query(Browser.BOOKMARKS_URI,
                                           null,
                                           Browser.BookmarkColumns.URL + " = ?",
                                           new String[] { getURL() },
                                           Browser.BookmarkColumns.URL);

            ContentValues values = new ContentValues();
            values.put(Browser.BookmarkColumns.BOOKMARK, "1");
            values.put(Browser.BookmarkColumns.TITLE, getTitle());

            if (cursor.getCount() == 1) {
                //entry exists, update the bookmark flag
                resolver.update(Browser.BOOKMARKS_URI,
                                values,
                                Browser.BookmarkColumns.URL + " = ?",
                                new String[] { getURL() });
            } else {
                //add a new entry
                values.put(Browser.BookmarkColumns.URL, url);
                resolver.insert(Browser.BOOKMARKS_URI,
                                values);
           }

           return null;
        }

        @Override
        protected void onPostExecute(Void unused) {
            setBookmark(true);
        }
    }

    private class RemoveBookmarkTask extends AsyncTask<Void, Void, Void> {
        @Override
        protected Void doInBackground(Void... unused) {
            ContentResolver resolver = Tabs.getInstance().getContentResolver();
            ContentValues values = new ContentValues();
            values.put(Browser.BookmarkColumns.BOOKMARK, "0");
            resolver.update(Browser.BOOKMARKS_URI,
                            values,
                            Browser.BookmarkColumns.URL + " = ?",
                            new String[] { getURL() });
            return null;
        }

        @Override
        protected void onPostExecute(Void unused) {
            setBookmark(false);
        }
    }
} 
