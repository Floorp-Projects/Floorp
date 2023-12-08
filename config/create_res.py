# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import sys
import tempfile
from argparse import Action, ArgumentParser

import buildconfig


class CPPFlag(Action):
    all_flags = []

    def __call__(self, parser, namespace, values, option_string=None):
        if "windres" in buildconfig.substs["RC"].lower():
            if option_string == "-U":
                return
            if option_string == "-I":
                option_string = "--include-dir"

        self.all_flags.extend((option_string, values))


def generate_res():
    parser = ArgumentParser()
    parser.add_argument(
        "-D", action=CPPFlag, metavar="VAR[=VAL]", help="Define a variable"
    )
    parser.add_argument("-U", action=CPPFlag, metavar="VAR", help="Undefine a variable")
    parser.add_argument(
        "-I", action=CPPFlag, metavar="DIR", help="Search path for includes"
    )
    parser.add_argument("-o", dest="output", metavar="OUTPUT", help="Output file")
    parser.add_argument("input", help="Input file")
    args = parser.parse_args()

    is_windres = "windres" in buildconfig.substs["RC"].lower()

    verbose = os.environ.get("BUILD_VERBOSE_LOG")

    # llvm-rc doesn't preprocess on its own, so preprocess manually
    # Theoretically, not windres could be rc.exe, but configure won't use it
    # unless you really ask for it, and it will still work with preprocessed
    # output.
    try:
        if not is_windres:
            fd, path = tempfile.mkstemp(suffix=".rc")
            command = buildconfig.substs["CXXCPP"] + CPPFlag.all_flags
            command.extend(("-DRC_INVOKED", args.input))

            cpu_arch_dict = {"x86_64": "_AMD64_", "x86": "_X86_", "aarch64": "_ARM64_"}

            # add a preprocessor #define that specifies the CPU architecture
            cpu_arch_ppd = cpu_arch_dict[buildconfig.substs["TARGET_CPU"]]

            command.extend(("-D", cpu_arch_ppd))

            if verbose:
                print("Executing:", " ".join(command))
            with os.fdopen(fd, "wb") as fh:
                retcode = subprocess.run(command, stdout=fh).returncode
                if retcode:
                    # Rely on the subprocess printing out any relevant error
                    return retcode
        else:
            path = args.input

        command = [buildconfig.substs["RC"]]
        if is_windres:
            command.extend(("-O", "coff"))

        # Even though llvm-rc doesn't preprocess, we still need to pass at least
        # the -I flags.
        command.extend(CPPFlag.all_flags)

        if args.output:
            if is_windres:
                command.extend(("-o", args.output))
            else:
                # Use win1252 code page for the input.
                command.extend(("-c", "1252", "-Fo" + args.output))

        command.append(path)

        if verbose:
            print("Executing:", " ".join(command))
        retcode = subprocess.run(command).returncode
        if retcode:
            # Rely on the subprocess printing out any relevant error
            return retcode
    finally:
        if path != args.input:
            os.remove(path)

    return 0


if __name__ == "__main__":
    sys.exit(generate_res())
