This folder contains scripts to sync the WebRender code from
mozilla-central to Github. The scripts in this folder were derived
from the code at `https://github.com/staktrace/wrupdater`; the main
difference is that the versions in this folder are designed to run
as a taskcluster task. The versions in this folder are the canonical
version going forward; the `staktrace/wrupdater` Github repo will
continue to exist only as a historical archive.

The main entry point is the `sync-to-github.sh` script. It creates a
staging directory at `~/.wrupdater` if one doesn't already exist,
and clones the `webrender` repo into it. The script also requires the
`GECKO_PATH` environment variable to point to a mercurial clone of
`mozilla-central`, and access to the taskcluster secrets service to
get a Github API token.

The script does some setup steps but the bulk of the actual work
is done by the `converter.py` script. This script scans the mercurial
repository for new changes to the `gfx/wr` folder, and adds commits to
the git repository corresponding to those changes. There are some
details in the implementation that make it more robust than simply
exporting patches and attempting to reapply them; in particular it
builds a commit tree structure that mirrors what is found in the
`mozilla-central` repository with respect to branches and merges. So
if conflicting changes land on autoland and inbound, and then get
merged, the git repository commits will have the same structure with
a fork/merge in the commit history. This was discovered to be
necessary after a previous version ran into multiple cases where
the simple patch approach didn't really work.

Once the converter is done converting, the `sync-to-github.sh` script
finishes the process by pushing the new commits to `moz-gfx/webrender`
and generating a pull request against `servo/webrender`. It also
leaves a comment on the PR that triggers testing and merge of the PR.
If there is already a pull request (perhaps from a previous run) the
pre-existing PR is force-updated instead. This allows for graceful
handling of scenarios where the PR failed to get merged (e.g. due to
CI failures on the Github side).

The script is intended to by run by taskcluster for any changes that
touch the `gfx/wr` folder that land on `mozilla-central`. This may mean
that multiple instances of this script run concurrently, or even out
of order (i.e. the task for an older m-c push runs after the task for
a newer m-c push). The script was written with these possibilities in
mind and should be able to eventually recover from any such scenario
automatically (although it may take additional changes to mozilla-central
for such recovery to occur).
