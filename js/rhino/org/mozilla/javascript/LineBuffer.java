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

package org.mozilla.javascript;

import java.io.Reader;
import java.io.IOException;

/**
 * An input buffer that combines fast character-based access with
 * (slower) support for retrieving the text of the current line.  It
 * also supports building strings directly out of the internal buffer
 * to support fast scanning with minimal object creation.
 *
 * Note that it is customized in several ways to support the
 * TokenStream class, and should not be considered general.
 *
 * Credits to Kipp Hickman and John Bandhauer.
 *
 * @author Mike McCabe
 */
final class LineBuffer {
    /*
     * for smooth operation of getLine(), this should be greater than
     * the length of any expected line.  Currently, 256 is 3% slower
     * than 4096 for large compiles, but seems safer given evaluateString.
     * Strings for the scanner are are built with StringBuffers
     * instead of directly out of the buffer whenever a string crosses
     * a buffer boundary, so small buffer sizes will mean that more
     * objects are created.
     */
    static final int BUFLEN = 256;

    LineBuffer(Reader in, int lineno) {
        this.in = in;
        this.lineno = lineno;
    }

    int read() throws IOException {
        if (end == offset && !fill())
            return -1;

        // Do only a bitmask + branch per character, at the cost of
        // three branches per low-bits-only character.
        if ((buffer[offset] & '\ufff0') == 0) {
            if (buffer[offset] == '\r') {
                // if the next character is a newline, skip past it.
                if ((offset + 1) < end) {
                    if (buffer[offset + 1] == '\n')
                        offset++;
                } else {
                    // set a flag for fill(), in case the first char of the
                    // next fill is a newline.
                    lastWasCR = true;
                }
            }
            else if (buffer[offset] != '\n') {
                return (int) buffer[offset++];
            }
            offset++;
            prevStart = lineStart;
            lineStart = offset;
            lineno++;
            return '\n';
        }

        return (int) buffer[offset++];
    }

    void unread() {
        if (offset == 0)
            // We can get here when we're asked to unread() an
            // implicit EOF_CHAR.
            
            // This would also be wrong behavior in the general case,
            // because a peek() could map a buffer.length offset to 0
            // in the process of a fill(), and leave it there.  But
            // the scanner never calls peek() or a failed match()
            // followed by unread()... this would violate 1-character
            // lookahead.  So we're OK.
            return;
        offset--;
        if ((buffer[offset] & '\ufff0') == 0
            && (buffer[offset] == '\r' || buffer[offset] == '\n')) {
            // back off from the line start we presumably just registered...
            lineStart = prevStart;
            lineno--;
        }
    }

    int peek() throws IOException {
        if (end == offset && !fill())
            return -1;

        if (buffer[offset] == '\r')
            return '\n';

        return buffer[offset];
    }

    boolean match(char c) throws IOException {
        if (end == offset && !fill())
            return false;

        // This'd be a place where we'd need to map '\r' to '\n' and
        // do other updates, but TokenStream never looks ahead for
        // '\n', so we don't bother.
        if (buffer[offset] == c) {
            offset++;
            return true;
        }
        return false;
    }

    // Reconstruct a source line from the buffers.  This can be slow...
    String getLine() {
        StringBuffer result = new StringBuffer();

        int start = lineStart;
        if (start >= offset) {
            // the line begins somewhere in the other buffer; get that first.
            if (otherStart < otherEnd)
                // if a line ending was seen in the other buffer... otherwise
                // just ignore this strange case.
                result.append(otherBuffer, otherStart,
                              otherEnd - otherStart);
            start = 0;
        }

        // get the part of the line in the current buffer.
        result.append(buffer, start, offset - start);

        // Get the remainder of the line.
        int i = offset;
        while(true) {
            if (i == buffer.length) {
                // we're out of buffer, let's just expand it.  We do
                // this instead of reading into a StringBuffer to
                // preserve the stream for later reads.
                char[] newBuffer = new char[buffer.length * 2];
                System.arraycopy(buffer, 0, newBuffer, 0, buffer.length);
                buffer = newBuffer;
                int charsRead = 0;
                try {
                    charsRead = in.read(buffer, end, buffer.length - end);
                } catch (IOException ioe) {
                    // ignore it, we're already displaying an error...
                }
                if (charsRead < 0)
                    break;
                end += charsRead;
            }
            if (buffer[i] == '\r' || buffer[i] == '\n')
                break;
            i++;
        }

        result.append(buffer, offset, i - offset);
        return result.toString();
    }

    // Get the offset of the current character, relative to
    // the line that getLine() returns.
    int getOffset() {
        if (lineStart >= offset)
            // The line begins somewhere in the other buffer.
            return offset + (otherEnd - otherStart);
        else
            return offset - lineStart;
    }

    // Set a mark to indicate that the reader should begin
    // accumulating characters for getString().  The string begins
    // with the last character read.
    void startString() {
        if (offset == 0) {
            // We can get here if startString is called after a peek()
            // or failed match() with offset past the end of the
            // buffer.

            // We're at the beginning of the buffer, and the previous character
            // (which we want to include) is at the end of the last one, so
            // we just go to StringBuffer mode.
            stringSoFar = new StringBuffer();
            stringSoFar.append(otherBuffer, otherEnd - 1, 1);

            stringStart = -1; // Set sentinel value.
        } else {
            // Support restarting strings
            stringSoFar = null;
            stringStart = offset - 1;
        }
    }

    // Get a string consisting of the characters seen since the last
    // startString.
    String getString() {
        String result;

        /*
         * There's one strange case here:  If the character offset currently
         * points to (which we never want to include in the string) is
         * a newline, then if the previous character is a carriage return,
         * we probably want to exclude that as well.  If the offset is 0,
         * then we hope that fill() handled excluding it from stringSoFar.
         */
        int loseCR = (offset > 0 &&
                      buffer[offset] == '\n' && buffer[offset - 1] == '\r') ?
            1 : 0;

        if (stringStart != -1) {
            // String mark is valid, and in this buffer.

            result = new String(buffer, stringStart, 
                                offset - stringStart - loseCR);
        } else {
            // Exclude cr as well as nl of newline.  If offset is 0, then
            // hopefully fill() did the right thing.
            result = (stringSoFar.append(buffer, 0, offset - loseCR)).toString();
        }
        
        stringStart = -1;
        stringSoFar = null;
        return result;
    }            

    boolean fill() throws IOException {
        // not sure I care...
        if (end - offset != 0) 
            throw new IOException("fill of non-empty buffer");

        // If there's a string currently being accumulated, save
        // off the progress.

        /*
         * Exclude an end-of-buffer carriage return.  NOTE this is not
         * fully correct in the general case, because we really only
         * want to exclude the carriage return if it's followed by a
         * linefeed at the beginning of the next buffer.  But we fudge
         * because the scanner doesn't do this.
         */
        int loseCR = (offset > 0 && lastWasCR) ? 1 : 0;

        if (stringStart != -1) {
            // The mark is in the current buffer, save off from the mark to the
            // end.
            stringSoFar = new StringBuffer();

            stringSoFar.append(buffer, stringStart, end - stringStart - loseCR);
            stringStart = -1;
        } else if (stringSoFar != null) {
            // the string began prior to the current buffer, so save the
            // whole current buffer.
            stringSoFar.append(buffer, 0, end - loseCR);
        }

        // swap buffers
        char[] tempBuffer = buffer;
        buffer = otherBuffer;
        otherBuffer = tempBuffer;

        // allocate the buffers lazily, in case we're handed a short string.
        if (buffer == null) {
            buffer = new char[BUFLEN];
        }

        // buffers have switched, so move the newline marker.
        otherStart = lineStart;
        otherEnd = end;

        // set lineStart to a sentinel value, unless this is the first
        // time around.
        prevStart = lineStart = (otherBuffer == null) ? 0 : buffer.length + 1;
        
        offset = 0;
        end = in.read(buffer, 0, buffer.length);
        if (end < 0) {
            end = 0;

            // can't null buffers here, because a string might be retrieved
            // out of the other buffer, and a 0-length string might be
            // retrieved out of this one.

            hitEOF = true;
            return false;
        }

        // If the last character of the previous fill was a carriage return,
        // then ignore a newline.

        // There's another bizzare special case here.  If lastWasCR is
        // true, and we see a newline, and the buffer length is
        // 1... then we probably just read the last character of the
        // file, and returning after advancing offset is not the right
        // thing to do.  Instead, we try to ignore the newline (and
        // likely get to EOF for real) by doing yet another fill().
        if (lastWasCR) {
            if (buffer[0] == '\n') {
              offset++;
              if (end == 1)
                  return fill();
            }
            lineStart = offset;
            lastWasCR = false;
        }
        return true;
    }

    int getLineno() { return lineno; }
    boolean eof() { return hitEOF; }
    
    private Reader in;
    private char[] otherBuffer = null;
    private char[] buffer = null;

    // Yes, there are too too many of these.
    private int offset = 0;
    private int end = 0;
    private int otherEnd;
    private int lineno;

    private int lineStart = 0;
    private int otherStart = 0;
    private int prevStart = 0;
    
    private boolean lastWasCR = false;
    private boolean hitEOF = false;

    private int stringStart = -1;
    private StringBuffer stringSoFar = null;
}


