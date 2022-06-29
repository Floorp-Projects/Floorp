gecko-strings and Quarantine
============================

The actual generation is currently done via `taskcluster cron <https://treeherder.mozilla.org/jobs?repo=mozilla-central&searchStr=cross-channel>`_.
The state that is good to use by localizers at large is published at
https://hg.mozilla.org/l10n/gecko-strings/.

The L10n team is doing a :ref:`review step <exposure-in-gecko-strings>` before publishing the strings, and while
that is ongoing, the intermediate state is published to
https://hg.mozilla.org/l10n/gecko-strings-quarantine/.

The code is in https://hg.mozilla.org/mozilla-central/file/tip/python/l10n/mozxchannel/,
supported as a mach subcommand in https://hg.mozilla.org/mozilla-central/file/tip/tools/compare-locales/mach_commands.py,
as a taskcluster kind in https://hg.mozilla.org/mozilla-central/file/tip/taskcluster/ci/l10n-cross-channel, and scheduled in cron in https://hg.mozilla.org/mozilla-central/file/tip/.cron.yml.
