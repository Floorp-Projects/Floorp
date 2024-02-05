This directory contains files that are common to all UIs (popup, devtools panel,
about:profiling) interacting with the profiler.
Other UIs external to the profiler (one example is about:logging) can also use
these files, especially background.sys.mjs, to interact with the profiler with
more capabilities than Services.profiler.
