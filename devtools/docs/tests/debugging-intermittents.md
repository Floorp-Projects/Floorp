# Debugging intermittent test failures

While working on automated tests, you will inevitably encounter intermittent test failures, also called "oranges" here.

This page documents some tips for finding and debugging these test failures.

## Finding Intermittents

Normally you will have no trouble finding out that a particular test is intermittent, because a bug will be filed and you will see it through the normal mechanisms.

However, it can still be useful to see intermittents in context.  The [War on Oranges site](https://brasstacks.mozilla.com/orangefactor/) shows intermittents ranked by frequency. The orange factor robot also posts weekly updates to the relevant bugs in Bugzilla (see [an example here](https://bugzilla.mozilla.org/show_bug.cgi?id=1250523#c4)).

You can also see oranges in Bugzilla.  Go to [the settings page](https://bugzilla.mozilla.org/userprefs.cgi?tab=settings) and enable "When viewing a bug, show its corresponding Orange Factor page".

## Reproducing Test Failures locally

The first step to fix an orange is to reproduce it.

If a test fails at different places for each failure it might be a timeout.  The current mochitest timeout is 45 seconds, so if successful runs of an intermittent are ~40 seconds, it might just be a
real timeout.  This is particularly true if the failure is most often seen on the slower builds, for example Linux 32 debug.  In this case you can either split the test or call `requestLongerTimeout`.

Sometimes reproducing can only be done in automation, but it's worth trying locally, because this makes it much simpler to debug.

First, try running the test in isolation.  You can use the `--repeat` and `--run-until-failure` flags to `mach mochitest` to automate this a bit.  It's nice to do this sort of thing in a VM (or using Xnest on Linux) to avoid locking up your machine.  Mozilla provides an [easy-to-use VM](https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Using_the_VM).

Sometimes, though, a test will only fail if it is run in conjunction with one or more other tests.  You can use the `--start-at` and `--end-at` flags with `mach mochitest` to run a group of tests together.

For some jobs, but not all, you can get an [interactive shell from TaskCluster](https://jonasfj.dk/2016/03/one-click-loaners-with-taskcluster/).

There's also a [handy page of e10s test debugging tips](https://wiki.mozilla.org/Electrolysis/e10s_test_tips) that is worth a read.

Because intermittents are often caused by race conditions, it's sometimes useful to enable Chaos Mode.  This changes timings and event orderings a bit. The simplest way to do this is to enable it in a specific test, by
calling `SimpleTest.testInChaosMode`.  You can also set the `MOZ_CHAOSMODE` environment variable, or even edit `mfbt/ChaosMode.cpp` directly.

Some tests leak intermittently. Use `ac_add_options --enable-logrefcnt` in your mozconfig to potentially find them.<!--TODO: how? add more detail about this -->

The `rr` tool has [its own chaos mode](http://robert.ocallahan.org/2016/02/introducing-rr-chaos-mode.html).  This can also sometimes reproduce a failure that isn't ordinarily reproducible.  While it's difficult to debug JS bugs using `rr`, often if you can reliably reproduce the failure you can at least experiment (see below) to attempt a fix.

## That Didn't Work

If you couldn't reproduce locally, there are other options.

One useful approach is to add additional logging to the test, then push again.  Sometimes log buffering makes the output weird; you can add a call to `SimpleTest.requestCompleteLog()` to fix this.

You can run a single directory of tests on try using `mach try DIR`.  You can also use the `--rebuild` flag to retrigger test jobs multiple times; or you can also do this easily from treeherder.<!--TODO: how? and why is it easy?-->

## Solving

Sometimes the problem is a race at a specific spot in the test.  You can test this theory by adding a short wait to see if the failure goes away, like:
```javascript
yield new Promise(r => setTimeout(r, 100));
```

See the `waitForTick` and `waitForTime` functions in `DevToolsUtils` for similar functionality.

You can use a similar trick to "pause" the test at a certain point. This is useful when debugging locally because it will leave Firefox open and responsive, at the specific spot you've chosen.  Do this
using `yield new Promise(r => r);`.

`shared-head.js` also has some helpers, like `once`, to bind to events with additional logging.

You can also binary search the test by either commenting out chunks of it, or hacking in early `return`s.  You can do a bunch of these experiments in parallel without waiting for the first to complete.

## Verifying

It's difficult to verify that an intermittent has truly been fixed.
One thing you can do is push to try, and then retrigger the job many times in treeherder.  Exactly how many times you should retrigger depends on the frequency of the failure.

