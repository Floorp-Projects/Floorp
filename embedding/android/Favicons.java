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
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.provider.Browser;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashMap;

public class Favicons {
    private static final String LOG_NAME = "Favicons";

    private Context mContext;
    private DatabaseHelper mDbHelper;

    public interface OnFaviconLoadedListener {
        public void onFaviconLoaded(String url, Drawable favicon);
    }

    private class DatabaseHelper extends SQLiteOpenHelper {
        private static final String DATABASE_NAME = "favicon_urls.db";
        private static final String TABLE_NAME = "favicon_urls";
        private static final int DATABASE_VERSION = 1;

        private static final String COLUMN_ID = "_id";
        private static final String COLUMN_FAVICON_URL = "favicon_url";
        private static final String COLUMN_PAGE_URL = "page_url";

        DatabaseHelper(Context context) {
            super(context, DATABASE_NAME, null, DATABASE_VERSION);
            Log.d(LOG_NAME, "Creating DatabaseHelper");
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            Log.d(LOG_NAME, "Creating database for favicon URLs");

            db.execSQL("CREATE TABLE " + TABLE_NAME + " (" +
                       COLUMN_ID + " INTEGER PRIMARY KEY," +
                       COLUMN_FAVICON_URL + " TEXT NOT NULL," +
                       COLUMN_PAGE_URL + " TEXT UNIQUE NOT NULL" +
                       ");");
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            Log.w(LOG_NAME, "Upgrading favicon URLs database from version " +
                  oldVersion + " to " + newVersion + ", which will destroy all old data");

            // Drop table completely
            db.execSQL("DROP TABLE IF EXISTS " + TABLE_NAME);

            // Recreate database
            onCreate(db);
        }

        public String getFaviconUrlForPageUrl(String pageUrl) {
            Log.d(LOG_NAME, "Calling getFaviconUrlForPageUrl() for " + pageUrl);

            SQLiteDatabase db = mDbHelper.getReadableDatabase();

            SQLiteQueryBuilder qb = new SQLiteQueryBuilder();
            qb.setTables(TABLE_NAME);

            Cursor c = qb.query(
                db,
                new String[] { COLUMN_FAVICON_URL },
                COLUMN_PAGE_URL + " = ?",
                new String[] { pageUrl },
                null, null, null
            );

            if (!c.moveToFirst())
                return null;

            String url = c.getString(c.getColumnIndexOrThrow(COLUMN_FAVICON_URL));
            c.close();
            return url;
        }

        public void setFaviconUrlForPageUrl(String pageUrl, String faviconUrl) {
            Log.d(LOG_NAME, "Calling setFaviconUrlForPageUrl() for " + pageUrl);

            SQLiteDatabase db = mDbHelper.getWritableDatabase();

            ContentValues values = new ContentValues();
            values.put(COLUMN_FAVICON_URL, faviconUrl);
            values.put(COLUMN_PAGE_URL, pageUrl);

            db.replace(TABLE_NAME, null, values);
        }
    }

    public Favicons(Context context) {
        Log.d(LOG_NAME, "Creating Favicons instance");

        mContext = context;
        mDbHelper = new DatabaseHelper(context);
    }

    public void loadFavicon(String pageUrl, String faviconUrl,
            OnFaviconLoadedListener listener) {

        Log.d(LOG_NAME, "Calling loadFavicon() with URL = " + pageUrl +
                        " and favicon URL = " + faviconUrl);

        // Handle the case where page url is empty
        if (pageUrl == null || pageUrl.length() == 0) {
            if (listener != null)
                listener.onFaviconLoaded(null, null);
        }

        new LoadFaviconTask(pageUrl, faviconUrl, listener).execute();
    }

    public void close() {
        Log.d(LOG_NAME, "Closing Favicons database");
        mDbHelper.close();
    }

    private class LoadFaviconTask extends AsyncTask<Void, Void, BitmapDrawable> {
        private String mPageUrl;
        private String mFaviconUrl;
        private OnFaviconLoadedListener mListener;

        public LoadFaviconTask(String pageUrl, String faviconUrl, OnFaviconLoadedListener listener) {
            mPageUrl = pageUrl;
            mFaviconUrl = faviconUrl;
            mListener = listener;

            Log.d(LOG_NAME, "Creating LoadFaviconTask with URL = " + pageUrl +
                            " and favicon URL = " + faviconUrl);
        }

        // Runs in background thread
        private BitmapDrawable loadFaviconFromDb() {
            Log.d(LOG_NAME, "Loading favicon from DB for URL = " + mPageUrl);

            ContentResolver resolver = mContext.getContentResolver();

            Cursor c = resolver.query(Browser.BOOKMARKS_URI,
                                      new String[] { Browser.BookmarkColumns.FAVICON },
                                      Browser.BookmarkColumns.URL + " = ?",
                                      new String[] { mPageUrl },
                                      null);

            if (!c.moveToFirst()) {
                c.close();
                return null;
            }

            int faviconIndex = c.getColumnIndexOrThrow(Browser.BookmarkColumns.FAVICON);
            
            byte[] b = c.getBlob(faviconIndex);
            c.close();
            if (b == null)
                return null;

            Bitmap bitmap = BitmapFactory.decodeByteArray(b, 0, b.length);

            Log.d(LOG_NAME, "Loaded favicon from DB successfully for URL = " + mPageUrl);

            return new BitmapDrawable(bitmap);
        }

        // Runs in background thread
        private void saveFaviconToDb(BitmapDrawable favicon) {
            Bitmap bitmap = favicon.getBitmap();

            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);

            ContentValues values = new ContentValues();
            values.put(Browser.BookmarkColumns.FAVICON, stream.toByteArray());
            values.put(Browser.BookmarkColumns.URL, mPageUrl);

            ContentResolver resolver = mContext.getContentResolver();

            Log.d(LOG_NAME, "Saving favicon on browser database for URL = " + mPageUrl);
            resolver.update(Browser.BOOKMARKS_URI,
                            values,
                            Browser.BookmarkColumns.URL + " = ?",
                            new String[] { mPageUrl });


            Log.d(LOG_NAME, "Saving favicon URL for URL = " + mPageUrl);
            mDbHelper.setFaviconUrlForPageUrl(mPageUrl, mFaviconUrl);
        }

        // Runs in background thread
        private BitmapDrawable downloadFavicon(URL faviconUrl) {
            Log.d(LOG_NAME, "Downloading favicon for URL = " + mPageUrl +
                            " with favicon URL = " + mFaviconUrl);

            BitmapDrawable image = null;

            try {
                InputStream is = (InputStream) faviconUrl.getContent();
                image = (BitmapDrawable) Drawable.createFromStream(is, "src");
            } catch (IOException e) {
                Log.d(LOG_NAME, "Error downloading favicon: " + e);
            }

            if (image != null) {
                Log.d(LOG_NAME, "Downloaded favicon successfully for URL = " + mPageUrl);
                saveFaviconToDb(image);
            }

            return image;
        }

        @Override
        protected BitmapDrawable doInBackground(Void... unused) {
            BitmapDrawable image = null;
            URL pageUrl = null;

            // Handle the case of malformed URL
            try {
                pageUrl = new URL(mPageUrl);
            } catch (MalformedURLException e) {
                Log.d(LOG_NAME, "The provided URL is not valid: " + e);
                return null;
            }

            URL faviconUrl = null;

            // Handle the case of malformed favicon URL
            try {
                // If favicon is empty, fallback to default favicon URI
                if (mFaviconUrl == null || mFaviconUrl.length() == 0) {
                    faviconUrl = new URL(pageUrl.getProtocol(), pageUrl.getAuthority(), "/favicon.ico");
                    mFaviconUrl = faviconUrl.toString();
                } else {
                    faviconUrl = new URL(mFaviconUrl);
                }
            } catch (MalformedURLException e) {
                Log.d(LOG_NAME, "The provided favicon URL is not valid: " + e);
                return null;
            }

            Log.d(LOG_NAME, "Favicon URL is now: " + mFaviconUrl);

            String storedFaviconUrl = mDbHelper.getFaviconUrlForPageUrl(mPageUrl);
            if (storedFaviconUrl != null && storedFaviconUrl.equals(mFaviconUrl)) {
                image = loadFaviconFromDb();

                // If favicon URL is defined but the favicon image is not
                // stored in the database for some reason, we force download.
                if (image == null) {
                    image = downloadFavicon(faviconUrl);
                }
            } else {
                image = downloadFavicon(faviconUrl);
            }

            return image;
        }

        @Override
        protected void onPostExecute(BitmapDrawable image) {
            Log.d(LOG_NAME, "LoadFaviconTask finished for URL = " + mPageUrl);

            if (mListener != null)
                mListener.onFaviconLoaded(mPageUrl, image);
        }
    }
}
