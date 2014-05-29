In order for the loop component to build, do the following:

* if you have an existing content/shared directory, move it out of the way:

mv content/shared content/shared.old

* create a symlink to the authoritative version of the shared directory in your
loop-client repo

ln -s /loop-client-repo-root-dir/static/shared ./content

Note that changes to the shared directory affect both the standlone loop-client
and the desktop client.  This means that sometimes, in order to
make a desktop client patch, you'll need to get reviews on patches to both
mozilla-central/gecko-dev as well as loop-client.  

