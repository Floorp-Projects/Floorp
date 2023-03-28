# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def main(output, intgemm_config):
    with open(intgemm_config, "r") as f:
        config = f.read()

    # Enable intel AVX2 hardware extension specific code to allow using AVX2 at run time
    # if target cpu supports it
    config = config.replace(
        "#cmakedefine INTGEMM_COMPILER_SUPPORTS_AVX2",
        "#define INTGEMM_COMPILER_SUPPORTS_AVX2",
    )

    # Disable more advanced intel hardware extensions for now because base-toolchain compiler
    # versions aren't able to compile them
    config = config.replace("#cmakedefine", "#undef")

    output.write(config)
    output.close()

    return 0
