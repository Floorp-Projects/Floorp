# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import buildconfig


def generate_bool(name):
    value = buildconfig.substs.get(name)
    return f"pub const {name}: bool = {'true' if value else 'false'};\n"


def generate(output):
    output.write(generate_bool("MOZ_FOLD_LIBS"))
    output.write(generate_bool("NIGHTLY_BUILD"))
    output.write(generate_bool("RELEASE_OR_BETA"))
    output.write(generate_bool("EARLY_BETA_OR_EARLIER"))
    output.write(generate_bool("MOZ_DEV_EDITION"))
    output.write(generate_bool("MOZ_ESR"))
    output.write(generate_bool("MOZ_DIAGNOSTIC_ASSERT_ENABLED"))
