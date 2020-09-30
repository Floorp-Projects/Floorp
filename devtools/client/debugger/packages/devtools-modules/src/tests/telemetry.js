const Telemetry = require("../utils/telemetry");
const telemetry = new Telemetry();

describe("telemetry shim", () => {
  it("msSystemNow", () => {
    expect(() => {
      telemetry.msSystemNow;
    }).not.toThrow();
  });

  it("start", () => {
    expect(() => {
      telemetry.start("foo", this);
    }).not.toThrow();
  });

  it("startKeyed", () => {
    expect(() => {
      telemetry.startKeyed("foo", "bar", this);
    }).not.toThrow();
  });

  it("finish", () => {
    expect(() => {
      telemetry.finish("foo", this);
    }).not.toThrow();
  });

  it("finishKeyed", () => {
    expect(() => {
      telemetry.finishKeyed("foo", "bar", this);
    }).not.toThrow();
  });

  it("getHistogramById", () => {
    expect(() => {
      telemetry.getHistogramById("foo").add(3);
    }).not.toThrow();
  });

  it("getKeyedHistogramById", () => {
    expect(() => {
      telemetry.getKeyedHistogramById("foo").add("foo", 3);
    }).not.toThrow();
  });

  it("scalarSet", () => {
    expect(() => {
      telemetry.scalarSet("foo", 3);
    }).not.toThrow();
  });

  it("scalarAdd", () => {
    expect(() => {
      telemetry.scalarAdd("foo", 3);
    }).not.toThrow();
  });

  it("keyedScalarAdd", () => {
    expect(() => {
      telemetry.keyedScalarAdd("foo", "bar", 3);
    }).not.toThrow();
  });

  it("setEventRecordingEnabled", () => {
    expect(() => {
      telemetry.setEventRecordingEnabled("foo", true);
    }).not.toThrow();
  });

  it("preparePendingEvent", () => {
    expect(() => {
      telemetry.preparePendingEvent(
        "devtools.main", "open", "inspector", null, ["foo", "bar"]);
    }).not.toThrow();
  });

  it("addEventProperty", () => {
    expect(() => {
      telemetry.addEventProperty(
        "devtools.main", "open", "inspector", "foo", "1");
    }).not.toThrow();
  });

  it("addEventProperties", () => {
    expect(() => {
      telemetry.addEventProperties("devtools.main", "open", "inspector", {
        "foo": "1",
        "bar": "2"
      });
    }).not.toThrow();
  });

  it("_sendPendingEvent", () => {
    expect(() => {
      telemetry._sendPendingEvent("devtools.main", "open", "inspector", null);
    }).not.toThrow();
  });

  it("recordEvent", () => {
    expect(() => {
      telemetry.recordEvent("devtools.main", "open", "inspector", null, {
        "foo": "1",
        "bar": "2"
      });
    }).not.toThrow();
  });

  it("toolOpened", () => {
    expect(() => {
      telemetry.toolOpened("foo");
    }).not.toThrow();
  });

  it("toolClosed", () => {
    expect(() => {
      telemetry.toolClosed("foo");
    }).not.toThrow();
  });
});
