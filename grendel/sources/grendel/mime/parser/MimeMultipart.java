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
 * Created: Jamie Zawinski <jwz@netscape.com>, 21 Aug 1997.
 */

package grendel.mime.parser;

import grendel.mime.IMimeOperator;

import calypso.util.ByteBuf;
import calypso.util.ByteLineBuffer;

import javax.mail.internet.InternetHeaders;


/** This class implements the parser for all "multipart/" MIME subtypes.
 * @see MimeMultipartDigest
 * @see MimeDwimText
 * @see MimeXSunAttachment
 */
class MimeMultipart extends MimeContainer {
    
    ByteLineBuffer line_buffer;
    ByteBuf line_bytes;
    ByteBuf boundary;
    ByteBuf crlf;
    InternetHeaders child_headers;
    
    int state;
    
    // enumeration of boundary types
    static protected final int BOUNDARY_NONE        = 100;
    static protected final int BOUNDARY_TERMINATOR  = 101;
    static protected final int BOUNDARY_SEPARATOR   = 102;
    
    // enumeration of states in the machine
    static protected final int MULTIPART_PREAMBLE   = 200;
    static protected final int MULTIPART_HEADERS    = 201;
    static protected final int MULTIPART_FIRST_LINE = 202;
    static protected final int MULTIPART_LINE       = 203;
    static protected final int MULTIPART_EPILOGUE   = 204;
    
    public MimeMultipart(String content_type, InternetHeaders headers) {
        super(content_type, headers);
        
        crlf = new ByteBuf("\r\n");
        line_buffer = new ByteLineBuffer();
        line_bytes = new ByteBuf(100);
        
        computeBoundary();
    }
    
    protected void computeBoundary() {
        // extract the boundary string from the Content-Type header.
        if (headers != null) {
            state = MULTIPART_PREAMBLE;
            ByteBuf ct = new ByteBuf();
            boundary = null;
            String hh[] = headers.getHeader("Content-Type");
            if (hh != null && hh.length != 0) {
                String ctv = hh[0];
                // #### do this right!
                int i = ctv.indexOf("boundary=");
                if (i > 0) {
                    int j;
                    i += 9;
                    if (ctv.charAt(i) == '"') {
                        i++;
                        j = ctv.indexOf('"', i);
                    } else {
                        j = ctv.indexOf(' ', i);
                        if (j < 0) j = ctv.length()-1;
                    }
                    boundary = new ByteBuf(ctv.substring(i, j));
                }
            }
        }
        
        if (boundary == null) {
            // Gag!  No boundary string?  Try and display it as plain-text.
            openChild("text/plain", null);
            state = MULTIPART_FIRST_LINE;
        }
    }
    
    /** When a sub-part doesn't have a content-type specified, this method
      returns the default type.  The default is "text/plain", but, for
      example, the "multipart/digest" handler overrides this to
      "message/rfc822".  Note that this is a class method.
     */
    public String defaultSubpartType() { return "text/plain"; }
    
    /** Buffers the bytes into lines, and calls process_line() on each line. */
    public void pushBytes(ByteBuf bytes) {
        line_buffer.pushBytes(bytes);
        while (line_buffer.pullLine(line_bytes))
            process_line(line_bytes);
    }
    
    /** Flushes the line buffer, and (maybe) calls process_line() one last time.
     */
    public void pushEOF() {
        line_buffer.pushEOF();
        while (line_buffer.pullLine(line_bytes))
            process_line(line_bytes);
        line_buffer = null;
        line_bytes = null;
        super.pushEOF();
    }
    
    
    /** Called for each line of the body of this multipart.
      Maintains a state machine and decides what the line means.
     */
    private void process_line(ByteBuf line) {
        
        int boundary_type;
        
        if (state == MULTIPART_EPILOGUE)
            boundary_type = BOUNDARY_NONE;
        else
            boundary_type = checkBoundary(line);
        
        if (boundary_type == BOUNDARY_TERMINATOR ||
                boundary_type == BOUNDARY_SEPARATOR) {
            // Match!  Close the currently-open part, move on to the next
            // state, and discard this line.
            if (state != MULTIPART_PREAMBLE)
                closeChild();
            
            if (boundary_type == BOUNDARY_TERMINATOR) {
                state = MULTIPART_EPILOGUE;
                child_headers = null;
            } else {
                state = MULTIPART_HEADERS;
                child_headers = new InternetHeaders();
            }
            
        } else {
            // this line is not a boundary line.
            
            switch (state) {
                case MULTIPART_PREAMBLE:
                case MULTIPART_EPILOGUE:
                    // ignore this line.
                    break;
                    
                case MULTIPART_HEADERS:
                    // parse this line as a header for the sub-part.
                    child_headers.addHeaderLine(line.toString());
                    
                    // If this is a blank line, create a child.
                    byte b = line.byteAt(0);
                    if (b == '\r' || b == '\n') {
                        createMultipartChild();
                        state = MULTIPART_FIRST_LINE;
                    }
                    break;
                    
                case MULTIPART_FIRST_LINE:
                    // hand this line off to the sub-part.
                    push_child_line(line, true);
                    state = MULTIPART_LINE;
                    break;
                    
                case MULTIPART_LINE:
                    // hand this line off to the sub-part.
                    push_child_line(line, false);
                    break;
                    
                default:
                    throw new Error("internal error");
            }
        }
    }
    
    
    protected int checkBoundary(ByteBuf line_buf) {
        
        if (boundary == null)
            return BOUNDARY_NONE;
        
        int length = line_buf.length();
        byte line[] = line_buf.toBytes();
        
        if (length < 3 || line[0] != '-' || line[1] != '-')
            return BOUNDARY_NONE;
        
        // strip trailing whitespace (including the newline.)
        while(length > 2 && (line[length-1] <= ' '))
            length--;
        
        int blen = boundary.length();
        boolean term_p = false;
        
        // Could this be a terminating boundary?
        if (length == blen + 4 &&
                line[length-1] == '-' &&
                line[length-2] == '-') {
            term_p = true;
            length -= 2;
        }
        
        // If the two don't have the same length (modulo trailing dashes)
        // then they can't match.
        if (blen != length-2)
            return BOUNDARY_NONE;
        
        // Compare each byte, and bug out if there's a mismatch.
        // (Note that if `boundary' is "xxx" then `line' is "--xxx";
        // that's what the +2 is for.)
        byte bbytes[] = boundary.toBytes();
        for (int i = 0; i < length-2; i++)
            if (line[i+2] != bbytes[i])
                return BOUNDARY_NONE;
        
        // Success!  Return the type of boundary.
        if (term_p)
            return BOUNDARY_TERMINATOR;
        else
            return BOUNDARY_SEPARATOR;
    }
    
    
    protected void createMultipartChild() {
        String hh[] = child_headers.getHeader("Content-Type");
        String child_type = null;
        
        if (hh != null && hh.length != 0) {
            child_type = hh[0];
            // #### strip to first token
            int i = child_type.indexOf(';');
            if (i > 0)
                child_type = child_type.substring(0, i);
            child_type = child_type.trim();
        }
        
        if (child_type != null) {
            openChild(child_type, child_headers);
        } else {
            // If there was no content-type, assume text/plain -- null as a ct
            // would mean that we should do the auto-uudecode-hack, which we don't
            // ever want to do for subparts of a multipart (only for untyped
            // children of message/rfc822.)
            openChild(defaultSubpartType(), child_headers);
        }
    }
    
    
    private void push_child_line(ByteBuf line_buf, boolean first_line_p) {
        
        // The newline issues here are tricky, since both the newlines before
        // and after the boundary string are to be considered part of the
        // boundary: this is so that a part can be specified such that it
        // does not end in a trailing newline.
        //
        // To implement this, we send a newline *before* each line instead
        // of after, except for the first line, which is not preceeded by a
        // newline.
        
        int length = line_buf.length();
        byte line[] = line_buf.toBytes();
        
        // Remove the trailing newline...
        if (length > 0 && line[length-1] == '\n') length--;
        if (length > 0 && line[length-1] == '\r') length--;
        line_buf.setLength(length);
        
        if (!first_line_p)
            // Push out a preceeding newline...
            open_child.pushBytes(crlf);
        
        // Now push out the line sans trailing newline.
        open_child.pushBytes(line_buf);
    }
}
