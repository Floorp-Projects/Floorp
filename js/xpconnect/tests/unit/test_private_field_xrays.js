'use strict'

ChromeUtils.import('resource://gre/modules/Preferences.jsm');

add_task(async function() {
  let webnav = Services.appShell.createWindowlessBrowser(false);

  let docShell = webnav.docShell;

  docShell.createAboutBlankContentViewer(null, null);

  let window = webnav.document.defaultView;
  let unwrapped = Cu.waiveXrays(window);

  class Base {
    constructor(o) {
      return o;
    }
  };

  var A;
  try {
    A = eval(`
    (function() {
      class A extends Base {
        #x = 12;
        static gx(o) {
          return o.#x;
        }

        static sx(o, v) {
          o.#x = v;
        }
      };
      return A})()`);
  } catch (e) {
    Assert.equal(e instanceof SyntaxError, true);
    Assert.equal(
        /private fields are not currently supported/.test(e.message), true);
    // Early return if private fields aren't enabled.
    return;
  }

  new A(window);
  Assert.equal(A.gx(window), 12);
  A.sx(window, 'wrapped');

  // Shouldn't tunnel past xray.
  Assert.throws(() => A.gx(unwrapped), TypeError);
  Assert.throws(() => A.sx(unwrapped, 'unwrapped'), TypeError);

  new A(unwrapped);
  Assert.equal(A.gx(unwrapped), 12);
  Assert.equal(A.gx(window), 'wrapped');

  A.sx(window, 'modified');
  Assert.equal(A.gx(unwrapped), 12);
  A.sx(unwrapped, 16);
  Assert.equal(A.gx(window), 'modified');
});
