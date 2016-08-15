This directory is treated specially by the loaders.

In particular, when running in chrome, a resource like
"devtools/shared/platform/mumble" will be found in the chrome
subdirectory; and when running in content, it will be found in the
content subdirectory.

Outside of tests, it's not ok to require a specific version of a file;
and there is an eslint test to check for that.  That is,
require("devtools/shared/platform/client/mumble") is an error.

When adding a new file, you must add two copies, one to chrome and one
to content.  Otherwise, one case or the other will fail to work.
