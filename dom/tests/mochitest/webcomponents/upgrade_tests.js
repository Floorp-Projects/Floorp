function test_upgrade(f, msg) {
  // Run upgrading test on an element created by HTML parser.
  test_with_new_window(function(testWindow, testMsg) {
    let elementParser = testWindow.document.querySelector("unresolved-element");
    f(testWindow, elementParser, testMsg);
  }, msg + " (HTML parser)");

  // Run upgrading test on an element created by document.createElement.
  test_with_new_window(function(testWindow, testMsg) {
    let element = testWindow.document.createElement("unresolved-element");
    testWindow.document.documentElement.appendChild(element);
    f(testWindow, element, testMsg);
  }, msg + " (document.createElement)");
}

// Test cases

test_upgrade(function(testWindow, testElement, msg) {
  class MyCustomElement extends testWindow.HTMLElement {}
  testWindow.customElements.define("unresolved-element", MyCustomElement);
  SimpleTest.is(
    Object.getPrototypeOf(Cu.waiveXrays(testElement)),
    MyCustomElement.prototype,
    msg
  );
}, "Custom element must be upgraded if there is a matching definition");

test_upgrade(function(testWindow, testElement, msg) {
  testElement.remove();
  class MyCustomElement extends testWindow.HTMLElement {}
  testWindow.customElements.define("unresolved-element", MyCustomElement);
  SimpleTest.is(
    Object.getPrototypeOf(testElement),
    testWindow.HTMLElement.prototype,
    msg
  );
}, "Custom element must not be upgraded if it has been removed from tree");

test_upgrade(function(testWindow, testElement, msg) {
  let exceptionToThrow = { name: "exception thrown by a custom constructor" };
  class ThrowCustomElement extends testWindow.HTMLElement {
    constructor() {
      super();
      if (exceptionToThrow) {
        throw exceptionToThrow;
      }
    }
  }

  let uncaughtError;
  window.onerror = function(message, url, lineNumber, columnNumber, error) {
    uncaughtError = error;
    return true;
  };
  testWindow.customElements.define("unresolved-element", ThrowCustomElement);

  SimpleTest.is(uncaughtError.name, exceptionToThrow.name, msg);
}, "Upgrading must report an exception thrown by a custom element constructor");

test_upgrade(function(testWindow, testElement, msg) {
  class InstantiatesItselfAfterSuper extends testWindow.HTMLElement {
    constructor(doNotCreateItself) {
      super();
      if (!doNotCreateItself) {
        new InstantiatesItselfAfterSuper(true);
      }
    }
  }

  let uncaughtError;
  window.onerror = function(message, url, lineNumber, columnNumber, error) {
    uncaughtError = error;
    return true;
  };
  testWindow.customElements.define(
    "unresolved-element",
    InstantiatesItselfAfterSuper
  );

  SimpleTest.is(uncaughtError.name, "InvalidStateError", msg);
}, "Upgrading must report an InvalidStateError when the top of the " +
  "construction stack is marked AlreadyConstructed");

test_upgrade(function(testWindow, testElement, msg) {
  class InstantiatesItselfBeforeSuper extends testWindow.HTMLElement {
    constructor(doNotCreateItself) {
      if (!doNotCreateItself) {
        new InstantiatesItselfBeforeSuper(true);
      }
      super();
    }
  }

  let uncaughtError;
  window.onerror = function(message, url, lineNumber, columnNumber, error) {
    uncaughtError = error;
    return true;
  };
  testWindow.customElements.define(
    "unresolved-element",
    InstantiatesItselfBeforeSuper
  );

  SimpleTest.is(uncaughtError.name, "InvalidStateError", msg);
}, "Upgrading must report an InvalidStateError when the top of the " +
  "construction stack is marked AlreadyConstructed due to a custom element " +
  "constructor constructing itself before super() call");

test_upgrade(function(testWindow, testElement, msg) {
  class MyOtherElement extends testWindow.HTMLElement {
    constructor() {
      super();
      if (this == testElement) {
        return testWindow.document.createElement("other-element");
      }
    }
  }

  let uncaughtError;
  window.onerror = function(message, url, lineNumber, columnNumber, error) {
    uncaughtError = error;
    return true;
  };
  testWindow.customElements.define("unresolved-element", MyOtherElement);

  SimpleTest.is(uncaughtError.name, "InvalidStateError", msg);
}, "Upgrading must report an InvalidStateError when the returned element is " +
  "not SameValue as the upgraded element");
