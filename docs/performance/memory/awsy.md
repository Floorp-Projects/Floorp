# Are We Slim Yet (AWSY)

The Are We Slim Yet project (commonly known as AWSY) for several years
tracked memory usage across builds on the (now defunct) website. 
It used the same
infrastructure as
[about:memory](about_colon_memory.md) to measure
memory usage on a predefined snapshot of Alexa top 100 pages known as
tp5.

Since Firefox transitioned to using multiple processes by default, we
[moved AWSY into the
TaskCluster](https://bugzilla.mozilla.org/show_bug.cgi?id=1272113)
infrastructure. This allowed us to run measurements on all branches and
platforms. The results are posted to
[perfherder](https://treeherder.mozilla.org/perf.html) where we can
track regressions automatically.

As new processes are added to Firefox we want to make sure their memory
usage is also tracked by AWSY. To this end we request that memory
reporting be integrated into any new process before it is enabled on
Nightly.
