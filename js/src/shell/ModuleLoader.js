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

    loadAndParse(path) {
        if (this.registry.has(path))
            return this.registry.get(path);

        let source = this.fetch(path);
        let module = parseModule(source, path);
        this.registry.set(path, module);
        return module;
    }

    loadAndExecute(path) {
        let module = this.loadAndParse(path);
        module.declarationInstantiation();
        return module.evaluation();
    }

    importRoot(path) {
        return this.loadAndExecute(path);
    }

    ["import"](name, referrer) {
        let path = this.resolve(name);
        return this.loadAndExecute(path);
    }
};

setModuleResolveHook((module, requestName) => {
    let path = Reflect.Loader.resolve(requestName);
    return Reflect.Loader.loadAndParse(path)
});
