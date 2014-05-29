In order for the loop component to build, run the make-links.sh script to
setup symlinks from the Gecko tree into your loop-client-repo for shared code,
noting that you'll need to put the correct path inside the [[ ]] placeholder
(and remove the [[ ]] characters).

$ export LOOP_CLIENT_REPO=[[/absolute-path/to/my/loop-client-repo/clone]]
$ ./make-links.sh

Note that changes to the shared directories affect both the standalone
loop-client and the desktop client. This means that sometimes, in order to
make a desktop client patch, you'll need to get reviews on patches to both
mozilla-central/gecko-dev as well as loop-client.
