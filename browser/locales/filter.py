# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def test(mod, path, entity=None):
    # ignore anything but Firefox
    if mod not in (
        "netwerk",
        "dom",
        "toolkit",
        "security/manager",
        "devtools/client",
        "devtools/shared",
        "devtools/startup",
        "browser",
        "browser/extensions/formautofill",
        "browser/extensions/report-site-issue",
        "extensions/spellcheck",
        "other-licenses/branding/firefox",
        "browser/branding/official",
        "services/sync",
    ):
        return "ignore"
    if mod not in ("browser", "extensions/spellcheck"):
        # we only have exceptions for browser and extensions/spellcheck
        return "error"
    if entity is None:
        # the only files to ignore are spell checkers
        if mod == "extensions/spellcheck":
            return "ignore"
        return "error"
    if mod == "extensions/spellcheck":
        # l10n ships en-US dictionary or something, do compare
        return "error"
    return "error"
