# Generate new gn json files and moz.build files for building libwebrtc in our tree

1. If generating on macOS for Apple Silicon (cross-compiling), make sure to have at least
[Xcode 12.2](https://download.developer.apple.com/Developer_Tools/Xcode_12.2/Xcode_12.2.xip).

   In addition the aarch64 Rust target will need to be installed via: `rustup target add aarch64-apple-darwin`

2. If generating on Windows 10, Visual Studio 2019 is required.  Please follow
   the install instructions from [here](https://firefox-source-docs.mozilla.org/setup/windows_build.html)

   In addition, the following options must be selected in the VS2019 installer:
   - C++ ATL for latest v142 build tools (ARM64)
   - Windows 10 SDK (10.0.19041.0)
   - MSVC v142 - VS2019 C++ ARM64 build tools

   "Debugging Tools for Windows" is also required.
   - Under `Settings -> Apps` search for "Windows Software Development Kit" with the version
   number 10.0.19041.685.
   - select Modify (and allow the installer to modify)
   - select Change and then click Next
   - select "Debugging Tools for Windows" and then click Change.

   And the aarch64 Rust target will need to be installed via: `rustup target add aarch64-pc-windows-msvc`

3. See information in `third_party/libwebrtc/README.mozilla` for the proper revision of libwebrtc

        libwebrtc updated from commit https://github.com/mozilla/libwebrtc/archive/149d693483e9055f574d9d65b01fe75a186b654b.tar.gz on 2020-11-30T15:48:48.472088.
        third_party updated from commit https://chromium.googlesource.com/chromium/src/third_party/+archive/5dc5a4a45df9592baa8e8c5f896006d9193d8e45.tar.gz on 2020-11-30T17:00:15.612630.

   In our current case, the revision is `149d693483e9055f574d9d65b01fe75a186b654b` which
   corresponds to:

        mozilla-modifications-rel86

4. Clone Mozilla's version of libwebrtc from [libwebrtc](https://github.com/mozilla/libwebrtc)

        git clone https://github.com/mozilla/libwebrtc moz-libwebrtc
        export MOZ_LIBWEBRTC=`pwd`/moz-libwebrtc
        (cd moz-libwebrtc ; git checkout mozilla-modifications-rel86)

   Note that branch was made on `Thu Nov 19 14:14:00 2020`

5. Clone `depot_tools` from [depot_tools](https://chromium.googlesource.com/chromium/tools/depot_tools.git)

        git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
        export DEPOT_TOOLS=`pwd`/depot_tools

   If generating files on Windows, the following steps must be completed
   from a traditional Windows Cmd prompt (cmd.exe) launched from the start
   menu or search bar.  This allows `gclient` to properly bootstrap the
   required python setup.

        cd {depot_tools directory}
        set PATH=%CD%;%PATH%
        set DEPOT_TOOLS_WIN_TOOLCHAIN=0
        set vs2019_install="c:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
        gclient
        exit

   Now, we need to checkout a revision of `depot_tools` that corresponds to the date of
   our libwebrtc branch.  The closest `depot_tools` commit to `Thu Nov 19 14:14:00 2020` is
   `e7d1862b155ac3ccbef72c4d70629b5c88ffcb32`.  There is additional information on how to
   more automatically determine this [here](https://chromium.googlesource.com/chromium/src/+/master/docs/building_old_revisions.md).

        (cd depot_tools ; git checkout e7d1862b155ac3ccbef72c4d70629b5c88ffcb32 )

6. It is necessary to let `depot_tools` pull information into the `libwebrtc` tree as well.  This can take a while.

        (cd moz-libwebrtc ; \
         export PATH=$DEPOT_TOOLS:$PATH ; \
         export DEPOT_TOOLS_UPDATE=0 ; \
         export DEPOT_TOOLS_WIN_TOOLCHAIN=0 ; \
         gclient config https://github.com/mozilla/libwebrtc && \
         gclient sync -D --force --reset --with_branch_heads \
        )
  
    Note that if one uses `gclient` sync with a different output directory `$MOZ_LIBWEBRTC_GIT`
    must be set to the original clone directory, and `$MOZ_LIBWEBRTC` needs to be set to the
    directory created by `gclient sync`.

7. Now it is time to generate the build files.  The script should be run from the top
directory of our firefox tree.

        ./dom/media/webrtc/third_party_build/gn-configs/generate-gn-build-files.sh

   Debugging the generate script itself may prove useful, and one can do this by setting the DEBUG_GEN environment
   variable to a non-empty value. This will print everything that the script executes.

8. Checkin all the generated/modified files and try your build!

# Adding new configurations to the build

- Each new platform/architecture will require 2 new mozconfig files,
  one for the debug build and one for the non-debug build.  The
  filenames follow the same pattern as the generated json files,
  `a-b-c-d.mozconfig` where:
  - a = generating cpu (example: x64)
  - b = debug (True / False)
  - c = target cpu (example: x64 / arm64)
  - d = target platform (mac/linux)
- Each mozconfig file defines, in addition to debug/non-debug, the output
  directory (MOZ_OBJDIR) and any architecture definition.
- The new configs must be added to the appropriate platform section in
  `generate-gn-build-files.sh`.

**Note:** when adding new mozconfig files, especially for linux/android
configs, it is important to include the `ac_add_options
--enable-bootstrap`.  This ensures switching archtectures for
"cross-compiled" generation works properly.  For example, when generating
`x86` or `arm64` linux json files, it would be necessary to install additional
libraries in order for the configure step to complete.
