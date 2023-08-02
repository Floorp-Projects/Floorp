# Mozilla Update Infrastructure

Firefox, Thunderbird, and Mozilla VPN updates rely on [backend service known as Balrog](https://github.com/mozilla-releng/balrog). Periodically, these applications check for updates by pinging an URL[^1] which contains information about both the application running, and details of the system it is running on. If Balrog finds an update based on the provided information, it returns a simple XML response with a link to the update file, and some metadata about it.

# Watershed Updates

Most of the time, an update will take an application directly from whatever version it is currently running to the latest version of that application. This is greatly preferred, both to save time, bandwidth, reduce restarts, and increase security by ensuring we get to the latest version of something as quickly as possible. Unfortunately, sometimes we must route through an intermediate version first, typically the version that is currently running does not provide Balrog with enough information to accurately serve a correct update to the latest version. These are colloquially known as "watershed" updates. (This name comes from the fact that these updates change something about the update process itself, making them a kind of "watershed moment" for updates.)

At the time of writing (August, 2023), we currently have the following watershed updates in place for Firefox's release channel:
* 12.0, all platforms. In this release [we added support for distinguishing the running and supported architectures in macOS builds in the update URL](https://bugzilla.mozilla.org/show_bug.cgi?id=583671).
* 43.0.1, Windows only. This release was [preparation for desupporting SHA-1 hashes in our Windows builds](https://bugzilla.mozilla.org/show_bug.cgi?id=1234277).
* 45.0.2, Linux only. We added support for [sending the GTK 3 version to the update URL](https://bugzilla.mozilla.org/show_bug.cgi?id=1227023).
* 47.0.2, Windows only. We [added CPU instruction set information to the update URL](https://bugzilla.mozilla.org/show_bug.cgi?id=1271761) to later allow SSE1 to be deprecated.
* 56.0, Windows only. We added support for [sending information about the JAWS accessibility application to the update URL](https://bugzilla.mozilla.org/show_bug.cgi?id=1402376) to avoid updating people on crashy versions of it. Additionally, we [added support for LZMA compression to the updater](https://bugzilla.mozilla.org/show_bug.cgi?id=641212) and [swapped to new MAR signing keys](https://bugzilla.mozilla.org/show_bug.cgi?id=1324498).
* 57.0.4[^2], Linux & Mac only. We [added support for LZMA compression to the updater](https://bugzilla.mozilla.org/show_bug.cgi?id=641212) and [swapped to new MAR signing keys](https://bugzilla.mozilla.org/show_bug.cgi?id=1324498). (Note: Windows was not required here because they picked up this change in the above 56.0 watershed.)
* 72.0.2, all platforms. This is a required version due to a [necessary two-step password database migration](https://bugzilla.mozilla.org/show_bug.cgi?id=1615382).
* 109.0.1, macOS only.  This updates the channel-prefs.js file (normally excluded from updates) due to [signature issues](https://bugzilla.mozilla.org/show_bug.cgi?id=1804303).


[^1]: Here is a sample URL: https://aus5.mozilla.org/update/3/Firefox/85.0/20200518093924/WINNT_x86_64-msvc-x64/en-US/release/Windows_NT%2010.0.0.0.18363.1016%20(x64)/default/default/update.xml
[^2]: While not directly related to the watershed, it is notable that due to LZMA MAR support shipping in the previous version (56.0), which was _not_ a watershed for Linux and Mac, we shipped two different sets of MARs for 57.0, 57.0.1, 57.0.2, 57.0.3, and 57.0.4: we had bz2 MARs, which builds older than 56.0 were served, and a separate set of LZMA MARs that were shipped to 56.0 and higher. More details on this can be found on [this awfully complicated spreadsheet that was produced around that time](https://docs.google.com/spreadsheets/d/1xcs9iTShJI8WBvxrz547Zr5JClqdRUYEZRMbQqdQ58I/edit#gid=0).


# Desupport Updates

From time to time we stop supporting a platform, hardware or library that we depend on. When we do this, we must ensure we don't update affected users to a version that they can no longer run. This can take two forms:
* In the distant past, we would simply stop serving updates altogether, leaving users on their current version.
* More recently, we will first move users to our `esr` channel - generally giving them another year of support before ending updates altogether.

Below is a list of desupports and moves to `esr` that we've done on the Firefox release channel:
* (Ancient, desupported at various old versions): Darwin 6, Darwin 7, Darwin 8, Darwin 9, Windows_95, Windows_98, Windows_NT 4, Windows_NT 5.0, Windows_NT 5.1.0, Windows_NT 5.1.1, GTK 2.0., GTK 2.1., GTK 2.2., GTK 2.3., GTK 2.4., GTK 2.5., GTK 2.6., GTK 2.7., GTK 2.8., GTK 2.9., GTK 2.10., GTK 2.11., GTK 2.12., GTK 2.13., GTK 2.14., GTK 2.15., GTK 2.16., GTK 2.17., PPC Architecture
* <46.0: [Desupported GTK <3.4](https://bugzilla.mozilla.org/show_bug.cgi?id=1270358)
* <49.0: Desupported [Darwin 10, 11, and 12](https://bugzilla.mozilla.org/show_bug.cgi?id=1275607) (macOS 10.6, 10.7, 10.8), [SSE and MMX instruction sets](https://bugzilla.mozilla.org/show_bug.cgi?id=1284901)
* <52.0: [Moved Windows XP, Server 2003, and Vista users to esr52](https://bugzilla.mozilla.org/show_bug.cgi?id=1318922)
* <53.0: [Desupported 32-bit macOS hardware](https://bugzilla.mozilla.org/show_bug.cgi?id=1325584)
* <77.0: [Moved Darwin 13, 14, and 15](https://bugzilla.mozilla.org/show_bug.cgi?id=1639199) (macOS 10.9, 10.10, and 10.11) to esr78
* <116.0: [Moved Darwin 16, 17, and 18](https://bugzilla.mozilla.org/show_bug.cgi?id=1836375) (macOS 10.12, 10.13, and 10.14) to esr115
* <116.0: [Moved Windows 7, 8, and 8.1](https://bugzilla.mozilla.org/show_bug.cgi?id=1594270) to esr115
