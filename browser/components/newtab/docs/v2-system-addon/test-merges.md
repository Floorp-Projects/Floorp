## bin/test-merges.js documentation

A script intended to be run from cron regularly.  It notices when a new PR has been merged to github, and then exports the code to a copy of mozilla-central and pushes it to pine, so that all the tests can be run.  It annotates the PR with the link to treeherder with the test results.

Setup, needs to happen before first run:

Ensure that mozilla/activity-stream has a label called pushed-to-pine.

```bash
# mkdir /home/monkey/as-pine-testing
# cd /home/monkey/as-pine-testing
# git clone https://github.com/mozilla/activity-stream.git
# npm install
```

Example usage:

```bash
AS_PINE_TOKEN=01234567890 \
AS_PINE_TEST_DIR=/home/monkey/as-pine-testing \
node bin/test-merges.js
```

AS_PINE_TOKEN is a github token for accessing mozilla/activity-stream. We use a token from the github user that has access to the mozilla/activity-stream repo (in order to label issues), and nothing else.

AS_PINE_TEST_DIR should be a single directory which will contain local copies of both the activity-stream github repo and mozilla-central.  It's highly advised that AS_PINE_TEST_DIR be used for nothing else, to avoid accidentally clobbering real work.
