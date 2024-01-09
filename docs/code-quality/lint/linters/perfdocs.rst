PerfDocs
========

`PerfDocs`_ is a tool that checks to make sure all performance tests are documented in tree.

At the moment, it is only used for this documentation verification, but in the future it will also auto-generate documentation from these descriptions that will be displayed in the source-docs documentation page (rather than the wiki, which is where they currently reside).

Run Locally
-----------

The mozlint integration of PerfDocs can be run using mach:

.. parsed-literal::

    $ mach lint --linter perfdocs .


Configuration
-------------

There are no configuration options available for this linter. It scans the full source tree under ``testing``, looking for folders named ``perfdocs`` and then validates their content. This has only been implemented for Raptor so far, but Talos will be added in the future. We also hope to expand this to search outside the ``testing`` directory.

The ``perfdocs`` folders, there needs to be an ``index.rst`` file and it needs to contain the string ``{documentation}`` in some location in the file which is where the test documentation will be placed. The folders must also have a ``config.yml`` file following this schema:

.. code-block:: python

    CONFIG_SCHEMA = {
        "type": "object",
        "properties": {
            "name": {"type": "string"},
            "manifest": {"type": "string"},
            "suites": {
                "type": "object",
                "properties": {
                    "suite_name": {
                        "type": "object",
                        "properties": {
                            "tests": {
                                "type": "object",
                                "properties": {
                                    "test_name": {"type": "string"},
                                }
                            },
                            "description": {"type": "string"},
                        },
                        "required": [
                            "description"
                        ]
                    }
                }
            }
        },
        "required": [
            "name",
            "manifest",
            "suites"
        ]
    }

Here is an example of a configuration file for the Raptor framework:

.. parsed-literal::

    name: raptor
    manifest: testing/raptor/raptor/raptor.toml
    suites:
        desktop:
            description: "Desktop tests."
            tests:
                raptor-tp6: "Raptor TP6 tests."
        mobile:
            description: "Mobile tests"
        benchmarks:
            description: "Benchmark tests."
            tests:
                wasm: "All wasm tests."

Note that there needs to be a FrameworkGatherer implemented for the framework being documented since each of them may have different ways of parsing test manifests for the tests. See `RaptorGatherer <https://searchfox.org/mozilla-central/source/tools/lint/perfdocs/framework_gatherers.py>`_ for an example gatherer that was implemented for Raptor.

Sources
-------

* `Configuration <https://searchfox.org/mozilla-central/source/tools/lint/perfdocs.yml>`__
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/perfdocs>`__
