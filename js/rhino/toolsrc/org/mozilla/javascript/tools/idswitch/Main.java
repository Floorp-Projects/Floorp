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

import java.io.*;
import java.util.*;
import java.text.SimpleDateFormat;

import org.mozilla.javascript.EvaluatorException;
import org.mozilla.javascript.tools.ToolErrorReporter;

public class Main {

    private static final String PROGRAM_NAME = "Main";

    private static final String SWITCH_TAG_STR = "string_id_map";
    private static final String GENERATED_TAG_STR = "generated";
    private static final String STRING_TAG_STR = "string";

    private static final int
        NORMAL_LINE        = 0,
        SWITCH_TAG         = 1,
        GENERATED_TAG      = 2,
        STRING_TAG         = 3;

    private final Vector all_pairs = new Vector();

    private ToolErrorReporter R;
    private CodePrinter P;
    private FileBody body;
    private String source_file;

    private int tag_definition_end;

    private int tag_value_start;
    private int tag_value_end;

    private static boolean is_value_type(int id) {
        if (id == STRING_TAG) { return true; }
        return false;
    }

    private static String tag_name(int id) {
        switch (id) {
            case SWITCH_TAG: return SWITCH_TAG_STR;
            case -SWITCH_TAG: return "/" + SWITCH_TAG_STR;
            case GENERATED_TAG: return GENERATED_TAG_STR;
            case -GENERATED_TAG: return "/" + GENERATED_TAG_STR;
        }
        return "";
    }

    void process_file(String file_path) throws IOException {
        source_file = file_path;

        body = new FileBody();

        InputStream is;
        if (file_path.equals("-")) {
            is = System.in;
        }
        else {
            is = new FileInputStream(file_path);
        }
        try {
            Reader r = new InputStreamReader(is, "ASCII");
            body.readData(r);
        }
        finally { is.close(); }

        process_file();

        if (body.wasModified()) {
            OutputStream os;
            if (file_path.equals("-")) {
                os = System.out;
            }
            else {
                os = new FileOutputStream(file_path);
            }

            try {
                Writer w = new OutputStreamWriter(os);
                body.writeData(w);
                w.flush();
            }
            finally { os.close(); }
        }
    }

    private void process_file() throws IOException {
        int cur_state = 0;
        char[] buffer = body.getBuffer();

        int generated_begin = -1, generated_end = -1;
        int time_stamp_begin = -1, time_stamp_end = -1;

        body.startLineLoop();
        L:while (body.nextLine()) {
            int begin = body.getLineBegin();
            int end = body.getLineEnd();

            int tag_id = extract_line_tag_id(buffer, begin, end);
            boolean bad_tag = false;
            switch (cur_state) {
                case 0:
                    if (tag_id == SWITCH_TAG) {
                        cur_state = SWITCH_TAG;
                        all_pairs.removeAllElements();
                        generated_begin = -1;
                    }
                    else if (tag_id == -SWITCH_TAG) {
                        bad_tag = true;
                    }
                    break;
                case SWITCH_TAG:
                    if (tag_id == 0) {
                        look_for_id_definitions(buffer, begin, end, false);
                    }
                    else if (tag_id == STRING_TAG) {
                        look_for_id_definitions(buffer, begin, end, true);
                    }
                    else if (tag_id == GENERATED_TAG) {
                        if (generated_begin >= 0) { bad_tag = true; }
                        else {
                            cur_state = GENERATED_TAG;
                            time_stamp_begin = tag_definition_end;
                            time_stamp_end = end;
                        }
                    }
                    else if (tag_id == -SWITCH_TAG) {
                        cur_state = 0;
                        if (generated_begin >= 0 && !all_pairs.isEmpty()) {
                            generate_java_code();
                            String code = P.toString();
                            boolean different = body.setReplacement
                                (generated_begin, generated_end, code);
                            if (different) {
                                String stamp = get_time_stamp();
                                body.setReplacement
                                    (time_stamp_begin, time_stamp_end, stamp);
                            }
                        }

                        break;
                    }
                    else {
                        bad_tag = true;
                    }
                    break;
                case GENERATED_TAG:
                    if (tag_id == 0) {
                        if (generated_begin < 0) { generated_begin = begin; }
                    }
                    else if (tag_id == -GENERATED_TAG) {
                        if (generated_begin < 0) { generated_begin = begin; }
                        cur_state = SWITCH_TAG;
                        generated_end = begin;
                    }
                    else {
                        bad_tag = true;
                    }
                    break;
            }
            if (bad_tag) {
                String text = R.getMessage("msg.idswitch.bad_tag_order",
                                           tag_name(tag_id));
                throw R.runtimeError
                    (text, source_file, body.getLineNumber(), null, 0);
            }
        }

        if (cur_state != 0) {
            String text = R.getMessage("msg.idswitch.file_end_in_switch",
                                       tag_name(cur_state));
            throw R.runtimeError
                (text, source_file, body.getLineNumber(), null, 0);
        }
    }

    private String get_time_stamp() {
        SimpleDateFormat f = new SimpleDateFormat
            (" 'Last update:' yyyy-MM-dd HH:mm:ss z");
        String dateString = f.format(new Date());
        return f.format(new Date());
    }

    private void generate_java_code() {

        P.clear();

        IdValuePair[] pairs = new IdValuePair[all_pairs.size()];
        all_pairs.copyInto(pairs);

        SwitchGenerator g = new SwitchGenerator();
        g.char_tail_test_threshold = 2;
        g.setReporter(R);
        g.setCodePrinter(P);

        g.generateSwitch(pairs, "0");
    }

    private int extract_line_tag_id(char[] array, int cursor, int end) {
        int id = 0;
        cursor = skip_white_space(array, cursor, end);
        int after_leading_white_space = cursor;
        cursor = look_for_slash_slash(array, cursor, end);
        if (cursor != end) {
            boolean at_line_start = (after_leading_white_space + 2 == cursor);
            cursor = skip_white_space(array, cursor, end);
            if (cursor != end && array[cursor] == '#') {
                ++cursor;

                boolean end_tag = false;
                if (cursor != end && array[cursor] == '/') {
                    ++cursor; end_tag = true;
                }

                int tag_start = cursor;

                for (; cursor != end; ++cursor) {
                    int c = array[cursor];
                    if (c == '#' || c == '=' ||is_white_space(c)) { break; }
                }

                if (cursor != end) {
                    int tag_end = cursor;
                    cursor = skip_white_space(array, cursor, end);
                    if (cursor != end) {
                        int c = array[cursor];
                        if (c == '=' || c == '#') {
                            id = get_tag_id
                                (array, tag_start, tag_end, at_line_start);
                            if (id != 0) {
                                String bad = null;
                                if (c == '#') {
                                    if (end_tag) {
                                        id = -id;
                                        if (is_value_type(id)) {
                                            bad = "msg.idswitch.no_end_usage";
                                        }
                                    }
                                    tag_definition_end = cursor + 1;
                                }
                                else  {
                                    if (end_tag) {
                                        bad = "msg.idswitch.no_end_with_value";
                                    }
                                    else if (!is_value_type(id)) {
                                        bad = "msg.idswitch.no_value_allowed";
                                    }
                                    id = extract_tag_value
                                        (array, cursor + 1, end, id);
                                }
                                if (bad != null) {
                                    String s = R.getMessage(bad, tag_name(id));
                                    throw R.runtimeError
                                        (s, source_file, body.getLineNumber(),
                                         null, 0);
                                }
                            }
                        }
                    }
                }
            }
        }
        return id;
    }

// Return position after first of // or end if not found
    private int look_for_slash_slash(char[] array, int cursor, int end) {
        while (cursor + 2 <= end) {
            int c = array[cursor++];
            if (c == '/') {
                c = array[cursor++];
                if (c == '/') {
                    return cursor;
                }
            }
        }
        return end;
    }

    private int extract_tag_value(char[] array, int cursor, int end, int id) {
        // cursor points after #[^#=]+=
        // ALERT: implement support for quoted strings
        boolean found = false;
        cursor = skip_white_space(array, cursor, end);
        if (cursor != end) {
            int value_start = cursor;
            int value_end = cursor;
            while (cursor != end) {
                int c = array[cursor];
                if (is_white_space(c)) {
                    int after_space = skip_white_space(array, cursor + 1, end);
                    if (after_space != end && array[after_space] == '#') {
                        value_end = cursor;
                        cursor = after_space;
                        break;
                    }
                    cursor = after_space + 1;
                }
                else if (c == '#') {
                    value_end = cursor;
                    break;
                }
                else {
                    ++cursor;
                }
            }
            if (cursor != end) {
                // array[cursor] is '#' here
                found = true;
                tag_value_start = value_start;
                tag_value_end = value_end;
                tag_definition_end = cursor + 1;
            }
        }
        return (found) ? id : 0;
    }

    private int get_tag_id
        (char[] array, int begin, int end, boolean at_line_start)
    {
        if (at_line_start) {
            if (equals(SWITCH_TAG_STR, array, begin, end)) {
                return SWITCH_TAG;
            }
            if (equals(GENERATED_TAG_STR, array, begin, end)) {
                return GENERATED_TAG;
            }
        }
        if (equals(STRING_TAG_STR, array, begin, end)) {
            return STRING_TAG;
        }
        return 0;
    }

    private void look_for_id_definitions
        (char[] array, int begin, int end, boolean use_tag_value_as_string)
    {
    // Look for the pattern
    // '^[ \t]+Id_([a-zA-Z0-9_]+)[ \t]*=.*$'
    // where \1 gives field or method name
        int cursor = begin;
        // Skip tab and spaces at the beginning
        cursor = skip_white_space(array, cursor, end);
        int id_start = cursor;
        int name_start = skip_matched_prefix("Id_", array, cursor, end);
        if (name_start >= 0) {
            // Found Id_ prefix
            cursor = name_start;
            cursor = skip_name_char(array, cursor, end);
            int name_end = cursor;
            if (name_start != name_end) {
                cursor = skip_white_space(array, cursor, end);
                if (cursor != end) {
                    if (array[cursor] == '=') {
                        int id_end = name_end;
                        if (use_tag_value_as_string) {
                            name_start = tag_value_start;
                            name_end = tag_value_end;
                        }
                        // Got the match
                        add_id(array, id_start, id_end, name_start, name_end);
                    }
                }
            }
        }
    }

    private void add_id
        (char[] array, int id_start, int id_end, int name_start, int name_end)
    {
        String name = new String(array, name_start, name_end - name_start);
        String value = new String(array, id_start, id_end - id_start);

        IdValuePair pair = new IdValuePair(name, value);

        pair.setLineNumber(body.getLineNumber());

        all_pairs.addElement(pair);
    }

    private static boolean is_white_space(int c) {
        return c == ' ' || c == '\t';
    }

    private static int skip_white_space(char[] array, int begin, int end) {
        int cursor = begin;
        for (; cursor != end; ++cursor) {
            int c = array[cursor];
            if (!is_white_space(c)) { break; }
        }
        return cursor;
    }

    private static int skip_matched_prefix
        (String prefix, char[] array, int begin, int end)
    {
        int cursor = -1;
        int prefix_length = prefix.length();
        if (prefix_length <= end - begin) {
            cursor = begin;
            for (int i = 0; i != prefix_length; ++i, ++cursor) {
                if (prefix.charAt(i) != array[cursor]) {
                    cursor = -1; break;
                }
            }
        }
        return cursor;
    }

    private static boolean equals(String str, char[] array, int begin, int end)
    {
        if (str.length() == end - begin) {
            for (int i = begin, j = 0; i != end; ++i, ++j) {
                if (array[i] != str.charAt(j)) { return false; }
            }
            return true;
        }
        return false;
    }

    private static int skip_name_char(char[] array, int begin, int end) {
        int cursor = begin;
        for (; cursor != end; ++cursor) {
            int c = array[cursor];
            if (!('a' <= c && c <= 'z') && !('A' <= c && c <= 'Z')) {
                if (!('0' <= c && c <= '9')) {
                    if (c != '_') {
                        break;
                    }
                }
            }
        }
        return cursor;
    }

    public static void main(String[] args) {
        Main self = new Main();
        int status = self.exec(args);
        System.exit(status);
    }

    private int exec(String[] args) {
        R = new ToolErrorReporter(true, System.err);

        int arg_count = process_options(args);

        if (arg_count == 0) {
            option_error(R.getMessage
                ("msg.idswitch.no_file_argument"));
            return -1;
        }
        if (arg_count > 1) {
            option_error(R.getMessage
                ("msg.idswitch.too_many_arguments"));
            return -1;
        }

        P = new CodePrinter();
        P.setIndentStep(4);
        P.setIndentTabSize(0);

        try {
            process_file(args[0]);
        }
        catch (IOException ex) {
            print_error(R.getMessage
                ("msg.idswitch.io_error", ex.toString()));
            return -1;
        }
        catch (EvaluatorException ex) {
            return -1;
        }
        return 0;
    }

    private int process_options(String[] args) {

        int status = 1;

        boolean show_usage = false;
        boolean show_version = false;

        int N = args.length;
        L: for (int i = 0; i != N; ++i) {
            String arg = args[i];
            int arg_length = arg.length();
            if (arg_length >= 2) {
                if (arg.charAt(0) == '-') {
                    if (arg.charAt(1) == '-') {
                        if (arg_length == 2) {
                            args[i] = null; break;
                        }
                        if (arg.equals("--help")) {
                            show_usage = true;
                        }
                        else if (arg.equals("--version")) {
                            show_version = true;
                        }
                        else {
                            option_error(R.getMessage
                                ("msg.idswitch.bad_option", arg));
                            status = -1; break L;
                        }
                    }
                    else {
                        for (int j = 1; j != arg_length; ++j) {
                            char c = arg.charAt(j);
                            switch (c) {
                                case 'h': show_usage = true; break;
                                default:
                                    option_error(R.getMessage
                                        ("msg.idswitch.bad_option_char",
                                         "" + c));
                                    status = -1;
                                    break L;
                            }

                        }
                    }
                    args[i] = null;
                }
            }
        }

        if (status == 1) {
            if (show_usage) { show_usage(); status = 0; }
            if (show_version) { show_version(); status = 0; }
        }

        if (status != 1) { System.exit(status); }

        return remove_nulls(args);
    }

    private void show_usage() {
        System.out.println(R.getMessage("msg.idswitch.usage"));
        System.out.println();
    }

    private void show_version() {
        System.out.println(R.getMessage("msg.idswitch.version"));
    }

    private void option_error(String str) {
        print_error(R.getMessage("msg.idswitch.bad_invocation", str));
    }

    private void print_error(String text) {
        System.err.println(text);
    }

    private int remove_nulls(String[] array) {
        int N = array.length;
        int cursor = 0;
        for (; cursor != N; ++cursor) {
            if (array[cursor] == null) { break; }
        }
        int destination = cursor;
        if (cursor != N) {
            ++cursor;
            for (; cursor != N; ++cursor) {
                String elem = array[cursor];
                if (elem != null) {
                    array[destination] = elem; ++destination;
                }
            }
        }
        return destination;
    }



}

