/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.javascript.tools.shell;

import java.io.*;
import org.mozilla.javascript.*;

public class DebugReader extends BufferedReader {
    private BufferedReader reader;
    private SourceTextManager stm;
    private SourceTextItem sti;

    public DebugReader(Reader reader, SourceTextManager stm, String name) {
        super(null);
        this.reader = new BufferedReader(reader);                
        this.stm = stm;
        this.sti = stm.newItem(name);
    }

    public int read() throws IOException {
        int c = reader.read();
        if(null != sti) {
            if(-1 == c) {
                sti.complete();
                sti = null;
            }
            else
                sti.append((char)c);
        }
        return c;
    }

    public int read(char cbuf[]) throws IOException {
        int i = reader.read(cbuf);
        if(null != sti) {
            if(-1 == i) {
                sti.complete();
                sti = null;
            }
            else if(0 != i) {
                sti.append(cbuf, 0, i);
            }
        }
        return i;
    }

    public int read(char cbuf[], int off, int len) throws IOException {
        int i = reader.read(cbuf, off, len);
        if(null != sti) {
            if(-1 == i) {
                sti.complete();
                sti = null;
            }
            else if(0 != i) {
                sti.append(cbuf, off, i);
            }
        }
        return i;
    }

    public String readLine() throws IOException {
        String s = reader.readLine();
        if(null != sti) {
            if(null == s) {
                sti.complete();
                sti = null;
            }
            else {
                sti.append(s+"\n");
            }
        }
        return s;
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
        if(null != sti) {
            sti.complete();
            sti = null;
        }
    }

    protected void finalize() throws Throwable {
        if(null != sti) {
            sti.complete();
            sti = null;
        }
        reader = null;    
    }
}
