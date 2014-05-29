In order for the loop component to build, do the following:

* if you have an existing content/shared directory, move it out of the way:

mv content/shared content/shared.old

* create a symlink to the authoritative version of the shared directory in your
loop-client repo.  If you first set the env vars referenced below,
you should be able to copy-paste these lines:

ln -s ${LOOP_CLIENT_REPO_DIR}/static/shared \
  ${GECKO_SRC_DIR}/browser/components/loop/content/shared

Once you're done, the browser/components/loop/content should look something
like this:

$ ls -l !$
ls -l browser/components/loop/content
total 32
-rw-r--r--  1 dmose  staff   900 Mar 27 16:17 conversation.html
drwxr-xr-x  6 dmose  staff   204 Mar 27 16:17 js
drwxr-xr-x  3 dmose  staff   102 Mar 26 15:36 libs
-rw-r--r--  1 dmose  staff  2159 Mar 26 15:36 panel.html
lrwxr-xr-x  1 dmose  staff    40 Mar 26 15:41 shared -> /Users/dmose/r/loop-client/static/shared


Note that changes to the shared directory affect both the standalone loop-client
and the desktop client.  This means that sometimes, in order to
make a desktop client patch, you'll need to get reviews on patches to both
mozilla-central/gecko-dev as well as loop-client.  

