#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# This script is run during configure, taking variables set in configure
# and producing a JSON file that describes some portions of the build
# configuration, such as the target OS and CPU.
#
# The output file is intended to be used as input to the mozinfo package.
from __future__ import print_function
import os
import re
import sys
import json


def build_dict(env=os.environ):
    """
    Build a dict containing data about the build configuration from
    the environment.
    """
    d = {}
    # Check that all required variables are present first.
    required = ["TARGET_CPU", "OS_TARGET", "MOZ_WIDGET_TOOLKIT"]
    missing = [r for r in required if r not in env]
    if missing:
        raise Exception("Missing required environment variables: %s" %
                        ', '.join(missing))

    if 'MOZCONFIG' in env:
        mozconfig = env["MOZCONFIG"]
        if 'TOPSRCDIR' in env:
            mozconfig = os.path.join(env["TOPSRCDIR"], mozconfig)
        d['mozconfig'] = os.path.normpath(mozconfig)

    if 'TOPSRCDIR' in env:
        d["topsrcdir"] = env["TOPSRCDIR"]

    # os
    o = env["OS_TARGET"]
    known_os = {"Linux": "linux",
                "WINNT": "win",
                "Darwin": "mac",
                "Android": "b2g" if env["MOZ_WIDGET_TOOLKIT"] == "gonk" else "android"}
    if o in known_os:
        d["os"] = known_os[o]
    else:
        # Allow unknown values, just lowercase them.
        d["os"] = o.lower()

    # Widget toolkit, just pass the value directly through.
    d["toolkit"] = env["MOZ_WIDGET_TOOLKIT"]

    # Application name
    if 'MOZ_APP_NAME' in env:
        d["appname"] = env["MOZ_APP_NAME"]

    # processor
    p = env["TARGET_CPU"]
    # for universal mac builds, put in a special value
    if d["os"] == "mac" and "UNIVERSAL_BINARY" in env and env["UNIVERSAL_BINARY"] == "1":
        p = "universal-x86-x86_64"
    else:
        # do some slight massaging for some values
        #TODO: retain specific values in case someone wants them?
        if p.startswith("arm"):
            p = "arm"
        elif re.match("i[3-9]86", p):
            p = "x86"
    d["processor"] = p
    # hardcoded list of 64-bit CPUs
    if p in ["x86_64", "ppc64"]:
        d["bits"] = 64
    # hardcoded list of known 32-bit CPUs
    elif p in ["x86", "arm", "ppc"]:
        d["bits"] = 32
    # other CPUs will wind up with unknown bits

    # debug
    d["debug"] = 'MOZ_DEBUG' in env and env['MOZ_DEBUG'] == '1'

    # crashreporter
    d["crashreporter"] = 'MOZ_CRASHREPORTER' in env and env['MOZ_CRASHREPORTER'] == '1'

    # asan
    d["asan"] = 'MOZ_ASAN' in env and env['MOZ_ASAN'] == '1'
    return d

def write_json(file, env=os.environ):
    """
    Write JSON data about the configuration specified in |env|
    to |file|, which may be a filename or file-like object.
    See build_dict for information about what  environment variables are used,
    and what keys are produced.
    """
    build_conf = build_dict(env)
    if isinstance(file, basestring):
        with open(file, "w") as f:
            json.dump(build_conf, f)
    else:
        json.dump(build_conf, file)


if __name__ == '__main__':
    try:
        write_json(sys.argv[1] if len(sys.argv) > 1 else sys.stdout)
    except Exception as e:
        print(str(e), file=sys.stderr)
        sys.exit(1)
