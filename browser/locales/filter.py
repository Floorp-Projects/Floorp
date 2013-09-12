# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

def test(mod, path, entity = None):
  import re
  # ignore anything but Firefox
  if mod not in ("netwerk", "dom", "toolkit", "security/manager",
                 "browser", "browser/metro", "webapprt",
                 "extensions/reporter", "extensions/spellcheck",
                 "other-licenses/branding/firefox",
                 "browser/branding/official",
                 "services/sync"):
    return "ignore"
  if mod not in ("browser", "browser/metro", "extensions/spellcheck"):
    # we only have exceptions for browser, metro and extensions/spellcheck
    return "error"
  if not entity:
    # the only files to ignore are spell checkers and search
    if mod == "extensions/spellcheck":
      return "ignore"
    # browser
    return "ignore" if re.match(r"searchplugins\/.+\.xml", path) else "error"
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
  if mod == "browser/metro" and path == "chrome/region.properties":
      return ("ignore"
              if re.match(r"browser\.search\.order\.[1-9]", entity)
              else "error")
  return "error"
