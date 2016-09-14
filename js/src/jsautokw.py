# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import sys

def read_keyword_list(filename):
    macro_pat = re.compile(r"^\s*macro\(([^,]+), *[^,]+, *[^\)]+\)\s*\\?$")

    keyword_list = []
    index = 0
    with open(filename, 'r') as f:
        for line in f:
            m = macro_pat.search(line)
            if m:
                keyword_list.append((index, m.group(1)))
                index += 1

    assert(len(keyword_list) != 0)

    return keyword_list

def line(opt, s):
    opt['output'].write('{}{}\n'.format('    ' * opt['indent_level'], s))

def indent(opt):
    opt['indent_level'] += 1

def dedent(opt):
    opt['indent_level'] -= 1

def span_and_count_at(keyword_list, column):
    assert(len(keyword_list) != 0);

    chars_dict = {}
    for index, keyword in keyword_list:
        chars_dict[ord(keyword[column])] = True

    chars = sorted(chars_dict.keys())
    return chars[-1] - chars[0] + 1, len(chars)

def optimal_switch_column(opt, keyword_list, columns, unprocessed_columns):
    assert(len(keyword_list) != 0);
    assert(unprocessed_columns != 0);

    min_count = 0
    min_span = 0
    min_count_index = 0
    min_span_index = 0

    for index in range(0, unprocessed_columns):
        span, count = span_and_count_at(keyword_list, columns[index])
        assert(span != 0)

        if span == 1:
            assert(count == 1)
            return 1, True

        assert(count != 1)
        if index == 0 or min_span > span:
            min_span = span
            min_span_index = index

        if index == 0 or min_count > count:
            min_count = count
            min_count_index = index

    if min_count <= opt['use_if_threshold']:
        return min_count_index, True

    return min_span_index, False

def split_list_per_column(keyword_list, column):
    assert(len(keyword_list) != 0);

    column_dict = {}
    for item in keyword_list:
        index, keyword = item
        per_column = column_dict.setdefault(keyword[column], [])
        per_column.append(item)

    return sorted(column_dict.items(), key=lambda (char, keyword): ord(char))

def generate_letter_switch(opt, unprocessed_columns, keyword_list,
                           columns=None):
    assert(len(keyword_list) != 0);

    if not columns:
        columns = range(0, unprocessed_columns)

    if len(keyword_list) == 1:
        index, keyword = keyword_list[0]

        if unprocessed_columns == 0:
            line(opt, 'JSKW_GOT_MATCH({}) /* {} */'.format(index, keyword))
            return

        if unprocessed_columns > opt['char_tail_test_threshold']:
            line(opt, 'JSKW_TEST_GUESS({}) /* {} */'.format(index, keyword))
            return

        conds = []
        for column in columns[0:unprocessed_columns]:
            quoted = repr(keyword[column])
            conds.append('JSKW_AT({})=={}'.format(column, quoted))

        line(opt, 'if ({}) {{'.format(' && '.join(conds)))

        indent(opt)
        line(opt, 'JSKW_GOT_MATCH({}) /* {} */'.format(index, keyword))
        dedent(opt)

        line(opt, '}')
        line(opt, 'JSKW_NO_MATCH()')
        return

    assert(unprocessed_columns != 0);

    optimal_column_index, use_if = optimal_switch_column(opt, keyword_list,
                                                         columns,
                                                         unprocessed_columns)
    optimal_column = columns[optimal_column_index]

    # Make a copy to avoid breaking passed list.
    columns = columns[:]
    columns[optimal_column_index] = columns[unprocessed_columns - 1]

    list_per_column = split_list_per_column(keyword_list, optimal_column)

    if not use_if:
        line(opt, 'switch (JSKW_AT({})) {{'.format(optimal_column))

    for char, keyword_list_per_column in list_per_column:
        quoted = repr(char)
        if use_if:
            line(opt, 'if (JSKW_AT({}) == {}) {{'.format(optimal_column,
                                                         quoted))
        else:
            line(opt, '  case {}:'.format(quoted))

        indent(opt)
        generate_letter_switch(opt, unprocessed_columns - 1,
                               keyword_list_per_column, columns)
        dedent(opt)

        if use_if:
            line(opt, '}')

    if not use_if:
        line(opt, '}')

    line(opt, 'JSKW_NO_MATCH()')

def split_list_per_length(keyword_list):
    assert(len(keyword_list) != 0);

    length_dict = {}
    for item in keyword_list:
        index, keyword = item
        per_length = length_dict.setdefault(len(keyword), [])
        per_length.append(item)

    return sorted(length_dict.items(), key=lambda (length, keyword): length)

def generate_switch(opt, keyword_list):
    assert(len(keyword_list) != 0);

    line(opt, '/*')
    line(opt, ' * Generating switch for the list of {} entries:'.format(len(keyword_list)))
    for index, keyword in keyword_list:
        line(opt, ' * {}'.format(keyword))
    line(opt, ' */')

    list_per_length = split_list_per_length(keyword_list)

    use_if = False
    if len(list_per_length) < opt['use_if_threshold']:
        use_if = True

    if not use_if:
        line(opt, 'switch (JSKW_LENGTH()) {')

    for length, keyword_list_per_length in list_per_length:
        if use_if:
            line(opt, 'if (JSKW_LENGTH() == {}) {{'.format(length))
        else:
            line(opt, '  case {}:'.format(length))

        indent(opt)
        generate_letter_switch(opt, length, keyword_list_per_length)
        dedent(opt)

        if use_if:
            line(opt, '}')

    if not use_if:
        line(opt, '}')
    line(opt, 'JSKW_NO_MATCH()')

def main(output, keywords_h):
    keyword_list = read_keyword_list(keywords_h)

    opt = {
        'indent_level': 1,
        'use_if_threshold': 3,
        'char_tail_test_threshold': 4,
        'output': output
    }
    generate_switch(opt, keyword_list)

if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
