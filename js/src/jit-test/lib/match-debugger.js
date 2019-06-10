// Debugger-oriented Pattern subclasses.

if (typeof Match !== 'function') {
  load(libdir + 'match.js');
}

class DebuggerObjectPattern extends Match.Pattern {
  constructor(className) {
    super();
    this.className = className;
  }

  match(actual) {
    if (!(actual instanceof Debugger.Object) || actual.class !== this.className) {
      throw new Match.MatchError(`Expected Debugger.Object of class ${this.className}, got ${actual}`);
    }
    return true;
  }
}

