/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript.debug;

import java.io.*;

public class DebugReader extends Reader {

    public DebugReader(Reader reader) {
        this.reader = new BufferedReader(reader);  
        this.saved = new StringBuffer();
    }
    
    public StringBuffer getSaved() {
        return saved;
    }

    public int read() throws IOException {
        int c = reader.read();
        if (c != -1)
            saved.append((char)c);
        return c;
    }

    public int read(char cbuf[]) throws IOException {
        int i = reader.read(cbuf);
        if (i != -1) 
            saved.append(cbuf, 0, i);
        return i;
    }

    public int read(char cbuf[], int off, int len) throws IOException {
        int i = reader.read(cbuf, off, len);
        if (i > 0) 
            saved.append(cbuf, off, i);
        return i;
    }

    public long skip(long n) throws IOException {
        return reader.skip(n);
    }

    public boolean ready() throws IOException {
        return reader.ready();
    }

    public boolean markSupported() {
        return reader.markSupported();
    }

    public void mark(int readAheadLimit) throws IOException {
        reader.mark(readAheadLimit);
    }

    public void reset() throws IOException {
        reader.reset();
    }

    public void close() throws IOException {
        reader.close();
    }

    protected void finalize() throws Throwable {
        reader = null;    
    }
    
    private BufferedReader reader;
    private StringBuffer saved;
}
