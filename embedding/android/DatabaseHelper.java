/*
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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.util.Log;

class DatabaseHelper extends SQLiteOpenHelper {
    private static final String LOG_NAME = "DatabaseHelper";

    private static final String CREATE_MOZ_PLACES =
      "CREATE TABLE moz_places ( " +
        "  id INTEGER PRIMARY KEY" +
        ", url LONGVARCHAR UNIQUE" +
        ", title LONGVARCHAR" +
        ", rev_host LONGVARCHAR" +
        ", visit_count INTEGER DEFAULT 0" +
        ", hidden INTEGER DEFAULT 0 NOT NULL" +
        ", typed INTEGER DEFAULT 0 NOT NULL" +
        ", favicon_id INTEGER" +
        ", frecency INTEGER DEFAULT -1 NOT NULL" +
        ", last_visit_date INTEGER " +
        ", guid TEXT" +
      ")";

    private static final String CREATE_MOZ_HISTORYVISITS  =
      "CREATE TABLE moz_historyvisits (" +
        "  id INTEGER PRIMARY KEY" +
        ", from_visit INTEGER" +
        ", place_id INTEGER" +
        ", visit_date INTEGER" +
        ", visit_type INTEGER" +
        ", session INTEGER" +
      ")";

    private static final String CREATE_MOZ_FAVICONS =
      "CREATE TABLE moz_favicons (" +
        "  id INTEGER PRIMARY KEY" +
        ", url LONGVARCHAR UNIQUE" +
        ", data BLOB" +
        ", mime_type VARCHAR(32)" +
        ", expiration LONG" +
      ")";

    private static final String CREATE_MOZ_BOOKMARKS =
      "CREATE TABLE moz_bookmarks (" +
        "  id INTEGER PRIMARY KEY" +
        ", type INTEGER" +
        ", fk INTEGER DEFAULT NULL" +
        ", parent INTEGER" +
        ", position INTEGER" +
        ", title LONGVARCHAR" +
        ", keyword_id INTEGER" +
        ", folder_type TEXT" +
        ", dateAdded INTEGER" +
        ", lastModified INTEGER" +
        ", guid TEXT" +
      ")";

    private static final String CREATE_MOZ_BOOKMARKS_ROOTS =
      "CREATE TABLE moz_bookmarks_roots (" +
        "  root_name VARCHAR(16) UNIQUE" +
        ", folder_id INTEGER" +
      ")";

    private static final String DATABASE_NAME = "places";
    private static final int DATABASE_VERSION = 1;

    DatabaseHelper(Context context) {
        super(context, DATABASE_NAME, null, DATABASE_VERSION);
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
        db.execSQL(CREATE_MOZ_PLACES);
        db.execSQL(CREATE_MOZ_HISTORYVISITS);
        db.execSQL(CREATE_MOZ_BOOKMARKS);
        db.execSQL(CREATE_MOZ_BOOKMARKS_ROOTS);
        db.execSQL(CREATE_MOZ_FAVICONS);
    }

    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        Log.w(LOG_NAME, "Upgrading database from version " + oldVersion + " to "
                + newVersion + ", which will destroy all old data");
        db.execSQL("DROP TABLE IF EXISTS places");
        onCreate(db);
    }
}
