# DevTools Performance Tests overview

This page provides a short overview of the various DevTools performance tests.

## damp

DAMP (short for DevTools At Maximum Performance) is the main DevTools performance test suite, based on the talos framework. It mostly runs end to end scenarios, opening the toolbox, various panels and interacting with the UI. It might regress for a wide variety of reasons: DevTools frontend changes, DevTools server changes, platform changes etc. To investigate DAMP regressions or improvements, it is usually necessary to analyze DAMP subtests individually.

See [DAMP Performance tests](performance-tests-damp.md) for more details on how to run DAMP, analyze results or add new tests.

## debugger-metrics

debugger-metrics measures the number of modules and the overall size of modules loaded when opening the Debugger in DevTools. This test is a mochitest which can be executed locally with:

```bash
./mach test devtools/client/framework/test/metrics/browser_metrics_debugger.js --headless
```

At the end of the test, logs should contain a `PERFHERDER_DATA` entry containing 4 measures. `debugger-modules` is the number of debugger-specific modules loaded, `debugger-chars` is the number of characters in said modules. `all-modules` is the number of modules loaded including shared modules, `all-chars` is the number of characters in said modules.

A significant regression or improvement to this test can indicate that modules are no longer lazy loaded, or a new part of the UI is now loaded upfront.

## inspector-metrics

See the description for debugger-metrics. This test is exactly the same but applied to the inspector panel. It can be executed locally with:

```bash
./mach test devtools/client/framework/test/metrics/browser_metrics_inspector.js --headless
```

## netmonitor-metrics

See the description for debugger-metrics. This test is exactly the same but applied to the netmonitor panel. It can be executed locally with:

```bash
./mach test devtools/client/framework/test/metrics/browser_metrics_netmonitor.js --headless
```

## webconsole-metrics

See the description for debugger-metrics. This test is exactly the same but applied to the webconsole panel. It can be executed locally with:

```bash
./mach test devtools/client/framework/test/metrics/browser_metrics_webconsole.js --headless
```

## server.pool

server.pool measures the performance of the DevTools `Pool` [class](https://searchfox.org/mozilla-central/source/devtools/shared/protocol/Pool.js) which is intensively used by the DevTools server. This test is a mochitest which can be executed with:

```bash
./mach test devtools/client/framework/test/metrics/browser_metrics_pool.js --headless
```

At the end of the test, logs should contain a `PERFHERDER_DATA` entry which contain values corresponding to various APIs of the `Pool` class.

A regression or improvement in this test is most likely linked to a change in a file from devtools/shared/protocol.

## total-after-gc

total-after-gc measures the number of objects created by DevTools to inspect a tab target. It does not involve DevTools frontend. This test is a mochitest which can be executed with:

```bash
./mach test devtools/client/framework/test/allocations/browser_allocations_target.js --headless
```

The test will open a tab and will objects in a similar way to what the DevTools Toolbox would need. At that point, the test will measure how many objects have been created. It will then attempt to destroy all the created objects, will perform several garbage and cycle collections and will measure how many objects are remaining. The `PERFHERDER_DATA` entry in the logs should contain both measures.

A regression or improvement here may indicate a leak, for instance a module which no longer cleans its dependencies. It can also indicate that DevTools is loading more singletons or other objects which are not tied to the lifecycle of the DevTools objects.
