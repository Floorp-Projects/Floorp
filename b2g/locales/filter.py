# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Axel Hecht <l10n@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****


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
