# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import re
import sys

def read_reserved_word_list(filename):
    macro_pat = re.compile(r"^\s*macro\(([^,]+), *[^,]+, *[^\)]+\)\s*\\?$")

    reserved_word_list = []
    with open(filename, 'r') as f:
        for line in f:
            m = macro_pat.search(line)
            if m:
                reserved_word_list.append(m.group(1))

    assert(len(reserved_word_list) != 0)

    return reserved_word_list


def line(opt, s):
    opt['output'].write('{}\n'.format(s))


def main(output, reserved_words_h):
    reserved_word_list = read_reserved_word_list(reserved_words_h)
    opt = {
        'output': output
    }

    line(opt, 'const JS_RESERVED_WORDS = [');
    for word in reserved_word_list:
        line(opt, '  "{}",'.format(word))
    line(opt, '];');
    line(opt, 'module.exports = JS_RESERVED_WORDS;');


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
