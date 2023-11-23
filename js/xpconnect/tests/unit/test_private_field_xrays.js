'use strict';

add_task(async function () {
  let webnav = Services.appShell.createWindowlessBrowser(false);

  let docShell = webnav.docShell;

  docShell.createAboutBlankDocumentViewer(null, null);

  let window = webnav.document.defaultView;

  let iframe = window.eval(`
    iframe = document.createElement("iframe");
    iframe.id = "iframe"
    document.body.appendChild(iframe)
    iframe`);


  let unwrapped = Cu.waiveXrays(iframe);


  class Base {
    constructor(o) {
      return o;
    }
  };


  class A extends Base {
    #x = 12;
    static gx(o) {
      return o.#x;
    }

    static sx(o, v) {
      o.#x = v;
    }
  };

  new A(iframe);
  Assert.equal(A.gx(iframe), 12);
  A.sx(iframe, 'wrapped');

  // Shouldn't tunnel past xray.
  Assert.throws(() => A.gx(unwrapped), TypeError);
  Assert.throws(() => A.sx(unwrapped, 'unwrapped'), TypeError);

  new A(unwrapped);
  Assert.equal(A.gx(unwrapped), 12);
  Assert.equal(A.gx(iframe), 'wrapped');

  A.sx(iframe, 'modified');
  Assert.equal(A.gx(unwrapped), 12);
  A.sx(unwrapped, 16);
  Assert.equal(A.gx(iframe), 'modified');
});
