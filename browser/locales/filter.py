# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

def test(mod, path, entity = None):
  import re
  # ignore anything but Firefox
  if mod not in ("netwerk", "dom", "toolkit", "security/manager",
                 "devtools/client", "devtools/shared",
                 "browser",
                 "extensions/spellcheck",
                 "other-licenses/branding/firefox",
                 "browser/branding/official",
                 "services/sync"):
    return "ignore"
  if mod not in ("browser", "extensions/spellcheck"):
    # we only have exceptions for browser and extensions/spellcheck
    return "error"
  if not entity:
    # the only files to ignore are spell checkers
    if mod == "extensions/spellcheck":
      return "ignore"
    return "error"
  if mod == "extensions/spellcheck":
    # l10n ships en-US dictionary or something, do compare
    return "error"
  if path == "defines.inc":
    return "ignore" if entity == "MOZ_LANGPACK_CONTRIBUTORS" else "error"

  if mod == "browser" and path == "chrome/browser-region/region.properties":
    # only region.properties exceptions remain, compare all others
    return ("ignore"
            if (re.match(r"browser\.search\.order\.[1-9]", entity) or
                re.match(r"browser\.contentHandlers\.types\.[0-5]", entity) or
                re.match(r"gecko\.handlerService\.schemes\.", entity) or
                re.match(r"gecko\.handlerService\.defaultHandlersVersion", entity))
            else "error")
  return "error"
