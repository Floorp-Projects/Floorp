const Services = require("devtools-services");

// const pref = Services;

describe("services telemetry shim", () => {
  beforeEach(() => {
    telemetry.scalars = {};
    telemetry.histograms = {};
  });

  it("getHistogramById", () => {
    const hist = Services.telemetry.getHistogramById("foo");
    hist.add(3);
    expect(telemetry.histograms.foo).toEqual([3]);

    hist.add(3);
    expect(telemetry.histograms.foo).toEqual([3, 3]);
  });

  it("getKeyedHistogramById", () => {
    const hist = Services.telemetry.getKeyedHistogramById("foo");
    hist.add("a", 3);
    expect(telemetry.histograms.foo.a).toEqual([3]);

    hist.add("a", 3);
    expect(telemetry.histograms.foo.a).toEqual([3, 3]);
  });

  it("scalarSet", () => {
    Services.telemetry.scalarSet("foo", 3);
    expect(telemetry.scalars.foo).toEqual(3);
  });

  it("scalarAdd", () => {
    Services.telemetry.scalarAdd("foo", 3);
    expect(telemetry.scalars.foo).toEqual(3);

    Services.telemetry.scalarAdd("foo", 3);
    expect(telemetry.scalars.foo).toEqual(6);
  });
});
