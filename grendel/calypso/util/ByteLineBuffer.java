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

/**
 * This class is stores data in a private ByteBuf appending new
 * data as needed, and returning line separated data.
 */

public class ByteLineBuffer {

    private static final boolean DEBUG = false;

    private boolean buffer_full;
    private ByteBuf buffer;
    private ByteBuf outputeol;
    private ByteBuf inputeol;

  /**
   * Constructs an empty ByteLineBuffer.
   */

    public ByteLineBuffer() {
        buffer = new ByteBuf();
        buffer_full = false;

        if (DEBUG) {
            System.err.println("BLB " + hashCode() + ": created");
        }
    }

  /**
   * Appends data to the end of the internal ByteBuf.
   * @param buf  data to append to internal buffer
   */
    public void pushBytes(ByteBuf buf) {
        if (DEBUG) {
            System.err.print("BLB " + hashCode() + ": pushBytes(\"");
            byte b[] = buf.toBytes();
            for (int i = 0; i < buf.length(); i++)
                System.err.print((b[i] == '\r' ? "\\r" :
                                  b[i] == '\n' ? "\\n" :
                                  b[i] == '\t' ? "\\t" :
                                  b[i] == '"'  ? "\\\"" :
                                  b[i] == '\\' ? "\\\\" :
                                  new String(b, i, 1)));
            System.err.print("\")\n");
        }
        buffer.append(buf);
    }

  /**
   * indicates that the last piece of data is now present.
   */

    public void pushEOF() {
        if (DEBUG) {
            System.err.println("BLB " + hashCode() + ": pushEOF()");
        }
        buffer_full = true;
    }

  /** Sets the EOL characters to look for in the incoming stream.  Setting
   * this to be null will cause it to look for any of <CR>, <LF>, or <CRLF>.
   * Note that null (the default) could cause up to one extra to be held in
   * the buffer (in the case where the last byte read from the underlying
   * stream was <CR>, and no following byte (or EOF) has yet been read.)
   */
  public void setInputEOL(ByteBuf buf) {
      Assert.Assertion(buf == null || buf.length() > 0);
      inputeol = buf;
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
        outputeol = buf;
    }

  /**
   * Returns a ByteBuf with a line of data which was separated by
   * <CR><LF> or any combination.  Holds the last line in the ByteBuf
   * until pushEOF() is called.  If a ByteBuf containing data is sent in,
   * the data is removed.
   * @param buf  single line of data in a ByteBuf
   * @return true if a line was retrieved, false if not
   */

    public boolean pullLine(ByteBuf buf) {
        int position;
        int buffer_length;
        byte value;
        // Remove any data that already exists in the buffer sent in
        buf.setLength(0);

        if (buffer_full && buffer == null)
          return false;

        buffer_length = buffer.length();

        if (inputeol != null) {
            int last = buffer_length - inputeol.length() + 1;
            byte first = inputeol.byteAt(0);
            byte[] peekbuf = buffer.toBytes();
            for (position = 0 ; position < last ; position++) {
                if (peekbuf[position] == first) {
                    boolean match = true;
                    for (int j=inputeol.length() - 1 ; j>=1 ; j--) {
                        if (peekbuf[position + j] != inputeol.byteAt(j)) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        buf.setLength(0);
                        if (outputeol != null) {
                            buf.append(peekbuf, 0, position);
                            buf.append(outputeol);
                        } else {
                            buf.append(peekbuf, 0,
                                       position + inputeol.length());
                        }
                        buffer.remove(0, position + inputeol.length());

                        if (DEBUG) {
                            System.err.print("BLB " + hashCode() +
                                             ": EOL matched; returning: \"");
                            byte b[] = buf.toBytes();
                            for (int i = 0; i < buf.length(); i++)
                                System.err.print((b[i] == '\r' ? "\\r" :
                                                  b[i] == '\n' ? "\\n" :
                                                  b[i] == '\t' ? "\\t" :
                                                  b[i] == '"'  ? "\\\"" :
                                                  b[i] == '\\' ? "\\\\" :
                                                  new String(b, i, 1)));
                            System.err.print("\"\n");
                        }

                        return true;
                    }
                }
            }
        } else {
            // process the entire buffer, or until a newline character is hit
            for (position = 0; position < buffer_length; position++) {
                value = buffer.byteAt(position);
                if (value == '\n' || value == '\r') {

                    boolean ambiguous = true;

                    position++;
                    // got a newline, pass it
                    if (value == '\r' && position < buffer_length) {

                        if (DEBUG) {
                            System.err.println("BLB " + hashCode() +
                                             ": got \\r not at end of buffer");
                        }

                        // if we have seen \r, and we know what character
                        // lies after it, then there is no ambiguity:
                        // the linebreak is "\r" or "\r\n", and we can
                        // tell which it is.
                        //
                        ambiguous = false;

                        // if we are not replacing the EOL chars, return the
                        // existing ones
                        if (outputeol == null)
                            buf.append(value);
                        value = buffer.byteAt(position);
                        if (value == '\n') {
                            position++;
                            if (outputeol == null)
                                buf.append(value);
                        }
                    } else {

                        if (DEBUG) {
                            if (value == '\r')
                                System.err.println("BLB " + hashCode() +
                                               ": got \\r at end of buffer");
                            else
                                System.err.println("BLB " + hashCode() +
                                                   ": got \\n without \\r");
                        }

                        // If we have seen \n, then the linebreak is
                        // unambiguously "\n".  If we have seen \r, but we
                        // don't know what character follows it (because
                        // it happened to be at the end of the buffer) then
                        // the linebreak is ambiguous ("\r" or "\r\n").
                        //
                        ambiguous = (value == '\r');

                        if (outputeol == null) {
                            buf.append(value);
                        }
                    }

                    // if this is the last line in the buffer, and pushEOF()
                    // hasn't been called, and the linebreak was ambiguous,
                    // then wait for more data or a call to pushEOF() before
                    // returning this line.
                    //
                    if (position == buffer_length &&
                        !buffer_full &&
                        ambiguous) {

                        if (DEBUG) {
                            System.err.println("BLB " + hashCode() +
                                               ": waiting for next line");
                        }

                        return false;
                    }
                    // remove the string from the internal ByteBuf
                    buffer.remove(0,position);
                    // if we have a replacement EOL string, use it
                    if (outputeol != null)
                        buf.append(outputeol);

                    if (DEBUG) {
                        System.err.print("BLB " + hashCode() +
                                         ": returning line: \"");
                        byte b[] = buf.toBytes();
                        for (int i = 0; i < buf.length(); i++)
                            System.err.print((b[i] == '\r' ? "\\r" :
                                              b[i] == '\n' ? "\\n" :
                                              b[i] == '\t' ? "\\t" :
                                              b[i] == '"'  ? "\\\"" :
                                              b[i] == '\\' ? "\\\\" :
                                              new String(b, i, 1)));
                        System.err.print("\"\n");
                    }

                    return true;
                }
                else
                    // append bytes to output ByteBuf
                    buf.append(value);
            }
        }
        // process full buffer
        if (buffer_full && buffer_length > 0)
        {
            if (outputeol != null)
                buf.append(outputeol);
            buffer.remove(0,position);
            buffer.Recycle(buffer);
            buffer = null;

            if (DEBUG) {
                System.err.print("BLB " + hashCode() +
                                 ": at EOF; returning line: \"");
                byte b[] = buf.toBytes();
                for (int i = 0; i < buf.length(); i++)
                    System.err.print((b[i] == '\r' ? "\\r" :
                                      b[i] == '\n' ? "\\n" :
                                      b[i] == '\t' ? "\\t" :
                                      b[i] == '"'  ? "\\\"" :
                                      b[i] == '\\' ? "\\\\" :
                                      new String(b, i, 1)));
                System.err.print("\"\n");
            }

            return true;
        }

        // didn't get a string this time around.
        buf.setLength(0);

        if (DEBUG) {
            System.err.println("BLB " + hashCode() +
                               ": don't have a line yet");
        }

        return false;
    }

//  public static void main(String args[]) {
//
//    ByteLineBuffer blb = new ByteLineBuffer();
//    ByteBuf bb = new ByteBuf();
//    ByteBuf line_bytes = new ByteBuf(100);
//
//    bb.setLength(0);
//    bb.append("from: jwz");
//    System.err.println("\npushed \"" + bb + "\""); blb.pushBytes(bb);
//    while(blb.pullLine(line_bytes)) {
//      System.err.println(" pulled \"" + line_bytes + "\"");
//    }
//
//    bb.setLength(0);
//    bb.append("\r\n");
//    System.err.println("\npushed \"" + bb + "\""); blb.pushBytes(bb);
//    while(blb.pullLine(line_bytes)) {
//      System.err.println(" pulled \"" + line_bytes + "\"");
//    }
//
//    bb.setLength(0);
//    bb.append("content-type: text/plain");
//    System.err.println("\npushed \"" + bb + "\""); blb.pushBytes(bb);
//    while(blb.pullLine(line_bytes)) {
//      System.err.println(" pulled \"" + line_bytes + "\"");
//    }
//
//    bb.setLength(0);
//    bb.append("\r\n");
//    System.err.println("\npushed \"" + bb + "\""); blb.pushBytes(bb);
//    while(blb.pullLine(line_bytes)) {
//      System.err.println(" pulled \"" + line_bytes + "\"");
//    }
//
//    bb.setLength(0);
//    System.err.println("\npushed EOF"); blb.pushEOF();
//    while(blb.pullLine(line_bytes)) {
//      System.err.println(" pulled \"" + line_bytes + "\"");
//    }
//  }

}
