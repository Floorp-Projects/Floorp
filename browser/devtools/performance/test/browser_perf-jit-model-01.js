/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that JITOptimizations track optimization sites and create
 * an OptimizationSiteProfile when adding optimization sites, like from the
 * FrameNode, and the returning of that data is as expected.
 */

const RecordingUtils = devtools.require("devtools/performance/recording-utils");

function test() {
  let { JITOptimizations } = devtools.require("devtools/performance/jit");

  let rawSites = [];
  rawSites.push(gRawSite2);
  rawSites.push(gRawSite2);
  rawSites.push(gRawSite1);
  rawSites.push(gRawSite1);
  rawSites.push(gRawSite2);
  rawSites.push(gRawSite3);

  let jit = new JITOptimizations(rawSites, gStringTable.stringTable);
  let sites = jit.optimizationSites;

  let [first, second, third] = sites;

  is(first.id, 0, "site id is array index");
  is(first.samples, 3, "first OptimizationSiteProfile has correct sample count");
  is(first.data.line, 34, "includes OptimizationSite as reference under `data`");
  is(second.id, 1, "site id is array index");
  is(second.samples, 2, "second OptimizationSiteProfile has correct sample count");
  is(second.data.line, 12, "includes OptimizationSite as reference under `data`");
  is(third.id, 2, "site id is array index");
  is(third.samples, 1, "third OptimizationSiteProfile has correct sample count");
  is(third.data.line, 78, "includes OptimizationSite as reference under `data`");

  finish();
}

let gStringTable = new RecordingUtils.UniqueStrings();

function uniqStr(s) {
  return gStringTable.getOrAddStringIndex(s);
}

let gRawSite1 = {
  line: 12,
  column: 2,
  types: [{
    mirType: uniqStr("Object"),
    site: uniqStr("A (http://foo/bar/bar:12)"),
    typeset: [{
      keyedBy: uniqStr("constructor"),
      name: uniqStr("Foo"),
      location: uniqStr("A (http://foo/bar/baz:12)")
    }, {
      keyedBy: uniqStr("primitive"),
      location: uniqStr("self-hosted")
    }]
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")],
      [uniqStr("Inlined"), uniqStr("SomeGetter3")]
    ]
  }
};

let gRawSite2 = {
  line: 34,
  types: [{
    mirType: uniqStr("Int32"),
    site: uniqStr("Receiver")
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")],
      [uniqStr("Failure3"), uniqStr("SomeGetter3")]
    ]
  }
};

let gRawSite3 = {
  line: 78,
  types: [{
    mirType: uniqStr("Object"),
    site: uniqStr("A (http://foo/bar/bar:12)"),
    typeset: [{
      keyedBy: uniqStr("constructor"),
      name: uniqStr("Foo"),
      location: uniqStr("A (http://foo/bar/baz:12)")
    }, {
      keyedBy: uniqStr("primitive"),
      location: uniqStr("self-hosted")
    }]
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")],
      [uniqStr("GenericSuccess"), uniqStr("SomeGetter3")]
    ]
  }
};
