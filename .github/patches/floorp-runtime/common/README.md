# Common Runtime patches

These patches are applied on every platform by `package.yml`, after checking out
Floorp and before the artifact build. Keep Runtime changes here rather than
editing a sibling Floorp-Runtime checkout. Validate patches against the source
commit in `floorp-runtime.lock.json`, using an isolated checkout or source fixture.

`release-notes-guards.patch` adds the confirmed release-notes choice to the existing
extension guard mechanism and includes an xpcshell regression test. It composes
the Floorp guard with existing enterprise guards without replacing their settings.
It uses the Runtime's existing `enterprise-per-extension` source enum value as
guard metadata; no enterprise policy is written. This avoids adding a WebIDL enum
value that would require rebuilding native bindings. The packaging workflow uses
prebuilt native artifacts, so native/WebIDL changes cannot be delivered by this
source-patch step alone.

The default remains unrestricted. A target extension is restricted on
`https://blog.floorp.app` only when `floorp.releaseNotes.choiceConfirmed` is true
and `floorp.releaseNotes.mode` is `support`.

After applying the patch in a Runtime test build, run:

```sh
./mach xpcshell-test toolkit/components/extensions/test/xpcshell/test_ext_floorp_release_notes.js
```

Local development uses the matching `tools/patches/release-notes-guards.patch`
through the existing development patcher. It contains the same JavaScript
changes mapped to the unpacked artifact's `modules/` paths. The host tests check
that the development and packaging patches stay in sync.

The support restriction also covers webRequest/DNR requests and script/CSS
injection in third-party frames whose top document is the HTTPS Floorp blog.
webRequest uses the request ancestor snapshot. Content injection queries the
parent for the top WindowContext before scripts/CSS execute, caching only the
document identity result. Neither uses the currently selected tab.
Changing away from support restores future access; reload open blog pages to
remove already injected content and retry requests.
