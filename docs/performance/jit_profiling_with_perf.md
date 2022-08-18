# JIT Profiling with perf

perf is a performance profiling tool available on Linux that is capable of measuring performance events such as cycles, instructions executed, cache misses, etc and providing assembly and source code annotation.
It is possible to collect performance profiles of the SpiderMonkey JIT using perf on Linux and also annotate the generated assembly with the IR opcodes that were used during compilation as shown below.

![](img/annotation.png)

### Build setup

To enable JIT profiling with perf jitdump, you must build Firefox or the JS shell with the following flag:

```
ac_add_options --enable-perf
```

### Environment Variables

Two environment variables are available for perf JIT profiling:

The first environment variable must be defined for SpiderMonkey to generate the necessary jitdump binary files for JIT profiling.
The supported values are "ir", "src" and "func".  `IONPERF=ir` will enable IR annotation, `IONPERF=src` will enable source code annotation **only if** perf can read the source file locally.  `IONPERF=func` will disable all annotation and only function names will be available.  It is recommended to use `IONPERF=ir` for most situations.

The second environment variable is optional, but is highly recommended to avoid polluting your cwd by putting all jitdump files into a separate directory:
```
mkdir output
export PERF_SPEW_DIR=output
```

### Profiling the JS shell

Profiling the JS shell requires the following commands but is very straight forward.

Begin by removing any pre-existing jitdump files:

`rm -rf output` or `rm -f jitted-*.so jit.data perf.data jit-*.dump jitdump-*.txt`

Next define environment variables:
```
export IONPERF=ir
export PERF_SPEW_DIR=output
```

Run your test case with perf attached:
```
perf record -g -k 1 /home/denis/src/mozilla-central/obj-js/dist/bin/js test.js
```

Inject the jitdump files into your perf.data file:
```
perf inject -j -i perf.data -o jit.data
```

View the profile:
```
perf report --no-children -i jit.data
```

All of the above commands can be put into a single shell script.

### Profiling the Browser

Profiling the browser is less straight forward than the shell, but the only main difference is that perf must attach to the content process while it is running.

Begin by removing any pre-existing jitdump files:

`rm -rf output` or `rm -f jitted-*.so jit.data perf.data jit-*.dump jitdump-*.txt`

Next define environment variables:
```
export IONPERF=ir
export PERF_SPEW_DIR=output
export MOZ_DISABLE_CONTENT_SANDBOX=1
```

Run the Firefox browser
```
~/mozilla-central/obj-opt64/dist/bin/firefox -no-remote -profile ~/mozilla-central/obj-opt64/tmp/profile-default &
```

Navigate to the test case, but do not start it yet.  Then hover over the tab to get the content process PID.

![](img/pid.png)

Attach perf to begin profiling:
```
perf record -g -k 1 -p <pid>
```

Close the browser when finished benchmarking.

Inject the jitdump files into your perf.data file:
```
perf inject -j -i perf.data -o jit.data
```

View the profile:
```
perf report --no-children -i jit.data
```

