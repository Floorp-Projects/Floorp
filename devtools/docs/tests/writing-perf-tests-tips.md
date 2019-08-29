# How to write a good performance test?

## Verify that you wait for all asynchronous code

If your test involves asynchronous code, which is very likely given the DevTools codebase, please review carefully your test script.
You should ensure that _any_ code ran directly or indirectly by your test is completed.
You should not only wait for the functions related to the very precise feature you are trying to measure.

This is to prevent introducing noise in the test run after yours. If any asynchronous code is pending,
it is likely to run in parallel with the next test and increase its variance.
Noise in the tests makes it hard to detect small regressions.

You should typically wait for:
* All RDP requests to finish,
* All DOM Events to fire,
* Redux action to be dispatched,
* React updates,
* ...


## Ensure that its results change when regressing/fixing the code or feature you want to watch.

If you are writing the new test to cover a recent regression and you have a patch to fix it, push your test to try without _and_ with the regression fix.
Look at the try push and confirm that your fix actually reduces the duration of your perf test significantly.
If you are introducing a test without any patch to improve the performance, try slowing down the code you are trying to cover with a fake slowness like `setTimeout` for asynchronous code, or very slow `for` loop for synchronous code. This is to ensure your test would catch a significant regression.

For our click performance test, we could do this from the inspector codebase:
```
window.addEventListener("click", function () {

  // This for loop will fake a hang and should slow down the duration of our test
  for (let i = 0; i < 100000000; i++) {}

}, true); // pass `true` in order to execute before the test click listener
```


## Keep your test execution short.

Running performance tests is expensive. We are currently running them 25 times for each changeset landed in Firefox.
Aim to run tests in less than a second on try.