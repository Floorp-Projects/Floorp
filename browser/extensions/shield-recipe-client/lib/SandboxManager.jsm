const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["SandboxManager"];

this.SandboxManager = class {
  constructor() {
    this._sandbox = makeSandbox();
    this.holds = [];
  }

  get sandbox() {
    if (this._sandbox) {
      return this._sandbox;
    }
    throw new Error("Tried to use sandbox after it was nuked");
  }

  addHold(name) {
    this.holds.push(name);
  }

  removeHold(name) {
    const index = this.holds.indexOf(name);
    if (index === -1) {
      throw new Error(`Tried to remove non-existant hold "${name}"`);
    }
    this.holds.splice(index, 1);
    this.tryCleanup();
  }

  cloneInto(value, options = {}) {
    return Cu.cloneInto(value, this.sandbox, options);
  }

  cloneIntoGlobal(name, value, options = {}) {
    const clonedValue = Cu.cloneInto(value, this.sandbox, options);
    this.addGlobal(name, clonedValue);
    return clonedValue;
  }

  addGlobal(name, value) {
    this.sandbox[name] = value;
  }

  evalInSandbox(script) {
    return Cu.evalInSandbox(script, this.sandbox);
  }

  tryCleanup() {
    if (this.holds.length === 0) {
      const sandbox = this._sandbox;
      this._sandbox = null;
      Cu.nukeSandbox(sandbox);
    }
  }

  isNuked() {
    // Do this in a promise, so other async things can resolve.
    return new Promise((resolve, reject) => {
      if (!this._sandbox) {
        resolve();
      } else {
        reject(new Error(`Sandbox is not nuked. Holds left: ${this.holds}`));
      }
    });
  }
};


function makeSandbox() {
  const sandbox = new Cu.Sandbox(null, {
    wantComponents: false,
    wantGlobalProperties: ["URL", "URLSearchParams"],
  });

  return sandbox;
}
