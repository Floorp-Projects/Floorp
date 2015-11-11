/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A basic synchronous module loader for testing the shell.

Reflect.Loader = new class {
    constructor() {
        this.registry = new Map();
        this.loadPath = getModuleLoadPath();
    }

    resolve(name) {
        if (os.path.isAbsolute(name))
            return name;

        return os.path.join(this.loadPath, name);
    }

    fetch(path) {
        return os.file.readFile(path);
    }

    loadAndParse(name) {
        let path = this.resolve(name);

        if (this.registry.has(path))
            return this.registry.get(path);

        let source = this.fetch(path);
        let module = parseModule(source, path);
        this.registry.set(path, module);
        return module;
    }

    ["import"](name, referrer) {
        let module = this.loadAndParse(name);
        module.declarationInstantiation();
        return module.evaluation();
    }
};

setModuleResolveHook((module, requestName) => Reflect.Loader.loadAndParse(requestName));
