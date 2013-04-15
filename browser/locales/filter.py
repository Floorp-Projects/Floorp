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
    return False
  if mod != "browser" and mod != "extensions/spellcheck":
    # we only have exceptions for browser and extensions/spellcheck
    return True
  if not entity:
    if mod == "extensions/spellcheck":
      return False
    # browser
    return not (re.match(r"searchplugins\/.+\.xml", path) or
                re.match(r"chrome\/help\/images\/[A-Za-z-_]+\.png", path))
  if mod == "extensions/spellcheck":
    # l10n ships en-US dictionary or something, do compare
    return True
  if path == "defines.inc":
    return entity != "MOZ_LANGPACK_CONTRIBUTORS"

  if path != "chrome/browser-region/region.properties":
    # only region.properties exceptions remain, compare all others
    return True
  
  return not (re.match(r"browser\.search\.order\.[1-9]", entity) or
              re.match(r"browser\.contentHandlers\.types\.[0-5]", entity) or
              re.match(r"gecko\.handlerService\.schemes\.", entity) or
              re.match(r"gecko\.handlerService\.defaultHandlersVersion", entity))
