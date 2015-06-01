/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fencp;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import android.database.AbstractWindowedCursor;
import android.database.CursorWindow;

public class FileCursor extends AbstractWindowedCursor {
    public static final String _ID = "_id";
    public static final String CHUNK = "chunk";
    public static final String LENGTH = "length";
    static final String[] DEFCOLUMNS = new String[] {
            _ID,
            CHUNK,
            LENGTH
         };
    private String filePath = null;
    private String [] theColumns = null;

    private static final int BUFSIZE = 4096;
    private long lFileSize = 0;
    private int nCount = 0;
    private File theFile = null;
    private byte [] theBuffer = null;
    private long lOffset = 0;
    private long lLength = -1;

    public FileCursor(String[] columnNames, String sFilePath, String [] selectionArgs) {
        super();
        theColumns = (columnNames == null ? DEFCOLUMNS : columnNames);
        filePath = sFilePath;
        nCount = -1;

        if ((selectionArgs != null) && (selectionArgs.length > 0)) {
            lOffset = Long.parseLong(selectionArgs[0]);
            lLength = Long.parseLong(selectionArgs[1]);
        }

        if (filePath.length() > 0) {
            theFile = new File(filePath);
            if (theFile.exists() && theFile.canRead()) {
                lFileSize = theFile.length();

                // lLength == -1 return everything between lOffset and eof
                // lLength == 0 return file length
                // lLength > 0 return lLength bytes
                if (lLength == -1) {
                    lFileSize = lFileSize - lOffset;
                } else if (lLength == 0) {
                    // just return the file length
                } else {
                    lFileSize = ((lLength <= (lFileSize - lOffset)) ? lLength : (lFileSize - lOffset));
                }

                if (lLength != 0) {
                    nCount = (int) (lFileSize / BUFSIZE);
                    if ((lFileSize % BUFSIZE) > 0)
                        nCount++;
                } else {
                    nCount = 1;
                }

            }
        }
    }

    public String getColumnName (int columnIndex) {
        return theColumns[columnIndex];
    }

    @Override
    public String[] getColumnNames() {
        return theColumns;
    }

    @Override
    public int getCount() {
        return nCount;
    }

    @Override
    public boolean onMove(int oldPosition, int newPosition) {
        boolean bRet = true;

        // get rid of old data
        mWindow.clear();
        bRet = mWindow.setNumColumns(theColumns.length);
        fillWindow(newPosition, mWindow);

        return bRet;
    }

    @Override
    public void fillWindow (int pos, CursorWindow window) {
        int    nNumRows = window.getNumRows();
        int nCIndex = 0;
        window.setStartPosition(0);

        if (pos > -1) {
            if (nNumRows == 0) {
                window.allocRow();
                nNumRows = window.getNumRows();
            }

            if (nNumRows == 1) {
                nCIndex = getColumnIndex(LENGTH);
                if (nCIndex > -1) {
                    window.putLong(lFileSize, 0, nCIndex);
                }
                nCIndex = getColumnIndex(_ID);
                if (nCIndex > -1) {
                    window.putLong((long)pos, 0, nCIndex);
                }
                nCIndex = getColumnIndex(CHUNK);
                if (nCIndex > -1) {
                    if (lLength != 0) {
                        byte[] value = getABlob (pos, 1);
                        window.putBlob(value, 0, nCIndex);
                    }
                }
            }
            window.setStartPosition(pos);
        }
        return;
    }

    public byte[] getABlob (int row, int column) {
        int    nRead = 0;
        int    nOffset = 0;
        int nBufSize = 0;

        if ((column == 1) && (theFile != null)) {
            try {
                FileInputStream fin = new FileInputStream(theFile);
                nOffset = row * BUFSIZE;
                if (row < (nCount - 1)) {
                    nBufSize = BUFSIZE;
                } else {
                    nBufSize = (int) (lFileSize - nOffset);
                }
                theBuffer = new byte[nBufSize];

                if (theBuffer != null) {
                    if (fin.skip(nOffset + lOffset) == (nOffset + lOffset)) {
                        if ((nRead = fin.read(theBuffer, 0, nBufSize)) != -1) {
                            if (nRead != nBufSize) {
                                return null;
                            }
                        }
                    }
                }

            fin.close();
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        return theBuffer;
    }
}
