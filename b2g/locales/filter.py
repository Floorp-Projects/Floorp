# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def test(mod, path, entity = None):
  import re
  # ignore anything but b2g, which is our local repo checkout name
  if mod not in ("netwerk", "dom", "toolkit", "security/manager",
                 "services/sync", "embedding/android",
                 "b2g"):
    return False

  # Ignore Lorentz strings, at least temporarily
  if mod == "toolkit" and path == "chrome/mozapps/plugins/plugins.dtd":
    if entity.startswith('reloadPlugin.'): return False
    if entity.startswith('report.'): return False

  if mod != "b2g":
    # we only have exceptions for b2g
    return True
  if not entity:
    return not (re.match(r"b2g-l10n.js", path) or
                re.match(r"defines.inc", path))
  if path == "defines.inc":
    return entity != "MOZ_LANGPACK_CONTRIBUTORS"

  if path != "chrome/region.properties":
    # only region.properties exceptions remain, compare all others
    return True
  
  return not (re.match(r"browser\.search\.order\.[1-9]", entity) or
              re.match(r"browser\.contentHandlers\.types\.[0-5]", entity) or
              re.match(r"gecko\.handlerService\.schemes\.", entity) or
              re.match(r"gecko\.handlerService\.defaultHandlersVersion", entity))
