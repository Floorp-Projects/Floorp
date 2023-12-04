#!/usr/bin/env python3
# This file is part of ICU4X. For terms of use, please see the file
# called LICENSE at the top level of the ICU4X source tree
# (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

import sys
import subprocess

SYMBOLS = [
    "ICU4XDataProvider_create_compiled",
    "ICU4XDataProvider_destroy",
    "ICU4XFixedDecimal_create_from_i32",
    "ICU4XFixedDecimal_destroy",
    "ICU4XFixedDecimal_multiply_pow10",
    "ICU4XFixedDecimalFormatter_create_with_grouping_strategy",
    "ICU4XFixedDecimalFormatter_destroy",
    "ICU4XFixedDecimalFormatter_format",
    "ICU4XLocale_create_from_string",
    "ICU4XLocale_destroy",
]

def main():
    new_argv = []
    is_export = False
    for arg in sys.argv[1:]:
        if is_export:
            if not arg.startswith("ICU4X") or arg in SYMBOLS:
                new_argv += ["--export", arg]
            is_export = False
        elif arg == "--export":
            is_export = True
        else:
            new_argv += [arg]
            is_export = False
    result = subprocess.run(["lld-15"] + new_argv, stdout=sys.stdout, stderr=sys.stderr)
    return result.returncode

if __name__ == "__main__":
    sys.exit(main())
