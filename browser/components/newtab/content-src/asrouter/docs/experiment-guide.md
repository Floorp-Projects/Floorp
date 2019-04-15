# How to run experiments with ASRouter

This guide will tell you how to run an experiment with ASRouter messages.
Note that the actual experiment proccess and infrastructure is handled by
the experiments team (#ask-experimenter).

## Why run an experiment

* To measure the effect of a message on a Firefox metric (e.g. retention)
* To test a potentially risky message on a smaller group of users
* To compare the performance of multiple variants of messages in a controlled way

## Choose cohort IDs and request an experiment

First you should decide on a cohort ID (this can be any arbitrary unique string) for each
individual group you need to segment for your experiment.

For example, if I want to test two variants of an FXA Snippet, I might have two cohort IDs,
`FXA_SNIPPET_V1` and `FXA_SNIPPET_V2`.

You will then [request](https://experimenter.services.mozilla.com/) a new "pref-flip" study with the Firefox Experiments team.
The preferences you will submit will be based on the cohort IDs you chose.

For the FXA Snippet example, your preference name would be `browser.newtabpage.activity-stream.asrouter.providers.snippets` and values would be:

Control (default value)
```json
{"id":"snippets","enabled":true,"type":"remote","url":"https://snippets.cdn.mozilla.net/%STARTPAGE_VERSION%/%NAME%/%VERSION%/%APPBUILDID%/%BUILD_TARGET%/%LOCALE%/release/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/","updateCycleInMs":14400000}
```

Variant 1:
```json
{"id":"snippets", "cohort": "FXA_SNIPPET_V1", "enabled":true,"type":"remote","url":"https://snippets.cdn.mozilla.net/%STARTPAGE_VERSION%/%NAME%/%VERSION%/%APPBUILDID%/%BUILD_TARGET%/%LOCALE%/release/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/","updateCycleInMs":14400000}
```

Variant 2:
```json
{"id":"snippets", "cohort": "FXA_SNIPPET_V1", "enabled":true,"type":"remote","url":"https://snippets.cdn.mozilla.net/%STARTPAGE_VERSION%/%NAME%/%VERSION%/%APPBUILDID%/%BUILD_TARGET%/%LOCALE%/release/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/","updateCycleInMs":14400000}
```

## Add targeting to your messages

You must now check for the cohort ID in the `targeting` expression of the messages you want to include in your experiments.

For the previous example, you wold include the following to target the first cohort:

```json
{
  "targeting": "providerCohorts.snippets == \"FXA_SNIPPET_V1\""
}

```
