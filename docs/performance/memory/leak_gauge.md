# Leak Gauge

Leak Gauge is a tool that can be used to detect certain kinds of leaks
in Gecko, including those involving documents, window objects, and
docshells. It has two parts: instrumentation in Gecko that produces a
log file, and a script to post-process the log file.

## Getting a log file

To get a log file, run the browser with these settings:

    NSPR_LOG_MODULES=DOMLeak:5,DocumentLeak:5,nsDocShellLeak:5,NodeInfoManagerLeak:5
    NSPR_LOG_FILE=nspr.log     # or any other filename of your choice

This overwrites any existing file named `nspr.log`. The browser runs
with a negligible slowdown. For reliable results, exit the browser
before post-processing the log file.

## Post-processing the log file

Post-process the log file with
[tools/leak-gauge/leak-gauge.pl](https://searchfox.org/mozilla-central/source/tools/leak-gauge/leak-gauge.html)

If there are no leaks, the output looks like this:

    Results of processing log leak.log :
    Summary:
    Leaked 0 out of 11 DOM Windows
    Leaked 0 out of 44 documents
    Leaked 0 out of 3 docshells
    Leaked content nodes in 0 out of 0 documents

If there are leaks, the output looks like this:

    Results of processing log leak2.log :
    Leaked outer window 2c6e410 at address 2c6e410.
    Leaked outer window 2c6ead0 at address 2c6ead0.
    Leaked inner window 2c6ec80 (outer 2c6ead0) at address 2c6ec80.
    Summary:
    Leaked 13 out of 15 DOM Windows
    Leaked 35 out of 46 documents
    Leaked 4 out of 4 docshells
    Leaked content nodes in 42 out of 53 documents

If you find leaks, please file a bug report.
