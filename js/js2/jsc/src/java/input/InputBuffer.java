/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mountain View Compiler
 * Company.  Portions created by Mountain View Compiler Company are
 * Copyright (C) 1998-2000 Mountain View Compiler Company. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Jeff Dyer <jeff@compilercompany.com>
 */

/**
 * Filters and buffers characters from a input reader.
 */

package com.compilercompany.ecmascript;
import java.io.*;

public class InputBuffer implements CharacterClasses {

    private static final boolean debug = false;
    private static final boolean debug_nextchar = true;
    private static final boolean debug_retract = false;

    StringBuffer lineA = new StringBuffer();
    StringBuffer lineB = new StringBuffer();
    StringBuffer curr_line,prev_line;
    int curr_line_offset,prev_line_offset;
    int lineA_offset, lineB_offset;
    
    private StringBuffer text;
    private int[] line_breaks = new int[1000];

    Reader in;
    PrintStream err;
    String origin;

    int pos;
    int colPos, lnNum=-1; // <0,0> is the position of the first character.

    public static PrintStream out;
    public static void setOut(String filename) throws Exception {
        out = new PrintStream( new FileOutputStream(filename) );
    }

    public static void main(String[] args) {
        test_retract();
        test_markandcopy();
        test_getLineText();
    }

    public InputBuffer( Reader in, PrintStream err, String origin ) {
        this(in,err,origin,0);
    }

    public InputBuffer( Reader in, PrintStream err, String origin, int pos ) {
        this.in  = in;
        this.err = err;
        this.origin = origin;
        this.pos = pos;
        this.text = new StringBuffer();
    }

    /**
     * read()
     *
     * Read the next character from the input reader and store it in a buffer
     * of the full text.
     */

    private int read() throws Exception {
        int c = in.read();
        text.append((char)c);
        pos++;
        return c;
    }

    /**
     * text()
     */

    public String source() {
        return text.toString();
    }

    /**
     * nextchar() -- 
     *
     * The basic function of nextchar() is to fetch the next character,
     * in the input array, increment next and return the fetched character.
     *
     * To simplify the Scanner, this method also does the following:
     * 1. normalizes certain special characters to a common character.
     * 2. skips unicode format control characters (category Cf).
     * 3. keeps track of line breaks, line position and line number.
     * 4. treats <cr>+<lf> as a single line terminator.
     * 5. returns 0 when the end of input is reached.
     */
      
    public final int nextchar() throws Exception {

        int c = -1;

        // If the last character was at the end of a line,
        // then swap buffers and fill the new one with characters
        // from the input reader.

        if( curr_line==null || colPos==curr_line.length() ) {

            lnNum++;
            colPos=0;

            // If the current character is a newline, then read
            // the next line of input into the other input buffer.
            
            if( curr_line == lineA ) {
                if( lineB == null ) {
                    lineB = new StringBuffer();
                    lineB_offset = pos;
                }
                curr_line = lineB; curr_line_offset = lineB_offset;
                prev_line = lineA; prev_line_offset = lineA_offset;
                lineA = null;
            } else {
                if( lineA == null ) {
                    lineA = new StringBuffer();
                    lineA_offset = pos;
                }
                curr_line = lineA; curr_line_offset = lineA_offset;
                prev_line = lineB; prev_line_offset = lineB_offset;
                lineB = null;
            }

            while(c != '\n' && c != 0) {

                c = read();

                if( false ) {
                    Debugger.trace( "c = " + c );
                }

                // Skip Unicode 3.0 format-control (general category Cf in
                // Unicode Character Database) characters. 

                while(true) {

                    switch(c) {
                        case 0x070f: // SYRIAC ABBREVIATION MARK
                        case 0x180b: // MONGOLIAN FREE VARIATION SELECTOR ONE
                        case 0x180c: // MONGOLIAN FREE VARIATION SELECTOR TWO
                        case 0x180d: // MONGOLIAN FREE VARIATION SELECTOR THREE
                        case 0x180e: // MONGOLIAN VOWEL SEPARATOR
                        case 0x200c: // ZERO WIDTH NON-JOINER
                        case 0x200d: // ZERO WIDTH JOINER
                        case 0x200e: // LEFT-TO-RIGHT MARK
                        case 0x200f: // RIGHT-TO-LEFT MARK
                        case 0x202a: // LEFT-TO-RIGHT EMBEDDING
                        case 0x202b: // RIGHT-TO-LEFT EMBEDDING
                        case 0x202c: // POP DIRECTIONAL FORMATTING
                        case 0x202d: // LEFT-TO-RIGHT OVERRIDE
                        case 0x202e: // RIGHT-TO-LEFT OVERRIDE
                        case 0x206a: // INHIBIT SYMMETRIC SWAPPING
                        case 0x206b: // ACTIVATE SYMMETRIC SWAPPING
                        case 0x206c: // INHIBIT ARABIC FORM SHAPING
                        case 0x206d: // ACTIVATE ARABIC FORM SHAPING
                        case 0x206e: // NATIONAL DIGIT SHAPES
                        case 0x206f: // NOMINAL DIGIT SHAPES
                        case 0xfeff: // ZERO WIDTH NO-BREAK SPACE
                        case 0xfff9: // INTERLINEAR ANNOTATION ANCHOR
                        case 0xfffa: // INTERLINEAR ANNOTATION SEPARATOR
                        case 0xfffb: // INTERLINEAR ANNOTATION TERMINATOR
                            c = read();
                            continue; // skip it.
                        default:
                            break;
                    }
                    break; // out of while loop.
                }                  


                if( c == 0x000a && prev_line.length()!=0 && prev_line.charAt(prev_line.length()-1) == 0x000d ) {

                    // If this is one of those funny double line terminators,
                    // Then ignore the second character by reading on.

                    line_breaks[lnNum] = pos; // adjust if forward.
                    c = read();
                } 
                
                // Line terminators.

                if( c == 0x000a ||
                    c == 0x000d ||
                    c == 0x2028 ||
                    c == 0x2029 ) {

                    curr_line.append((char)c);
                    c = '\n';
                    line_breaks[lnNum+1] = pos;
                
                // White space

                } else if( c == 0x0009 ||
                           c == 0x000b ||
                           c == 0x000c ||
                           c == 0x0020 ||
                           c == 0x00a0 ||
                           false /* other cat Zs' */  ) {

                    c = ' ';
                    curr_line.append((char)c);
                
                // End of line

                } else if( c == -1 ) {

                    c = 0;
                    curr_line.append((char)c);
                
                // All other characters.
                
                } else {

                    // Use c as is.
                    curr_line.append((char)c);

                }
            }
        }

        // Get the next character.

        int ln  = lnNum;
        int col = colPos;

        if( curr_line.length() != 0 && curr_line.charAt(colPos) == 0 ) {
            c = 0;
            colPos++;
        } else if( colPos == curr_line.length()-1 ) {
            c = '\n';
            colPos++;
        } else {
            c = curr_line.charAt(colPos++);
        }

        if( out != null ) {
            out.println("Ln " + ln + ", Col " + col + ": nextchar " + Integer.toHexString(c) + " = " + (char)c + " @ " + positionOfNext());
        }
        if( debug || debug_nextchar ) {
            Debugger.trace("Ln " + ln + ", Col " + col + ": nextchar " + Integer.toHexString(c) + " = " + (char)c + " @ " + positionOfNext());
        }

        return c;
    }

    /**
     * test_nextchar() -- 
     * Return an indication of how nextchar() performs
     * in various situations, relative to expectations.
     */

    boolean test_nextchar() {
        return true;
    }

    /**
     * time_nextchar() -- 
     * Return the milliseconds taken to do various ordinary
     * tasks with nextchar().
     */

    int time_nextchar() {
        return 0;
    }

    /**
     * retract
     *
     * Backup one character position in the input. If at the beginning
     * of the line, then swap buffers and point to the last character
     * in the other buffer.
     */

    public final void retract() throws Exception {
        colPos--;
        if( colPos<0 ) {
            if( curr_line == prev_line ) {
                // Can only retract over one line.
                throw new Exception("Can only retract past one line at a time.");
            } else if( curr_line == lineA ) {
                curr_line = lineB = prev_line;
            } else {
                curr_line = lineA = prev_line;
            }
            lnNum--;
            colPos = curr_line.length()-1;
            curr_line_offset = prev_line_offset;
        }
        if( debug || debug_retract ) {
            Debugger.trace("Ln " + lnNum + ", Col " + colPos + ": retract");
        }
        return;
    }
    
    static boolean test_retract() {
        Debugger.trace("test_retract");
        Reader reader = new StringReader("abc\ndef\nghi");
        try {
        InputBuffer inbuf = new InputBuffer(reader,System.err,"test");
        int c=-1;
        for(int i=0;i<9;i++) {
           c = inbuf.nextchar();
        }
        for(int i=0;i<3;i++) {
           inbuf.retract();
           c = inbuf.nextchar();
           inbuf.retract();
        }
        while(c!=0) {
           c = inbuf.nextchar();
        }
        } catch (Exception x) {
           x.printStackTrace();
           System.out.println(x);
        }
        return true;
    }

    /**
     * classOfNext
     */

    public byte classOfNext() {

        // return the Unicode character class of the current
        // character, which is pointed to by 'next-1'.

        return CharacterClasses.data[curr_line.charAt(colPos-1)];
    }

    /**
     * positionOfNext
     */

    public int positionOfNext() {
        return curr_line_offset+colPos-1;
    }

    /**
     * positionOfMark
     */

    public int positionOfMark() {
        return line_breaks[markLn]+markCol-1;
    }

    /**
     * mark
     */

    int markCol;
    int markLn;

    public int mark() {
        markLn  = (lnNum==-1)?0:lnNum; // In case nextchar hasn't been called yet.
        markCol = colPos;

        if( debug ) {
            Debugger.trace("Ln " + markLn + ", Col " + markCol + ": mark");
        }

        return markCol;
    }

    /**
     * copy
     */

    public String copy() throws Exception {
        StringBuffer buf = new StringBuffer();

        if( debug ) {
            Debugger.trace("Ln " + lnNum + ", Col " + colPos + ": copy " + buf);
        }

        if(markLn!=lnNum || markCol>colPos) {
            throw new Exception("Internal error: InputBuffer.copy() markLn = " + markLn + ", lnNum = " + lnNum + ", markCol = " +
                                       markCol + ", colPos = " + colPos );
        }

        for (int i = markCol-1; i < colPos; i++) {
            buf.append(curr_line.charAt(i));
        }

        return buf.toString();
    }

    static boolean test_markandcopy() {
        Debugger.trace("test_copy");
        Reader reader = new StringReader("abc\ndef\nghijklmnopqrst\nuvwxyz");
        String s;
        try {
        InputBuffer inbuf = new InputBuffer(reader,System.err,"test");
        int c=-1;
        int i;
        for(i=0;i<10;i++) {
           c = inbuf.nextchar();
        }
        inbuf.mark();
        for(;i<13;i++) {
           c = inbuf.nextchar();
        }
        s = inbuf.copy();
        if(s.equals("ijk")) {
            Debugger.trace("1: passed: " + s);
        } else {
            Debugger.trace("1: failed: " + s);
        }
        while(c!=0) {
           c = inbuf.nextchar();
        }
        s = inbuf.copy();
        } catch (Exception x) {
           s = x.getMessage();
           //x.printStackTrace();
        }
        if(s.equals("Internal error: InputBuffer.copy() markLn = 2, lnNum = 3, markCol = 2, colPos = 6")) {
            Debugger.trace("2: passed: " + s);
        } else {
            Debugger.trace("2: failed: " + s);
        }
        return true;
    }

    public String getLineText(int pos) {
        int i,len;
        for(i = 0; line_breaks[i] <= pos && i <= lnNum; i++)
            ;
        
        int offset = line_breaks[i-1];
        
        for(len = offset ; (text.charAt(len)!=(char)-1 &&
                       text.charAt(len)!=0x0a &&
                       text.charAt(len)!=0x0d &&
                       text.charAt(len)!=0x2028 &&
                       text.charAt(len)!=0x2029) ; len++) {
        }

        return text.toString().substring(offset,len);
    }

    static boolean test_getLineText() {
        Debugger.trace("test_getLineText");
        Reader reader = new StringReader("abc\ndef\nghi");
        try {
        InputBuffer inbuf = new InputBuffer(reader,System.err,"test");
        int c=-1;
        for(int i=0;i<9;i++) {
           c = inbuf.nextchar();
        }
        for(int i=0;i<3;i++) {
           inbuf.retract();
           c = inbuf.nextchar();
           inbuf.retract();
        }
        while(c!=0) {
           c = inbuf.nextchar();
        }
        for(int i=0;i<inbuf.text.length()-1;i++) {
            Debugger.trace("text @ " + i + " " + inbuf.getLineText(i));
        }
        } catch (Exception x) {
           x.printStackTrace();
           System.out.println(x);
        }
        return true;
    }

    public int getColPos(int pos) {
        //Debugger.trace("pos " + pos);
        int i,len;
        for(i = 0; line_breaks[i] <= pos && i <= lnNum; i++)
            ;
        
        int offset = line_breaks[i-1];
        //Debugger.trace("offset " + offset);
        
        return pos-offset;
    }

    public int getLnNum(int pos) {
        int i;
        for(i = 0; line_breaks[i] <= pos && i <= lnNum; i++)
            ;
        return i-1;
    }
}

/*
 * The end.
 */
