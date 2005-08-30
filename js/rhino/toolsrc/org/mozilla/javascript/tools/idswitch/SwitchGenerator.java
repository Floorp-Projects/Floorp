/* -*- Mode: java; tab-width: 4; indent-tabs-mode: 1; c-basic-offset: 4 -*-
 *
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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Bukanov
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
package org.mozilla.javascript.tools.idswitch;

import org.mozilla.javascript.EvaluatorException;
import org.mozilla.javascript.tools.ToolErrorReporter;

public class SwitchGenerator {

    String v_switch_label = "L0";
    String v_label = "L";
    String v_s = "s";
    String v_c = "c";
    String v_guess = "X";
    String v_id = "id";
    String v_length_suffix = "_length";

    int use_if_threshold = 3;
    int char_tail_test_threshold = 2;

    private IdValuePair[] pairs;
    private String default_value;
    private int[] columns;
    private boolean c_was_defined;

    private CodePrinter P;
    private ToolErrorReporter R;
    private String source_file;

    public CodePrinter getCodePrinter() { return P; }
    public void setCodePrinter(CodePrinter value) { P = value; }

    public ToolErrorReporter getReporter() { return R; }
    public void setReporter(ToolErrorReporter value) { R = value; }

    public String getSourceFileName() { return source_file; }
    public void setSourceFileName(String value) { source_file = value; }

    public void generateSwitch(String[] pairs, String default_value) {
        int N = pairs.length / 2;
        IdValuePair[] id_pairs = new IdValuePair[N];
        for (int i = 0; i != N; ++i) {
            id_pairs[i] = new IdValuePair(pairs[2 * i], pairs[2 * i + 1]);
        }
        generateSwitch(id_pairs, default_value);

    }

    public void generateSwitch(IdValuePair[] pairs, String default_value) {
        int begin = 0;
        int end = pairs.length;
        if (begin == end) { return; }
        this.pairs = pairs;
        this.default_value = default_value;

        generate_body(begin, end, 2);
    }

    private void generate_body(int begin, int end, int indent_level) {
        P.indent(indent_level);
        P.p(v_switch_label); P.p(": { ");
        P.p(v_id); P.p(" = "); P.p(default_value);
        P.p("; String "); P.p(v_guess); P.p(" = null;");

        c_was_defined = false;
        int c_def_begin = P.getOffset();
        P.p(" int "); P.p(v_c); P.p(';');
        int c_def_end = P.getOffset();
        P.nl();

        generate_length_switch(begin, end, indent_level + 1);

        if (!c_was_defined) {
            P.erase(c_def_begin, c_def_end);
        }

        P.indent(indent_level + 1);
        P.p("if ("); P.p(v_guess); P.p("!=null && ");
        P.p(v_guess); P.p("!="); P.p(v_s);
        P.p(" && !"); P.p(v_guess); P.p(".equals("); P.p(v_s); P.p(")) ");
        P.p(v_id); P.p(" = "); P.p(default_value); P.p(";"); P.nl();
        P.line(indent_level, "}");
    }

    private void generate_length_switch(int begin, int end, int indent_level) {

        sort_pairs(begin, end, -1);

        check_all_is_different(begin, end);

        int lengths_count = count_different_lengths(begin, end);

        columns = new int[pairs[end  - 1].idLength];

        boolean use_if;
        if (lengths_count <= use_if_threshold) {
            use_if = true;
            if (lengths_count != 1) {
                P.indent(indent_level);
                P.p("int "); P.p(v_s); P.p(v_length_suffix);
                P.p(" = "); P.p(v_s); P.p(".length();");
                P.nl();
            }
        }
        else {
            use_if = false;
            P.indent(indent_level);
            P.p(v_label); P.p(": switch (");
            P.p(v_s); P.p(".length()) {");
            P.nl();
        }

        int same_length_begin = begin;
        int cur_l = pairs[begin].idLength, l = 0;
        for (int i = begin;;) {
            ++i;
            if (i == end || (l = pairs[i].idLength) != cur_l) {
                int next_indent;
                if (use_if) {
                    P.indent(indent_level);
                    if (same_length_begin != begin) { P.p("else "); }
                    P.p("if (");
                    if (lengths_count == 1) {
                        P.p(v_s); P.p(".length()==");
                    }
                    else {
                        P.p(v_s); P.p(v_length_suffix); P.p("==");
                    }
                    P.p(cur_l);
                    P.p(") {");
                    next_indent = indent_level + 1;
                }
                else {
                    P.indent(indent_level);
                    P.p("case "); P.p(cur_l); P.p(":");
                    next_indent = indent_level + 1;
                }
                generate_letter_switch
                    (same_length_begin, i, next_indent, !use_if, use_if);
                if (use_if) {
                    P.p("}"); P.nl();
                }
                else {
                    P.p("break "); P.p(v_label); P.p(";"); P.nl();
                }

                if (i == end) { break; }
                same_length_begin = i;
                cur_l = l;
            }
        }

        if (!use_if) {
            P.indent(indent_level); P.p("}"); P.nl();
        }

    }

    private void generate_letter_switch
        (int begin, int end,
         int indent_level, boolean label_was_defined, boolean inside_if)
    {
        int L = pairs[begin].idLength;

        for (int i = 0; i != L; ++i) {
            columns[i] = i;
        }

        generate_letter_switch_r
            (begin, end, L, indent_level, label_was_defined, inside_if);
    }


    private boolean generate_letter_switch_r
        (int begin, int end, int L,
         int indent_level, boolean label_was_defined, boolean inside_if)
    {
        boolean next_is_unreachable = false;
        if (begin + 1 == end) {
            P.p(' ');
            IdValuePair pair = pairs[begin];
            if (L > char_tail_test_threshold) {
                P.p(v_guess); P.p("="); P.qstring(pair.id); P.p(";");
                P.p(v_id); P.p("="); P.p(pair.value); P.p(";");
            }
            else {
                if (L == 0) {
                    next_is_unreachable = true;
                    P.p(v_id); P.p("="); P.p(pair.value);
                    P.p("; break "); P.p(v_switch_label); P.p(";");
                }
                else {
                    P.p("if (");
                    int column = columns[0];
                    P.p(v_s); P.p(".charAt("); P.p(column); P.p(")==");
                    P.qchar(pair.id.charAt(column));
                    for (int i = 1; i != L; ++i) {
                        P.p(" && ");
                        column = columns[i];
                        P.p(v_s); P.p(".charAt("); P.p(column); P.p(")==");
                        P.qchar(pair.id.charAt(column));
                    }
                    P.p(") {");
                    P.p(v_id); P.p("="); P.p(pair.value);
                    P.p("; break "); P.p(v_switch_label); P.p(";}");
                }
            }
            P.p(' ');
            return next_is_unreachable;
        }

        int max_column_index = find_max_different_column(begin, end, L);
        int max_column = columns[max_column_index];
        int count = count_different_chars(begin, end, max_column);

        columns[max_column_index] = columns[L - 1];

        if (inside_if) { P.nl(); P.indent(indent_level); }
        else { P.p(' '); }

        boolean use_if;
        if (count <= use_if_threshold) {
            use_if = true;
            c_was_defined = true;
            P.p(v_c); P.p("="); P.p(v_s);
            P.p(".charAt("); P.p(max_column); P.p(");");
        }
        else {
            use_if = false;
            if (!label_was_defined) {
                label_was_defined = true;
                P.p(v_label); P.p(": ");
            }
            P.p("switch ("); P.p(v_s);
            P.p(".charAt("); P.p(max_column); P.p(")) {");
        }

        int same_char_begin = begin;
        int cur_ch = pairs[begin].id.charAt(max_column), ch = 0;
        for (int i = begin;;) {
            ++i;
            if (i == end || (ch = pairs[i].id.charAt(max_column)) != cur_ch) {
                int next_indent;
                if (use_if) {
                    P.nl(); P.indent(indent_level);
                    if (same_char_begin != begin) { P.p("else "); }
                    P.p("if ("); P.p(v_c); P.p("==");
                    P.qchar(cur_ch); P.p(") {");
                    next_indent = indent_level + 1;
                }
                else {
                    P.nl(); P.indent(indent_level);
                    P.p("case "); P.qchar(cur_ch); P.p(":");
                    next_indent = indent_level + 1;
                }
                boolean after_unreachable = generate_letter_switch_r
                    (same_char_begin, i, L - 1,
                     next_indent, label_was_defined, use_if);
                if (use_if) {
                    P.p("}");
                }
                else {
                    if (!after_unreachable) {
                        P.p("break "); P.p(v_label); P.p(";");
                    }
                }
                if (i == end) { break; }
                same_char_begin = i;
                cur_ch = ch;
            }
        }

        if (use_if) {
            P.nl();
            if (inside_if) { P.indent(indent_level - 1); }
            else { P.indent(indent_level); }
        }
        else {
            P.nl(); P.indent(indent_level); P.p("}");
            if (inside_if) { P.nl(); P.indent(indent_level - 1);}
            else { P.p(' '); }
        }

        columns[max_column_index] = max_column;

        return next_is_unreachable;
    }


    private int count_different_lengths(int begin, int end) {
        int lengths_count = 0;
        int cur_l = -1;
        for (; begin != end; ++begin) {
            int l = pairs[begin].idLength;
            if (cur_l != l) {
                ++lengths_count; cur_l = l;
            }
        }
        return lengths_count;
    }

    private int find_max_different_column(int begin, int end, int L) {
        int max_count = 0;
        int max_index = 0;

        for (int i = 0; i != L; ++i) {
            int column = columns[i];
            sort_pairs(begin, end, column);
            int count = count_different_chars(begin, end, column);
            if (count == end - begin) { return i; }
            if (max_count < count) {
                max_count = count;
                max_index = i;
            }
        }

        if (max_index != L - 1) {
            sort_pairs(begin, end, columns[max_index]);
        }

        return max_index;
    }

    private int count_different_chars(int begin, int end, int column) {
        int chars_count = 0;
        int cur_ch = -1;
        for (; begin != end; ++begin) {
            int ch = pairs[begin].id.charAt(column);
            if (ch != cur_ch) {
                ++chars_count; cur_ch = ch;
            }
        }
        return chars_count;
    }

    private void check_all_is_different(int begin, int end) {
        if (begin != end) {
            IdValuePair prev = pairs[begin];
            while (++begin != end) {
                IdValuePair current = pairs[begin];
                if (prev.id.equals(current.id)) {
                    throw on_same_pair_fail(prev, current);
                }
                prev = current;
            }
        }
    }

    private EvaluatorException on_same_pair_fail(IdValuePair a, IdValuePair b) {
        StringBuffer sb = new StringBuffer();
        int line1 = a.getLineNumber(), line2 = b.getLineNumber();
        if (line2 > line1) { int tmp = line1; line1 = line2; line2 = tmp; }
        String error_text = ToolErrorReporter.getMessage(
			"msg.idswitch.same_string", a.id, new Integer(line2));
        return R.runtimeError(error_text, source_file, line1, null, 0);
    }

    private void sort_pairs(int begin, int end, int comparator) {
        heap4Sort(pairs, begin, end - begin, comparator);
    }

    private static boolean bigger
        (IdValuePair a, IdValuePair b, int comparator)
    {
        if (comparator < 0) {
        // For length selection switch it is enough to compare just length,
        // but to detect same strings full comparison is essential
            //return a.idLength > b.idLength;
            int diff = a.idLength - b.idLength;
            if (diff != 0) { return diff > 0; }
            return a.id.compareTo(b.id) > 0;
        }
        else {
            return a.id.charAt(comparator) > b.id.charAt(comparator);
        }
    }

    private static void heap4Sort
        (IdValuePair[] array, int offset, int size, int comparator)
    {
        if (size <= 1) { return; }
        makeHeap4(array, offset, size, comparator);
        while (size > 1) {
            --size;
            IdValuePair v1 = array[offset + size];
            IdValuePair v2 = array[offset + 0];
            array[offset + size] = v2;
            array[offset + 0] = v1;
            heapify4(array, offset, size, 0, comparator);
        }
    }

    private static void makeHeap4
        (IdValuePair[] array, int offset, int size, int comparator)
    {
        for (int i = ((size + 2) >> 2); i != 0;) {
            --i;
            heapify4(array, offset, size, i, comparator);
        }
    }

    private static void heapify4
        (IdValuePair[] array, int offset, int size, int i, int comparator)
    {
        int new_i1, new_i2, new_i3;
        IdValuePair i_val = array[offset + i];
        for (;;) {
            int base = (i << 2);
            new_i1 = base | 1;
            new_i2 = base | 2;
            new_i3 = base | 3;
            int new_i4 = base + 4;
            if (new_i4 >= size) { break; }
            IdValuePair val1 = array[offset + new_i1];
            IdValuePair val2 = array[offset + new_i2];
            IdValuePair val3 = array[offset + new_i3];
            IdValuePair val4 = array[offset + new_i4];
            if (bigger(val2, val1, comparator)) {
                val1 = val2; new_i1 = new_i2;
            }
            if (bigger(val4, val3, comparator)) {
                val3 = val4; new_i3 = new_i4;
            }
            if (bigger(val3, val1, comparator)) {
                val1 = val3; new_i1 = new_i3;
            }
            if (bigger(i_val, val1, comparator)) { return; }
            array[offset + i] = val1;
            array[offset + new_i1] = i_val;
            i = new_i1;
        }
        if (new_i1 < size) {
            IdValuePair val1 = array[offset + new_i1];
            if (new_i2 != size) {
                IdValuePair val2 = array[offset + new_i2];
                if (bigger(val2, val1, comparator)) {
                    val1 = val2; new_i1 = new_i2;
                }
                if (new_i3 != size) {
                    IdValuePair val3 = array[offset + new_i3];
                    if (bigger(val3, val1, comparator)) {
                        val1 = val3; new_i1 = new_i3;
                    }
                }
            }
            if (bigger(val1, i_val, comparator)) {
                array[offset + i] = val1;
                array[offset + new_i1] = i_val;
            }
        }
    }
}

