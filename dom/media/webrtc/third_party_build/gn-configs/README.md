# Generate new gn json files and moz.build files for building libwebrtc in our tree

1. If generating on macOS for Apple Silicon (cross-compiling), make sure to have
   at least [Xcode 12.2](https://download.developer.apple.com/Developer_Tools/Xcode_12.2/Xcode_12.2.xip).

   The aarch64 Rust target will need to be installed via:
   `rustup target add aarch64-apple-darwin`

2. If generating on Windows 10, Visual Studio 2019 is required.  Please follow
   the install instructions from [here](https://firefox-source-docs.mozilla.org/setup/windows_build.html)

   In addition, the following options must be selected in the VS2019 installer:
   - C++ ATL for latest v142 build tools (ARM64)
   - Windows 10 SDK (10.0.19041.0)
   - MSVC v142 - VS2019 C++ ARM64 build tools

   "Debugging Tools for Windows" is also required.
   - Under `Settings -> Apps` search for "Windows Software Development Kit" with
     the version number 10.0.19041.685.
   - select Modify (and allow the installer to modify)
   - select Change and then click Next
   - select "Debugging Tools for Windows" and then click Change.

   The aarch64 Rust target will need to be installed via:
   `rustup target add aarch64-pc-windows-msvc`

3. Download a version of the `gn` executable that corresponds to
    `Thu Nov 19 14:14:00 2020`.  In our case, that is version `1889 (8fe02009)`.
   
   - [Win](https://chrome-infra-packages.appspot.com/p/gn/gn/windows-amd64/+/e_UmTHedzuu4zJ2gdpW8jrFFNnzIhThljx3jn3RMlVsC)
   - [Linux](https://chrome-infra-packages.appspot.com/p/gn/gn/linux-amd64/+/bvBFKgehaepiKy_YhFnbiOpF38CK26N2OyE1R1jXof0C)
   - [macOS](https://chrome-infra-packages.appspot.com/p/gn/gn/mac-amd64/+/nXvMRpyJhLhisAcnRmU5s9UZqovzMAhKAvWjax-swioC)

   Find the downloaded `.zip` file, unzip and export the location of the
   executable:

        unzip gn-mac-amd64.zip && export GN=`pwd`/gn
        unzip gn-windows-amd64.zip && export GN=`pwd`/gn.exe
        unzip gn-linux-amd64.zip && export GN=`pwd`/gn

   On platforms that don't have pre-built `gn` executables, `ninja` and `gn` can
   be easily built:

        git clone https://github.com/ninja-build/ninja.git
        git clone https://gn.googlesource.com/gn
        (cd gn && git checkout 8fe02009)
        (cd ninja && ./configure.py --bootstrap)
        (export NINJA=`pwd`/ninja/ninja ; cd gn && python build/gen.py && $NINJA -C out)
        export GN=`pwd`/gn/out/gn

   On OpenBSD, a slightly newer version of `gn` is needed in order to build:

        (cd gn && git checkout 31f2bba8)

4. It is time to generate the build files.  The script should be run from the
   top directory of our firefox tree.

        bash ./dom/media/webrtc/third_party_build/gn-configs/generate-gn-build-files.sh

   Debugging the generate script itself may prove useful, and one can do this by
   setting the DEBUG_GEN environment variable to a non-empty value. This will 
   print everything that the script executes.

5. Checkin all the generated/modified files and try your build!

# Adding new configurations to the build

- Each new platform/architecture will require 2 new mozconfig files, one for the
  debug build and one for the non-debug build.  The filenames follow the same
  pattern as the generated json files, `a-b-c-d.mozconfig` where:
  - a = generating cpu (example: x64)
  - b = debug (True / False)
  - c = target cpu (example: x64 / arm64)
  - d = target platform (mac/linux)
- Each mozconfig file defines, in addition to debug/non-debug, the output
  directory (MOZ_OBJDIR) and any architecture definition.
- The new configs must be added to the appropriate platform section in
  `generate-gn-build-files.sh`.

**Note:** when adding new mozconfig files, especially for linux/android configs,
it is important to include the `ac_add_options --enable-bootstrap`.  This
ensures switching archtectures for "cross-compiled" generation works properly.
For example, when generating `x86` or `arm64` linux json files, it would be
necessary to install additional libraries in order for the configure step to
complete.
