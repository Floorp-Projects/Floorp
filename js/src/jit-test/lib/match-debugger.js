// Debugger-oriented Pattern subclasses.

if (typeof Match !== 'function') {
  load(libdir + 'match.js');
}

class DebuggerObjectPattern extends Match.Pattern {
  constructor(className, props) {
    super();
    this.className = className;
    if (props) {
      this.props = Match.Pattern.OBJECT_WITH_EXACTLY(props);
    }
  }

  match(actual) {
    if (!(actual instanceof Debugger.Object)) {
      throw new Match.MatchError(`Expected Debugger.Object, got ${actual}`);
    }

    if (actual.class !== this.className) {
      throw new Match.MatchError(`Expected Debugger.Object of class ${this.className}, got Debugger.Object of class ${actual.class}`);
    }

    if (this.props !== undefined) {
      const lifted = {};
      for (const name of actual.getOwnPropertyNames()) {
        const desc = actual.getOwnPropertyDescriptor(name);
        if (!('value' in desc)) {
          throw new Match.MatchError(`Debugger.Object referent has non-value property ${uneval(name)}`);
        }
        lifted[name] = desc.value;
      }

      try {
        this.props.match(lifted);
      } catch (inner) {
        if (!(inner instanceof Match.MatchError)) {
          throw inner;
        }
        inner.message = `matching Debugger.Object referent properties:\n${inner.message}`;
        throw inner;
      }
    }

    return true;
  }
}

