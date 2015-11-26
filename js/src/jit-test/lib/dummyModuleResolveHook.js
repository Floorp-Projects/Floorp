/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A dummy implementation of the module resolve hook used by module tests. This
// implements the bare minimum necessary to allow modules to refer to each
// other.

let moduleRepo = {};
setModuleResolveHook(function(module, specifier) {
    if (specifier in moduleRepo)
        return moduleRepo[specifier];
    throw "Module '" + specifier + "' not found";
});
