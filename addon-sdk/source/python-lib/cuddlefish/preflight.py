# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os, sys
import base64
import simplejson as json

def create_jid():
    """Return 'jid1-XYZ', where 'XYZ' is a randomly-generated string. (in the
    previous jid0- series, the string securely identified a specific public
    key). To get a suitable add-on ID, append '@jetpack' to this string.
    """
    # per https://developer.mozilla.org/en/Install_Manifests#id all XPI id
    # values must either be in the form of a 128-bit GUID (crazy braces
    # and all) or in the form of an email address (crazy @ and all).
    # Firefox will refuse to install an add-on with an id that doesn't
    # match one of these forms. The actual regexp is at:
    # http://mxr.mozilla.org/mozilla-central/source/toolkit/mozapps/extensions/XPIProvider.jsm#130
    # So the JID needs an @-suffix, and the only legal punctuation is
    # "-._". So we start with a base64 encoding, and replace the
    # punctuation (+/) with letters (AB), losing a few bits of integrity.

    # even better: windows has a maximum path length limitation of 256
    # characters:
    #  http://msdn.microsoft.com/en-us/library/aa365247%28VS.85%29.aspx
    # (unless all paths are prefixed with "\\?\", I kid you not). The
    # typical install will put add-on code in a directory like:
    # C:\Documents and Settings\<username>\Application Data\Mozilla\Firefox\Profiles\232353483.default\extensions\$JID\...
    # (which is 108 chars long without the $JID).
    # Then the unpacked XPI contains packaged resources like:
    #  resources/$JID-api-utils-lib/main.js   (35 chars plus the $JID)
    #
    # We create a random 80 bit string, base64 encode that (with
    # AB instead of +/ to be path-safe), then bundle it into
    # "jid1-XYZ@jetpack". This gives us 27 characters. The resulting
    # main.js will have a path length of 211 characters, leaving us 45
    # characters of margin.
    #
    # 80 bits is enough to generate one billion JIDs and still maintain lower
    # than a one-in-a-million chance of accidental collision. (1e9 JIDs is 30
    # bits, square for the "birthday-paradox" to get 60 bits, add 20 bits for
    # the one-in-a-million margin to get 80 bits)

    # if length were no issue, we'd prefer to use this:
    h = os.urandom(80/8)
    s = base64.b64encode(h, "AB").strip("=")
    jid = "jid1-" + s
    return jid

def preflight_config(target_cfg, filename, stderr=sys.stderr):
    modified = False
    config = json.load(open(filename, 'r'))

    if "id" not in config:
        print >>stderr, ("No 'id' in package.json: creating a new ID for you.")
        jid = create_jid()
        config["id"] = jid
        modified = True

    if modified:
        i = 0
        backup = filename + ".backup"
        while os.path.exists(backup):
            if i > 1000:
                raise ValueError("I'm having problems finding a good name"
                                 " for the backup file. Please move %s out"
                                 " of the way and try again."
                                 % (filename + ".backup"))
            backup = filename + ".backup-%d" % i
            i += 1
        os.rename(filename, backup)
        new_json = json.dumps(config, indent=4)
        open(filename, 'w').write(new_json+"\n")
        return False, True

    return True, False
