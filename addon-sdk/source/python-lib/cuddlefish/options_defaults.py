# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

def parse_options_defaults(options, jetpack_id):
    # this returns a unicode string
    pref_list = []

    for pref in options:
        if ('value' in pref):
            value = pref["value"]

            if isinstance(value, float):
                continue
            elif isinstance(value, bool):
                value = str(pref["value"]).lower()
            elif isinstance(value, str): # presumably ASCII
                value = "\"" + unicode(pref["value"]) + "\""
            elif isinstance(value, unicode):
                value = "\"" + pref["value"] + "\""
            else:
                value = str(pref["value"])

            pref_list.append("pref(\"extensions." + jetpack_id + "." + pref["name"] + "\", " + value + ");")

    return "\n".join(pref_list) + "\n"
