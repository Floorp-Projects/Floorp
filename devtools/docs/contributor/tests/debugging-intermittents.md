# Debugging Intermittent Test Failures

## What are Intermittents (aka Oranges)?

Intermittents are test failures which happen intermittently, in a seemingly random way. Often you'll write a test that passes fine locally on your computer, but when ran thousands of times on various CI environments (some of them under heavy load) it may start to fail randomly.

Intermittents are also known as Oranges, because the corresponding test jobs are rendered orange on [treeherder](http://treeherder.mozilla.org/).

These intermittent failures are tracked in Bugzilla. When a test starts being intermittent a bug is filed in Bugzilla (usually by a Mozilla code sheriff).

Once the bug exists for a given test failure, all further similar failures of that test will be reported as comments within that bug.
These reports are usually posted weekly and look like this:

> 5 failures in 2740 pushes (0.002 failures/push) were associated with this bug in the last 7 days.

See [an example here](https://bugzilla.mozilla.org/show_bug.cgi?id=1250523#c4).

Sometimes, tests start failing more frequently and these reports are then posted daily.

To help with the (unfortunately) ever-growing list of intermittents, the Stockwell project was initiated a while ago (read more about the goals of that project on [their wiki](https://wiki.mozilla.org/Auto-tools/Projects/Stockwell)).

This project defines a scenario where very frequently failing tests get disabled.
Ideally, we should try to avoid this, because this means reducing our test coverage, but sometimes we do not have time to investigate the failure, and disabling it is the only remaining option.

## Finding Intermittents

You will have no trouble finding out that a particular test is intermittent, because a bug for it will be filed and you will see it in Bugzilla ([watching the Bugzilla component of your choice](https://bugzilla.mozilla.org/userprefs.cgi?tab=component_watch) is a good way to avoid missing the failure reports).

However, it can still be useful to see intermittents in context. The [Intermittent Failures View on Treeherder](https://treeherder.mozilla.org/intermittent-failures.html) shows intermittents ranked by frequency.

You can also see intermittents in Bugzilla.  Go to [the settings page](https://bugzilla.mozilla.org/userprefs.cgi?tab=settings) and enable "When viewing a bug, show its corresponding Orange Factor page".

## Reproducing Test Failures locally

The first step to fix an intermittent is to reproduce it.

Sometimes reproducing the failure can only be done in automation, but it's worth trying locally, because this makes it much simpler to debug.

First, try running the test in isolation.  You can use the `--repeat` and `--run-until-failure` flags to `mach mochitest` to automate this a bit.  It's nice to do this sort of thing in headless mode (`--headless`) or in a VM (or using Xnest on Linux) to avoid locking up your machine.  Mozilla provides an [easy-to-use VM](https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Using_the_VM).

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

If a test fails at different places for each failure it might be a timeout.  The current mochitest timeout is 45 seconds, so if successful runs of an intermittent are ~40 seconds, it might just be a
real timeout.  This is particularly true if the failure is most often seen on the slower builds, for example Linux 32 debug.  In this case you can either split the test or call `requestLongerTimeout` somewhere at the beginning of the test (here's [an example](https://searchfox.org/mozilla-central/rev/c56977420df7a1b692ce0f7e499ddb364d9fd7b2/devtools/client/framework/test/browser_toolbox_tool_remote_reopen.js#12)).

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
