# Mozilla Update Infrastructure

Firefox, Thunderbird, and Mozilla VPN updates rely on [backend service known as Balrog](https://github.com/mozilla-releng/balrog). Periodically, these applications check for updates by pinging an URL[^1] which contains information about both the application running, and details of the system it is running on. If Balrog finds an update based on the provided information, it returns a simple XML response with a link to the update file, and some metadata about it.

# Watershed Updates

Most of the time, an update will take an application directly from whatever version it is currently running to the latest version of that application. This is greatly preferred, both to save time, bandwidth, reduce restarts, and increase security by ensuring we get to the latest version of something as quickly as possible. Unfortunately, sometimes we must route through an intermediate version first, typically the version that is currently running does not provide Balrog with enough information to accurately serve a correct update to the latest version. These are colloquially known as "watershed" updates. (This name comes from the fact that these updates change something about the update process itself, making them a kind of "watershed moment" for updates.)

At the time of writing (December, 2022), we currently have the following watershed updates in place for Firefox's release channel:
* 12.0, all platforms. In this release [we added support for distinguishing the running and supported architectures in macOS builds in the update URL](https://bugzilla.mozilla.org/show_bug.cgi?id=583671).
* 43.0, Windows only. This release was [preparation for desupporting SHA-1 hashes in our Windows builds](https://bugzilla.mozilla.org/show_bug.cgi?id=1234277).
* 45.0.2, Linux only. We added support for [sending the GTK 3 version to the update URL](https://bugzilla.mozilla.org/show_bug.cgi?id=1227023).
* 47.0.2, Windows only. We [added CPU instruction set information to the update URL](https://bugzilla.mozilla.org/show_bug.cgi?id=1271761) to later allow SSE1 to be deprecated.
* 56.0, Windows only. We added support for [sending information about the JAWS accessibility application to the update URL](https://bugzilla.mozilla.org/show_bug.cgi?id=1402376) to avoid updating people on crashy versions of it.
* 72.0.2, all platforms. This is a required version due to a [necessary two-step password database migration](https://bugzilla.mozilla.org/show_bug.cgi?id=1615382).


[^1]: Here is a sample URL: https://aus5.mozilla.org/update/3/Firefox/85.0/20200518093924/WINNT_x86_64-msvc-x64/en-US/release/Windows_NT%2010.0.0.0.18363.1016%20(x64)/default/default/update.xml
