/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.ffxcp;

import java.io.File;
import java.io.IOException;

import android.database.MatrixCursor;

public class DirCursor extends MatrixCursor {
    public static final String _ID = "_id";
    public static final String ISDIR = "isdir";
    public static final String FILENAME = "filename";
    public static final String LENGTH = "length";
    public static final String TIMESTAMP = "ts";
    public static final String WRITABLE = "writable";
    static final String[] DEFCOLUMNS = new String[] {
            _ID,
            ISDIR,
            FILENAME,
            LENGTH,
            TIMESTAMP,
            WRITABLE
         };
    private String dirPath = null;
    private String [] theColumns = null;

    public DirCursor(String[] columnNames, String sPath) {
        super((columnNames == null ? DEFCOLUMNS : columnNames));
        theColumns = (columnNames == null ? DEFCOLUMNS : columnNames);
        dirPath = sPath;
        doLoadCursor(dirPath);
    }

    public DirCursor(String[] columnNames, int initialCapacity, String sPath) {
        super((columnNames == null ? DEFCOLUMNS : columnNames), initialCapacity);
        theColumns = (columnNames == null ? DEFCOLUMNS : columnNames);
        dirPath = sPath;
        doLoadCursor(dirPath);
    }

    private void doLoadCursor(String sDir) {
        File dir = new File(sDir);
        int    nFiles = 0;
        int nCols = theColumns.length;
        int    lcvFiles = 0;
        int    nCIndex = 0;
        Object [] vals = new Object[nCols];

        if (dir.isDirectory()) {
            try {
                nCIndex = getColumnIndex(_ID);
                if (nCIndex > -1)
                    vals[nCIndex] = -1;

                nCIndex = getColumnIndex(ISDIR);
                if (nCIndex > -1)
                    vals[nCIndex] = 1;

                nCIndex = getColumnIndex(FILENAME);
                if (nCIndex > -1)
                    try {
                        vals[nCIndex] = dir.getCanonicalPath();
                    } catch (IOException e) {
                        vals[nCIndex] = dir.getName();
                    }

                nCIndex = getColumnIndex(LENGTH);
                if (nCIndex > -1)
                    vals[nCIndex] = 0;

                nCIndex = getColumnIndex(TIMESTAMP);
                if (nCIndex > -1)
                    vals[nCIndex] = 0;

                nCIndex = getColumnIndex(WRITABLE);
                if (nCIndex > -1)
                    vals[nCIndex] = (dir.canWrite() ? 1 : 0);

                addRow(vals);
                }
            catch (IllegalArgumentException iae) {
                iae.printStackTrace();
            }

            File [] files = dir.listFiles();
            if (files != null) {
                if ((nFiles = files.length) > 0) {
                    for (lcvFiles = 0; lcvFiles < nFiles; lcvFiles++) {
                        nCIndex = getColumnIndex(_ID);
                        if (nCIndex > -1)
                            vals[nCIndex] = lcvFiles;

                        nCIndex = getColumnIndex(ISDIR);
                        if (nCIndex > -1)
                            vals[nCIndex] = (files[lcvFiles].isDirectory() ? 1 : 0);

                        nCIndex = getColumnIndex(FILENAME);
                        if (nCIndex > -1)
                                vals[nCIndex] = files[lcvFiles].getName();

                        nCIndex = getColumnIndex(LENGTH);
                        if (nCIndex > -1)
                            vals[nCIndex] = (files[lcvFiles].isDirectory() ? -1 : files[lcvFiles].length());

                        try {
                            addRow(vals);
                        } catch (IllegalArgumentException iae) {
                            iae.printStackTrace();
                        }
                    }
                }
            }
        } else {
            if (dir.isFile()) {
                nCIndex = getColumnIndex(_ID);
                if (nCIndex > -1)
                    vals[nCIndex] = -1;

                nCIndex = getColumnIndex(ISDIR);
                if (nCIndex > -1)
                    vals[nCIndex] = 0;

                nCIndex = getColumnIndex(FILENAME);
                if (nCIndex > -1)
                    vals[nCIndex] = dir.getName();

                nCIndex = getColumnIndex(LENGTH);
                if (nCIndex > -1)
                    vals[nCIndex] = dir.length();

                nCIndex = getColumnIndex(TIMESTAMP);
                if (nCIndex > -1) {
                    vals[nCIndex] = dir.lastModified();
                }

                try {
                    addRow(vals);
                    }
                catch (IllegalArgumentException iae) {
                    iae.printStackTrace();
                }
            }
            else {
                try {
                    nCIndex = getColumnIndex(_ID);
                    if (nCIndex > -1)
                        vals[nCIndex] = -1;

                    nCIndex = getColumnIndex(ISDIR);
                    if (nCIndex > -1)
                        vals[nCIndex] = 0;

                    nCIndex = getColumnIndex(FILENAME);
                    if (nCIndex > -1)
                        vals[nCIndex] = null;

                    nCIndex = getColumnIndex(LENGTH);
                    if (nCIndex > -1)
                        vals[nCIndex] = 0;

                    nCIndex = getColumnIndex(TIMESTAMP);
                    if (nCIndex > -1)
                        vals[nCIndex] = 0;

                    addRow(vals);
                    }
                catch (IllegalArgumentException iae) {
                    iae.printStackTrace();
                }
            }
        }
    }
}
