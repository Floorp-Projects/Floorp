/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/* global jest */
const { shallow } = require("enzyme");
const {
  REPS,
  getRep,
} = require("resource://devtools/client/shared/components/reps/reps/rep.js");

const {
  expectActorAttribute,
} = require("resource://devtools/client/shared/components/test/node/components/reps/test-helpers.js");

const { ErrorRep } = REPS;
const {
  MODE,
} = require("resource://devtools/client/shared/components/reps/reps/constants.js");
const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/error.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

describe("Error - Simple error", () => {
  // Test object = `new Error("Error message")`
  const stub = stubs.get("SimpleError");

  it("correctly selects Error Rep for Error object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text for simple error", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        shouldRenderTooltip: true,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
    expect(renderedComponent.prop("title")).toBe('Error: "Error message"');
    expectActorAttribute(renderedComponent, stub.actor);
  });

  it("renders with expected text for simple error in tiny mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("Error");
  });

  it("renders with expected text for simple error in HEADER mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.HEADER,
      })
    );

    expect(renderedComponent.text()).toEqual("Error");
  });

  it("renders with error type and preview message when in short mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stubs.get("MultilineStackError"),
        mode: MODE.SHORT,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });

  it("renders with error type only when customFormat prop isn't set", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stubs.get("MultilineStackError"),
        mode: MODE.SHORT,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });

  it("renders with error type only when depth is > 0", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stubs.get("MultilineStackError"),
        customFormat: true,
        depth: 1,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });
});

describe("Error - Multi line stack error", () => {
  /*
   * Test object = `
   *   function errorFoo() {
   *     errorBar();
   *   }
   *   function errorBar() {
   *     console.log(new Error("bar"));
   *   }
   *   errorFoo();`
   */
  const stub = stubs.get("MultilineStackError");

  it("correctly selects the Error Rep for Error object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text for Error object", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });

  it("renders expected text for simple multiline error in tiny mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("Error");
  });

  it("renders expected text for simple multiline error in HEADER mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.HEADER,
      })
    );

    expect(renderedComponent.text()).toEqual("Error");
  });
});

describe("Error - Error without stacktrace", () => {
  const stub = stubs.get("ErrorWithoutStacktrace");

  it("correctly selects the Error Rep for Error object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text for Error object", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent.text()).toEqual("Error: Error message");
  });

  it("renders expected text for error without stacktrace in tiny mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("Error");
  });
});

describe("Error - Eval error", () => {
  // Test object = `new EvalError("EvalError message")`
  const stub = stubs.get("EvalError");

  it("correctly selects the Error Rep for EvalError object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text for an EvalError", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });

  it("renders with expected text for an EvalError in tiny mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("EvalError");
  });

  it("renders with expected text for an EvalError in HEADER mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.HEADER,
      })
    );

    expect(renderedComponent.text()).toEqual("EvalError");
  });
});

describe("Error - Internal error", () => {
  // Test object = `new InternalError("InternalError message")`
  const stub = stubs.get("InternalError");

  it("correctly selects the Error Rep for InternalError object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text for an InternalError", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });

  it("renders with expected text for an InternalError in tiny mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("InternalError");
  });

  it("renders with expected text for an InternalError in HEADER mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.HEADER,
      })
    );

    expect(renderedComponent.text()).toEqual("InternalError");
  });
});

describe("Error - Range error", () => {
  // Test object = `new RangeError("RangeError message")`
  const stub = stubs.get("RangeError");

  it("correctly selects the Error Rep for RangeError object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text for RangeError", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });

  it("renders with expected text for RangeError in tiny mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("RangeError");
  });

  it("renders with expected text for RangeError in HEADER mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.HEADER,
      })
    );

    expect(renderedComponent.text()).toEqual("RangeError");
  });
});

describe("Error - Reference error", () => {
  // Test object = `new ReferenceError("ReferenceError message"`
  const stub = stubs.get("ReferenceError");

  it("correctly selects the Error Rep for ReferenceError object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text for ReferenceError", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });

  it("renders with expected text for ReferenceError in tiny mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("ReferenceError");
  });

  it("renders with expected text for ReferenceError in HEADER mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.HEADER,
      })
    );

    expect(renderedComponent.text()).toEqual("ReferenceError");
  });
});

describe("Error - Syntax error", () => {
  // Test object = `new SyntaxError("SyntaxError message"`
  const stub = stubs.get("SyntaxError");

  it("correctly selects the Error Rep for SyntaxError object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text for SyntaxError", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });

  it("renders with expected text for SyntaxError in tiny mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("SyntaxError");
  });

  it("renders with expected text for SyntaxError in HEADER mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.HEADER,
      })
    );

    expect(renderedComponent.text()).toEqual("SyntaxError");
  });
});

describe("Error - Type error", () => {
  // Test object = `new TypeError("TypeError message"`
  const stub = stubs.get("TypeError");

  it("correctly selects the Error Rep for TypeError object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text for TypeError", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });

  it("renders with expected text for TypeError in tiny mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("TypeError");
  });

  it("renders with expected text for TypeError in HEADER mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.HEADER,
      })
    );

    expect(renderedComponent.text()).toEqual("TypeError");
  });
});

describe("Error - URI error", () => {
  // Test object = `new URIError("URIError message")`
  const stub = stubs.get("URIError");

  it("correctly selects the Error Rep for URIError object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text for URIError", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });

  it("renders with expected text for URIError in tiny mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("URIError");
  });

  it("renders with expected text for URIError in HEADER mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.HEADER,
      })
    );

    expect(renderedComponent.text()).toEqual("URIError");
  });
});

describe("Error - DOMException", () => {
  const stub = stubs.get("DOMException");

  it("correctly selects Error Rep for Error object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text for DOMException", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent.text()).toEqual(
      "DOMException: 'foo;()bar!' is not a valid selector"
    );
  });

  it("renders with expected text for DOMException in tiny mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("DOMException");
  });

  it("renders with expected text for DOMException in HEADER mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.HEADER,
      })
    );

    expect(renderedComponent.text()).toEqual("DOMException");
  });
});

describe("Error - base-loader.sys.mjs", () => {
  const stub = stubs.get("base-loader Error");

  it("renders as expected without mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });

  it("renders as expected in tiny mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });

  it("renders as expected in HEADER mode", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.HEADER,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });
});

describe("Error - longString stacktrace", () => {
  const stub = stubs.get("longString stack Error");

  it("renders as expected", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });
});

describe("Error - longString stacktrace - cut-off location", () => {
  const stub = stubs.get("longString stack Error - cut-off location");

  it("renders as expected", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });
});

describe("Error - stacktrace location click", () => {
  it("Calls onViewSourceInDebugger with the expected arguments", () => {
    const onViewSourceInDebugger = jest.fn();
    const object = stubs.get("base-loader Error");

    const renderedComponent = shallow(
      ErrorRep.rep({
        object,
        onViewSourceInDebugger,
        customFormat: true,
      })
    );

    const locations = renderedComponent.find(".objectBox-stackTrace-location");
    expect(locations.exists()).toBeTruthy();

    expect(locations.first().prop("title")).toBe(
      "View source in debugger → " +
        "resource://devtools/client/debugger-client.js:856:9"
    );
    locations.first().simulate("click", {
      type: "click",
      stopPropagation: () => {},
    });

    expect(onViewSourceInDebugger.mock.calls).toHaveLength(1);
    let mockCall = onViewSourceInDebugger.mock.calls[0][0];
    expect(mockCall.url).toEqual(
      "resource://devtools/client/debugger-client.js"
    );
    expect(mockCall.line).toEqual(856);
    expect(mockCall.column).toEqual(9);

    expect(locations.last().prop("title")).toBe(
      "View source in debugger → " +
        "resource://devtools/shared/ThreadSafeDevToolsUtils.js:109:14"
    );
    locations.last().simulate("click", {
      type: "click",
      stopPropagation: () => {},
    });

    expect(onViewSourceInDebugger.mock.calls).toHaveLength(2);
    mockCall = onViewSourceInDebugger.mock.calls[1][0];
    expect(mockCall.url).toEqual(
      "resource://devtools/shared/ThreadSafeDevToolsUtils.js"
    );
    expect(mockCall.line).toEqual(109);
    expect(mockCall.column).toEqual(14);
  });

  it("Does not call onViewSourceInDebugger on excluded urls", () => {
    const onViewSourceInDebugger = jest.fn();
    const object = stubs.get("URIError");

    const renderedComponent = shallow(
      ErrorRep.rep({
        object,
        onViewSourceInDebugger,
        customFormat: true,
      })
    );

    const locations = renderedComponent.find(".objectBox-stackTrace-location");
    expect(locations.exists()).toBeTruthy();
    expect(locations.first().prop("title")).toBe(undefined);

    locations.first().simulate("click", {
      type: "click",
      stopPropagation: () => {},
    });

    expect(onViewSourceInDebugger.mock.calls).toHaveLength(0);
  });

  it("Does not throw when onViewSourceInDebugger props is not provided", () => {
    const object = stubs.get("base-loader Error");

    const renderedComponent = shallow(
      ErrorRep.rep({
        object,
        customFormat: true,
      })
    );

    const locations = renderedComponent.find(".objectBox-stackTrace-location");
    expect(locations.exists()).toBeTruthy();
    expect(locations.first().prop("title")).toBe(undefined);

    locations.first().simulate("click", {
      type: "click",
      stopPropagation: () => {},
    });
  });
});

describe("Error - renderStacktrace prop", () => {
  it("uses renderStacktrace prop when provided", () => {
    const stub = stubs.get("MultilineStackError");

    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        renderStacktrace: frames => {
          return frames.map(frame =>
            dom.li(
              { className: "frame" },
              `Function ${frame.functionName} called from ${frame.filename}:${frame.lineNumber}:${frame.columnNumber}\n`
            )
          );
        },
        customFormat: true,
      })
    );
    expect(renderedComponent).toMatchSnapshot();
  });

  it("uses renderStacktrace with longString errors too", () => {
    const stub = stubs.get("longString stack Error - cut-off location");
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        renderStacktrace: frames => {
          return frames.map(frame =>
            dom.li(
              { className: "frame" },
              `Function ${frame.functionName} called from ${frame.filename}:${frame.lineNumber}:${frame.columnNumber}\n`
            )
          );
        },
        customFormat: true,
      })
    );
    expect(renderedComponent).toMatchSnapshot();
  });
});

describe("Error - Error with V8-like stack", () => {
  // Test object:
  // x = new Error("BOOM");
  // x.stack = "Error: BOOM\ngetAccount@http://moz.com/script.js:1:2";
  const stub = stubs.get("Error with V8-like stack");

  it("correctly selects Error Rep for Error object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
    expectActorAttribute(renderedComponent, stub.actor);
  });
});

describe("Error - Error with invalid stack", () => {
  // Test object:
  // x = new Error("bad stack");
  // x.stack = "bar\nbaz\nfoo\n\n\n\n\n\n\n";
  const stub = stubs.get("Error with invalid stack");

  it("correctly selects Error Rep for Error object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
    expectActorAttribute(renderedComponent, stub.actor);
  });
});

describe("Error - Error with undefined-grip stack", () => {
  // Test object:
  // x = new Error("sd");
  // x.stack = undefined;
  const stub = stubs.get("Error with undefined-grip stack");

  it("correctly selects Error Rep for Error object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
    expectActorAttribute(renderedComponent, stub.actor);
  });
});

describe("Error - Error with undefined-grip name", () => {
  // Test object:
  // x = new Error("");
  // x.name = undefined;
  const stub = stubs.get("Error with undefined-grip name");

  it("correctly selects Error Rep for Error object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );
    expect(renderedComponent).toMatchSnapshot();

    const tinyRenderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(tinyRenderedComponent).toMatchSnapshot();
  });
});

describe("Error - Error with undefined-grip message", () => {
  // Test object:
  // x = new Error("");
  // x.message = undefined;
  const stub = stubs.get("Error with undefined-grip message");

  it("correctly selects Error Rep for Error object", () => {
    expect(getRep(stub)).toBe(ErrorRep.rep);
  });

  it("renders with expected text", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );
    expect(renderedComponent).toMatchSnapshot();

    const tinyRenderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(tinyRenderedComponent).toMatchSnapshot();
  });
});

describe("Error - Error with stack having frames with multiple @", () => {
  const stub = stubs.get("Error with stack having frames with multiple @");

  it("renders with expected text for Error object", () => {
    const renderedComponent = shallow(
      ErrorRep.rep({
        object: stub,
        customFormat: true,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });
});
