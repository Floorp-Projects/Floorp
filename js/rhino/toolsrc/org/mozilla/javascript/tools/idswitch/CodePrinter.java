/* -*- Mode: java; tab-width: 4; indent-tabs-mode: 1; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above. If
 * you wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replacing
 * them with the notice and other provisions required by the GPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */
package org.mozilla.javascript.tools.idswitch;

class CodePrinter {

// length of u-type escape like \u12AB
    private static final int LITERAL_CHAR_MAX_SIZE = 6;

    private String lineTerminator = "\n";

    private int indentStep = 4;
    private int indentTabSize = 8;

    private char[] buffer = new char[1 << 12]; // 4K
    private int offset;

    public String getLineTerminator() { return lineTerminator; }
    public void setLineTerminator(String value) { lineTerminator = value; }

    public int getIndentStep() { return indentStep; }
    public void setIndentStep(int char_count) { indentStep = char_count; }

    public int getIndentTabSize() {    return indentTabSize; }
    public void setIndentTabSize(int tab_size) { indentTabSize = tab_size; }

    public void clear() {
        offset = 0;
    }

    private int ensure_area(int area_size) {
        int begin = offset;
        int end = begin + area_size;
        if (end > buffer.length) {
            int new_capacity = buffer.length * 2;
            if (end > new_capacity) { new_capacity = end; }
            char[] tmp = new char[new_capacity];
            System.arraycopy(buffer, 0, tmp, 0, begin);
            buffer = tmp;
        }
        return begin;
    }

    private int add_area(int area_size) {
        int pos = ensure_area(area_size);
        offset = pos + area_size;
        return pos;
    }

    public int getOffset() {
        return offset;
    }

    public int getLastChar() {
        return offset == 0 ? -1 : buffer[offset - 1];
    }

    public void p(char c) {
        int pos = add_area(1);
        buffer[pos] = c;
    }

    public void p(String s) {
        int l = s.length();
        int pos = add_area(l);
        s.getChars(0, l, buffer, pos);
    }

    public final void p(char[] array) {
        p(array, 0, array.length);
    }

    public void p(char[] array, int begin, int end) {
        int l = end - begin;
        int pos = add_area(l);
        System.arraycopy(array, begin, buffer, pos, l);
    }

    public void p(int i) {
        p(Integer.toString(i));
    }

    public void qchar(int c) {
        int pos = ensure_area(2 + LITERAL_CHAR_MAX_SIZE);
        buffer[pos] = '\'';
        pos = put_string_literal_char(pos + 1, c, false);
        buffer[pos] = '\'';
        offset = pos + 1;
    }

    public void qstring(String s) {
        int l = s.length();
        int pos = ensure_area(2 + LITERAL_CHAR_MAX_SIZE * l);
        buffer[pos] = '"';
        ++pos;
        for (int i = 0; i != l; ++i) {
            pos = put_string_literal_char(pos, s.charAt(i), true);
        }
        buffer[pos] = '"';
        offset = pos + 1;
    }

    private int put_string_literal_char(int pos, int c, boolean in_string) {
        boolean backslash_symbol = true;
        switch (c) {
            case '\b': c = 'b'; break;
            case '\t': c = 't'; break;
            case '\n': c = 'n'; break;
            case '\f': c = 'f'; break;
            case '\r': c = 'r'; break;
            case '\'': backslash_symbol = !in_string; break;
            case '"': backslash_symbol = in_string; break;
            default: backslash_symbol = false;
        }

        if (backslash_symbol) {
            buffer[pos] = '\\';
            buffer[pos + 1] = (char)c;
            pos += 2;
        }
        else if (' ' <= c && c <= 126) {
            buffer[pos] = (char)c;
            ++pos;
        }
        else {
            buffer[pos] = '\\';
            buffer[pos + 1] = 'u';
            buffer[pos + 2] = digit_to_hex_letter(0xF & (c >> 12));
            buffer[pos + 3] = digit_to_hex_letter(0xF & (c >> 8));
            buffer[pos + 4] = digit_to_hex_letter(0xF & (c >> 4));
            buffer[pos + 5] = digit_to_hex_letter(0xF & c);
            pos += 6;
        }
        return pos;
    }

    private static char digit_to_hex_letter(int d) {
        return (char)((d < 10) ? '0' + d : 'A' - 10 + d);
    }

    public void indent(int level) {
        int visible_size = indentStep * level;
        int indent_size, tab_count;
        if (indentTabSize <= 0) {
            tab_count = 0; indent_size = visible_size;
        }
        else {
            tab_count = visible_size / indentTabSize;
            indent_size = tab_count + visible_size % indentTabSize;
        }
        int pos = add_area(indent_size);
        int tab_end = pos + tab_count;
        int indent_end = pos + indent_size;
        for (; pos != tab_end; ++pos) {    buffer[pos] = '\t'; }
        for (; pos != indent_end; ++pos) {    buffer[pos] = ' '; }
    }

    public void nl() {
        p('\n');
    }

    public void line(int indent_level, String s) {
        indent(indent_level); p(s); nl();
    }

    public void erase(int begin, int end) {
        System.arraycopy(buffer, end, buffer, begin, offset - end);
        offset -= end - begin;
    }

    public String toString() {
        return new String(buffer, 0, offset);
    }



}
