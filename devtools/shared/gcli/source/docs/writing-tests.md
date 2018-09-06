
# Writing Tests

There are several sources of GCLI tests and several environments in which they
are run.

The majority of GCLI tests are stored in
[this repository](https://github.com/joewalker/gcli/) in files named like
```./lib/gclitest/test*.js```. These tests run in Firefox, Chrome, Opera,
and NodeJS/JsDom

See [Running Tests](running-tests.md) for further details.

GCLI comes with a generic unit test harness (in ```./lib/test/```) and a
set of helpers for creating GCLI tests (in ```./lib/gclitest/helpers.js```).

# GCLI tests in Firefox

The build process converts the GCLI tests to run under Mochitest inside the
Firefox unit tests. It also adds some
