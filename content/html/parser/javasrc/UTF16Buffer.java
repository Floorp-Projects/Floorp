/*
 * Copyright (c) 2008 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

package nu.validator.htmlparser.impl;

import nu.validator.htmlparser.annotation.NoLength;

public final class UTF16Buffer {
    private final @NoLength char[] buffer;

    private int start;

    private int end;

    /**
     * @param buffer
     * @param start
     * @param end
     */
    public UTF16Buffer(@NoLength char[] buffer, int start, int end) {
        this.buffer = buffer;
        this.start = start;
        this.end = end;
    }

    /**
     * Returns the start.
     * 
     * @return the start
     */
    public int getStart() {
        return start;
    }

    /**
     * Sets the start.
     * 
     * @param start the start to set
     */
    public void setStart(int start) {
        this.start = start;
    }

    /**
     * Returns the buffer.
     * 
     * @return the buffer
     */
    public @NoLength char[] getBuffer() {
        return buffer;
    }

    /**
     * Returns the end.
     * 
     * @return the end
     */
    public int getEnd() {
        return end;
    }
    
    public boolean hasMore() {
        return start < end;
    }
    
    public void adjust(boolean lastWasCR) {
        if (lastWasCR && buffer[start] == '\n') {
            start++;
        }
    }

    /**
     * Sets the end.
     * 
     * @param end the end to set
     */
    public void setEnd(int end) {
        this.end = end;
    }
}
