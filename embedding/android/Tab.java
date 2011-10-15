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

import java.util.*;

import android.content.*;
import android.database.sqlite.SQLiteDatabase;
import android.os.AsyncTask;
import android.graphics.drawable.*;
import android.util.Log;

public class Tab {

    private static final String LOG_FILE_NAME = "Tab";
    private int id;
    private String url, title;
    private Drawable favicon, thumbnail;
    private Stack<HistoryEntry> history;
    private boolean loading;

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
    }

    public Tab(int id, String url) {
        this.id = id;
        this.url = new String(url);
        this.title = new String();
        this.favicon = null;
        this.thumbnail = null;
        this.history = new Stack<HistoryEntry>();
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

    public boolean isLoading() {
        return loading;
    }

    public Stack<HistoryEntry> getHistory() {
        return history;
    }

    public void updateURL(String url) {

        if(url != null && url.length() > 0) {
            this.url = new String(url);
            Log.i(LOG_FILE_NAME, "Updated url: " + url + " for tab with id: " + this.id);
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
        if (favicon != null) {
            this.favicon = favicon;
            Log.i(LOG_FILE_NAME, "Updated favicon for tab with id: " + this.id);
        }
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
            Log.d(LOG_FILE_NAME, "adding uri=" + entry.mUri + ", title=" + entry.mTitle + " to history");
            ContentValues values = new ContentValues();
            values.put("url", entry.mUri);
            values.put("title", entry.mTitle);

            DatabaseHelper dbHelper = GeckoApp.getDatabaseHelper();
            SQLiteDatabase mDb = dbHelper.getWritableDatabase();
            long id = mDb.insertWithOnConflict("moz_places", null, values, SQLiteDatabase.CONFLICT_REPLACE);
            values = new ContentValues();
            values.put("place_id", id);
            mDb.insertWithOnConflict("moz_historyvisits", null, values, SQLiteDatabase.CONFLICT_REPLACE);
            mDb.close();
            return null;
        }
    }
} 
