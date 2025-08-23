/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const _mapPromiseModuleState = new Map<
  string,
  [Promise<void>, () => void, (reason: any) => void]
>();

function createPromise(): [Promise<void>, () => void, (reason: any) => void] {
  let rs: (() => void) | null = null;
  let rj: ((reason: any) => void) | null = null;
  const p = new Promise<void>((resolve, reject) => {
    rs = resolve;
    rj = reject;
  });
  return [p, rs!, rj!];
}

interface TModuleLib {
  onModuleLoaded: (module: string) => Promise<void>;
  _registerModuleLoadState: (module: string, isLoaded: boolean) => void;
}

class ModuleLib implements TModuleLib {
  static _instance: ModuleLib | null = null;
  static getInstance() {
    if (!this._instance) {
      this._instance = new ModuleLib();
    }
    return this._instance;
  }
  onModuleLoaded(module: string): Promise<void> {
    if (_mapPromiseModuleState.has(module)) {
      return _mapPromiseModuleState.get(module)![0];
    } else if (this.rejected) {
      return Promise.reject(new Error("Module Not Found"));
    }

    const pms = createPromise();
    _mapPromiseModuleState.set(module, pms);
    return pms[0];
  }
  _registerModuleLoadState(module: string, isLoaded: boolean) {
    if (!_mapPromiseModuleState.has(module)) {
      const pms = createPromise();
      _mapPromiseModuleState.set(module, pms);
    }
    if (isLoaded) {
      _mapPromiseModuleState.get(module)![1]();
    } else {
      _mapPromiseModuleState.get(module)![2](
        new Error(`Failed to load module : ${module}`),
      );
    }
  }
  async _rejectOtherLoadStates() {
    for (const [_, pms] of _mapPromiseModuleState) {
      const t = {};
      if (t === (await Promise.race([pms[0], t]))) {
        pms[2](new Error("Module Not Found"));
      }
    }
    this.rejected = true;
  }
  private rejected = false;
}

export function onModuleLoaded(module: string): Promise<void> {
  return ModuleLib.getInstance().onModuleLoaded(module);
}

export function _registerModuleLoadState(module: string, isLoaded: boolean) {
  return ModuleLib.getInstance()._registerModuleLoadState(module, isLoaded);
}

export function _rejectOtherLoadStates() {
  return ModuleLib.getInstance()._rejectOtherLoadStates();
}
