/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
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
 */

package calypso.util;

import java.io.FilterInputStream;
import java.io.InputStream;
import java.io.IOException;

import calypso.util.ByteBuf;
import calypso.util.ByteLineBuffer;

public class LineBufferingInputStream extends FilterInputStream {

  private static final boolean DEBUG = false;

  private static final int CAPACITY = 1024;

  ByteBuf curline = new ByteBuf();
  ByteBuf inbuf = new ByteBuf(CAPACITY);
  ByteLineBuffer linebuf = new ByteLineBuffer();
  boolean eof = false;

  public LineBufferingInputStream(InputStream s) {
    super(s);

    if (DEBUG) {
      System.err.println("LBIS " + hashCode() + ": created for " + s);
    }
  }

  /** Reads bytes into a portion of an array.  This method will block
      until some input is available.
      <P>
      The returned data will not be more than one line long: that is, there
      will be at most one linebreak (CR, LF, or CRLF) and if it is present,
      it will be at the end.
      <P>
      If the next line available to be read is smaller than `length',
      then the first `length' bytes of that line will be returned, and
      the remainder of the bytes on that line will be available for
      subsequent reads.

      @return   the total number of bytes read into the buffer, or -1 if
                there is no more data because the end of the stream has
                been reached.
   */
  public int read(byte[] buf, int start, int length) throws IOException {

    if (DEBUG) {
      System.err.println("LBIS " + hashCode() + ": read(byte[], " +
                         start + ", " + length + ")");
    }


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

        if (DEBUG) {
          byte b[] = inbuf.toBytes();
          System.err.print("LBIS " + hashCode() + ": read " + inlen +
                           " bytes: ");
          for (int i = 0; i < inlen; i++)
            System.err.print((b[i] == '\r' ? "\\r" :
                              b[i] == '\n' ? "\\n" :
                              b[i] == '\t' ? "\\t" :
                              b[i] == '\\' ? "\\\\" :
                              new String(b, i, 1)));
          System.err.print("\n");
        }

        linebuf.pushBytes(inbuf);
      }

      curlen = curline.length();
      if (curlen == 0) return -1;
    }
    int result = length;
    if (result > curlen) result = curlen;
    curline.getBytes(0, result, buf, start);
    curlen -= result;
    curline.remove(0, result);

    if (DEBUG) {
      byte b[] = curline.toBytes();
      System.err.print("LBIS " + hashCode() + ": read line: ");
      for (int i = 0; i < curline.length(); i++)
        System.err.print((b[i] == '\r' ? "\\r" :
                          b[i] == '\n' ? "\\n" :
                          b[i] == '\t' ? "\\t" :
                          b[i] == '\\' ? "\\\\" :
                          new String(b, i, 1)));
      System.err.print("\n");
    }

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

  public void close() throws IOException {

    if (DEBUG) {
      System.err.println("LBIS " + hashCode() + ": closed");
    }

    curline = null;
    inbuf = null;
    linebuf = null;
    eof = true;
    super.close();
  }

  /** Sets the EOL characters to look for in the incoming stream.  Setting
   * this to be null will cause it to look for any of <CR>, <LF>, or <CRLF>.
   * Note that null (the default) could cause up to one extra to be held in
   * the buffer (in the case where the last byte read from the underlying
   * stream was <CR>, and no following byte (or EOF) has yet been read.)
   */
  public void setInputEOL(ByteBuf buf) {
    linebuf.setInputEOL(buf);
  }

  /**
   * End of line characters <CR> & <LF> or any combination
   * will be replaced by this string if it is present. Setting
   * this to a zero length string will cause them to be stripped.
   * Setting this to null will cause the EOL characters to be passed
   * through unchanged (the default).
   * @param buf  ByteBuf representing EOL replacement string
   */
    public void setOutputEOL(ByteBuf buf) {
      linebuf.setOutputEOL(buf);
    }


}
