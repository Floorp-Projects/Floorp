/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Android SUTAgent code.
 *
 * The Initial Developer of the Original Code is
 * Bob Moss.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Bob Moss <bmoss@mozilla.com>
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
package org.mozilla.fencp;

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
        
        if (vals == null)
            return;
        
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
