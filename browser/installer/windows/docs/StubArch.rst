===========================
Stub Installer Architecture
===========================

The stub is called a stub because it doesn't actually install anything. It's just a GUI frontend that downloads and runs the full installer in silent mode. The exact full installer that will be downloaded isn't baked into the stub; the channel and the locale (which are baked in) are sent in a request to the Bouncer service, which uses that information to redirect to the URL of the specific full installer file.

The main stub installer source code file is `stub.nsi <https://searchfox.org/mozilla-central/source/browser/installer/windows/nsis/stub.nsi>`_. Even though the stub installer doesn't install anything, it's still built on NSIS. This means the structure of stub.nsi is a bit odd for an NSIS script. There's only one section, and it's empty, and there are no predefined pages used (including no instfiles page, which we get a warning from the compiler about). The work is all done in two custom pages, createProfileCleanup and createInstall, and the functions called by those two pages. Also, the stub's dialogs are built using HTML and the Windows WebBrowser control (based on Internet Explorer); see :doc:`this page <StubGUI>` for details about that.

The basic execution flow is this:

1. .onInit checks basic system requirements, determines whether we should install a 64-bit or a 32-bit build, looks for an existing installation that we should try to pave over, displays a UAC prompt, and initializes lots of variables and GUI objects.
2. createProfileCleanup determines if a profile cleanup prompt should be offered (see the ShouldPromptForProfileCleanup function), and calls the web control to draw the UI for that if so.
3. createInstall draws the UI for the download/install page, again using the web control, kicks off the periodic timer that swaps out the blurb text every few seconds (if the branding includes that), and runs StartDownload.
4. StartDownload invokes the InetBgDl plugin to begin the full installer download on a background thread. It then starts a periodic timer for the OnDownload function.
5. Every time OnDownload runs (every 200 ms), it checks the status of the background download, retries or restarts if the download has failed, updates the progress bar if the download is still running, and verifies and runs the full installer if the download is complete.
6. CheckInstall waits for the full installer to exit, then deletes the file.
7. FinishInstall copies the post-signing data, then waits for the application to launch, requesting it perform profile cleanup if that option was selected.
8. Once the installed application is running and has shown a window (or if another copy was already running), the stub sends its ping and then exits.
