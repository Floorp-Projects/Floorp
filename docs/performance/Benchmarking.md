# Benchmarking

## Debug Builds

Debug builds (\--enable-debug) and non-optimized builds
(\--disable-optimize) are *much* slower. Any performance metrics
gathered by such builds are largely unrelated to what would be found in
a release browser.

## Rust optimization level

Local optimized builds are [compiled with rust optimization level 1 by
default](https://groups.google.com/forum/#!topic/mozilla.dev.platform/pN9O5EB_1q4),
unlike Nightly builds, which use rust optimization level 2. This setting
reduces build times significantly but comes with a serious hit to
runtime performance for any rust code ([for example stylo and
webrender](https://groups.google.com/d/msg/mozilla.dev.platform/pN9O5EB_1q4/ooXNuqMECAAJ)).
Add the following to your
[mozconfig](/setup/configuring_build_options.html#using-a-mozconfig-configuration-file)
in order to build with level 2:

```
ac_add_options RUSTC_OPT_LEVEL=2
```

## GC Poisoning

Many Firefox builds have a diagnostic tool that causes crashes to happen
sooner and produce much more actionable information, but also slow down
regular usage substantially. In particular, \"GC poisoning\" is used in
all debug builds, and in optimized Nightly builds (but not opt Developer
Edition or Beta builds). The poisoning can be disabled by setting the
environment variable

```
    JSGC_DISABLE_POISONING=1
```

before starting the browser.

## Async Stacks

Async stacks no longer impact performance since **Firefox 78**, as
{{bug(1601179)}} limits async stack capturing to when DevTools is
opened.

Another option that is on by default in non-release builds is the
preference javascript.options.asyncstack, which provides better
debugging information to developers. Set it to false to match a release
build. (This may be disabled for many situations in the future. See
{{bug(1280819)}}.

## Accelerated Graphics

Especially on Linux, accelerated graphics can sometimes lead to severe
performance problems even if things look ok visually. Normally you would
want to leave acceleration enabled while profiling, but on Linux you may
wish to disable accelerated graphics (Preferences -\> Advanced -\>
General -\> Use hardware acceleration when available).

## Flash Plugin

If you are profiling real websites, you should disable the Adobe Flash
plugin so you are testing Firefox code and not Flash jank problems. In
about:addons \> Plugins, set Shockwave Flash to \"Never Activate\".

## Timer Precision

Firefox reduces the precision of the Performance APIs and other clock
and timer APIs accessible to Web Content. They are currently reduce to a
multiple of 2ms; which is controlled by the privacy.reduceTimerPrecision
about:config flag.

The exact value of the precision is controlled by the
privacy.resistFingerprinting.reduceTimerPrecision.microseconds
about:config flag.

## Profiling tools

Currently the Gecko Profiler has limitations in the UI for inverted call
stack top function analysis which is very useful for finding heavy
functions that call into a whole bunch of code. Currently such functions
may be easy to miss looking at a profile, so feel free to *also* use
your favorite native profiler. It also lacks features such as
instruction level profiling which can be helpful in low level profiling,
or finding the hot loop inside a large function, etc. Some example tools
include Instruments on OSX (part of XCode), [RotateRight
Zoom](http://www.rotateright.com/) on Linux (uses perf underneath), and
Intel VTune on Windows or Linux.
