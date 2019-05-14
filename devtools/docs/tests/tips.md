# Automated tests: Tips

If you run tests in debug mode, e.g. when debugging memory leaks, the test logs are often dominated by `++DOMWINDOW`, `++DOCSHELL`, `--DOMWINDOW`, `--DOCSHELL` lines.

These lines pollute the test logs making it difficult to find real problems. They also slow down our tests because they are not useful for debugging DevTools issues.

You can add this to your `.zshrc` or `.bashrc` to disable them:

```bash
# Disable those annoying +++DOMWINDOW and +++DOCSHELL printfs from Firefox logs
export MOZ_QUIET=1
```

You can also send `MOZ_QUIET` when you push to try&hellip; it makes the logs easier to read and makes the tests run faster because there is so much less logging.

Example try syntax containing `MOZ_QUIET`:

```
./mach try -b do -p linux,linux64,macosx64,win32,win64 \
  -u xpcshell,mochitest-bc,mochitest-e10s-bc,mochitest-dt,mochitest-chrome \
  -t damp --setenv MOZ_QUIET=1
```
