/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Created: Terry Weissman <terry@netscape.com>,  5 Sep 1997.
 */

package grendel.storage;

import java.io.FilterInputStream;
import java.io.InputStream;
import java.io.IOException;

import calypso.util.Assert;
import calypso.util.ByteBuf;
import calypso.util.ByteLineBuffer;

class DotTerminatedInputStream extends FilterInputStream {

  private static final int CAPACITY = 1024;

  ByteBuf curline = new ByteBuf();
  ByteBuf inbuf = new ByteBuf(CAPACITY);
  ByteLineBuffer linebuf;
  boolean eof = false;

  DotTerminatedInputStream(InputStream s) {
    super(s);
    linebuf = new ByteLineBuffer();
    ByteBuf inputeol = new ByteBuf();
    inputeol.append((byte) '\r');
    inputeol.append((byte) '\n');
    linebuf.setInputEOL(inputeol);
  }

  public int read(byte[] buf, int start, int length) throws IOException {
    if (length <= 0) return length;
    int curlen = curline.length();
    if (curlen == 0) {
      if (eof) return -1;
      while (!linebuf.pullLine(curline)) {
        inbuf.setLength(CAPACITY);
        int inlen = in.read(inbuf.toBytes(), 0, CAPACITY);
        if (inlen <= 0) {
          eof = true;
          linebuf.pushEOF();
          linebuf.pullLine(curline); // Try for the last gasp...
          break;
        }
        inbuf.setLength(inlen);
// System.out.println("<< " + inbuf);
        linebuf.pushBytes(inbuf);
      }
// System.out.println(">> " + curline);
      curlen = curline.length();
      if (curlen >= 2 && curline.byteAt(0) == '.') {
        byte c = curline.byteAt(1);
        if (c == '\r' || c == '\n') {
          eof = true;
          curline.setLength(0);
          curlen = 0;
        } else if (c == '.' && curlen >= 3) {
          c = curline.byteAt(2);
          if (c == '\r' || c == '\n') {
            // This line is a "..", which we need to translate to ".".
            curline.remove(0, 1);
            curlen--;
          }
        }
      }
      if (curlen == 0) return -1;
    }
    int result = length;
    if (result > curlen) result = curlen;
    curline.getBytes(0, result, buf, start);
    curlen -= result;
    curline.remove(0, result);
// for (int i=0 ; i<result ; i++) System.out.print(buf[i]);
    return result;
  }


  public int read(byte[] buf) throws IOException {
    return read(buf, 0, buf.length);
  }

  byte[] singlebyte;
  public int read() throws IOException {
    if (singlebyte == null) singlebyte = new byte[1];
    if (read(singlebyte, 0, 1) <= 0) return -1;
    return singlebyte[0];
  }

  public long skip(long n) {
    return 0;                   // Feh.
  }

  public boolean markSupported() {
    return false;
  }

  public void close() {
    if (!eof) {
      // Flush out the rest of the data.
      byte tmp[] = new byte[CAPACITY];
      int numread;
      do {
        numread = -1;
        try {
          numread = read(tmp, 0, CAPACITY);
        } catch (IOException e) {
        }
      } while (numread > 0);
    }
  }

  // Suck everything up into a buffer, so that we can use the input socket
  // for something else.  Right now, we just suck it into memory; we should
  // put it on a temp file if it becomes real big.  ###
  public void bufferUpEverything() {
    if (eof) return;
    ByteBuf tmp = new ByteBuf(CAPACITY);
    int length = 0;
    for (;;) {
      tmp.ensureCapacity(length + CAPACITY);
      int numread = -1;
      try {
        numread = read(tmp.toBytes(), length, CAPACITY);
      } catch (IOException e) {
      }
      if (numread <= 0) break;
      length += numread;
    }
    Assert.Assertion(eof);
    curline = tmp;
  }


}
