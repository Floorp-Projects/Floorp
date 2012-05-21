/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.ffxcp;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.UriMatcher;
import android.database.Cursor;
import android.net.Uri;

public class FfxCPFP extends ContentProvider {
    public static final String PROVIDER_NAME = "org.mozilla.ffxcp";
    public static final Uri CONTENT_URI = Uri.parse("content://" + PROVIDER_NAME + "/file");

    public static final String _ID = "_id";
    public static final String ISDIR = "isdir";
    public static final String FILENAME = "filename";
    public static final String LENGTH = "length";
    public static final String CHUNK = "chunk";
    static String[] dircolumns = new String[] {
        _ID,
        ISDIR,
        FILENAME,
        LENGTH
     };

    static String[] filecolumns = new String[] {
        _ID,
        CHUNK
     };

    private static final int DIR = 1;
    private static final int FILE_NAME = 2;

    private static final UriMatcher uriMatcher;
    static {
        uriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
           uriMatcher.addURI(PROVIDER_NAME, "dir", DIR);
           uriMatcher.addURI(PROVIDER_NAME, "file", FILE_NAME);
        }

    public int PruneDir(String sTmpDir) {
        int    nRet = 0;
        int nFiles = 0;
        String sSubDir = null;

        File dir = new File(sTmpDir);

        if (dir.isDirectory()) {
            File [] files = dir.listFiles();
            if (files != null) {
                if ((nFiles = files.length) > 0) {
                    for (int lcv = 0; lcv < nFiles; lcv++) {
                        if (files[lcv].isDirectory()) {
                            sSubDir = files[lcv].getAbsolutePath();
                            nRet += PruneDir(sSubDir);
                        }
                        else {
                            if (files[lcv].delete()) {
                                nRet++;
                            }
                        }
                    }
                }
            }
            if (dir.delete()) {
                nRet++;
            }
            if ((nFiles + 1) > nRet) {
                nRet = -1;
            }
        }

    return(nRet);
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        int nFiles = 0;
        switch (uriMatcher.match(uri)) {
            case FILE_NAME:
                File f = new File(selection);
                if (f.delete())
                    nFiles = 1;
                break;

            case DIR:
                nFiles = PruneDir(selection);
                break;

            default:
                break;
        }
        return nFiles;
    }

    @Override
    public String getType(Uri uri)
        {
        switch (uriMatcher.match(uri))
            {
            //---get directory---
            case DIR:
                return "vnd.android.cursor.dir/vnd.mozilla.dir ";
            //---get a particular file---
            case FILE_NAME:
                return "vnd.android.cursor.item/vnd.mozilla.file ";
            //---Unknown---
            default:
                throw new IllegalArgumentException("Unsupported URI: " + uri);
            }
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        Cursor retCursor = null;

        switch(uriMatcher.match(uri)) {
            case DIR:
                retCursor = new DirCursor(projection, selection);
                break;

            case FILE_NAME:
                retCursor = new FileCursor(projection, selection, selectionArgs);
                break;

            default:
                break;
        }
        return (retCursor);
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        int    nRet = 0;
        FileOutputStream dstFile = null;

        switch(uriMatcher.match(uri)) {
            case DIR:
                File dir = new File(selection);
                if (dir.mkdirs())
                    nRet = 1;
                break;

            case FILE_NAME:
                try {
                    long lOffset = values.getAsLong("offset");
                    byte [] buf = values.getAsByteArray(CHUNK);
                    int    nLength = values.getAsInteger(LENGTH);
                    if ((buf != null) && (nLength > 0)) {
                        File f = new File(selection);
                        dstFile = new FileOutputStream(f, (lOffset == 0 ? false : true));
                        dstFile.write(buf,0, nLength);
                        dstFile.flush();
                        dstFile.close();
                        nRet = nLength;
                    }
                } catch (FileNotFoundException fnfe) {
                    fnfe.printStackTrace();
                } catch (IOException ioe) {
                    try {
                        dstFile.flush();
                    } catch (IOException e) {
                    }
                    try {
                        dstFile.close();
                    } catch (IOException e) {
                    }
                }
                break;

            default:
                break;
        }
        return nRet;
    }
}
