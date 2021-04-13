# Generate new gn json files and moz.build files for building libwebrtc in our tree

1. If generating on macOS for Apple Silicon (cross-compiling), download and install
[Xcode 12.2](https://download.developer.apple.com/Developer_Tools/Xcode_12.2/Xcode_12.2.xip)

        https://download.developer.apple.com/Developer_Tools/Xcode_12.2/Xcode_12.2.xip

    It may be possible to use the [Command Line Tools for Xcode 12.2](https://download.developer.apple.com/Developer_Tools/Command_Line_Tools_for_Xcode_12.2/Command_Line_Tools_for_Xcode_12.2.dmg)
    for a smaller download.

2. See information in `third_party/libwebrtc/README.mozilla` for the proper revision of libwebrtc

        libwebrtc updated from commit https://github.com/mozilla/libwebrtc/archive/149d693483e9055f574d9d65b01fe75a186b654b.tar.gz on 2020-11-30T15:48:48.472088.
        third_party updated from commit https://chromium.googlesource.com/chromium/src/third_party/+archive/5dc5a4a45df9592baa8e8c5f896006d9193d8e45.tar.gz on 2020-11-30T17:00:15.612630.

   In our current case, the revision is `149d693483e9055f574d9d65b01fe75a186b654b` which
   corresponds to:

        mozilla-modifications-rel86

3. Clone Mozilla's version of libwebrtc from [libwebrtc](https://github.com/mozilla/libwebrtc)

        git clone https://github.com/mozilla/libwebrtc moz-libwebrtc
        export MOZ_LIBWEBRTC=`pwd`/moz-libwebrtc
        (cd moz-libwebrtc ; git checkout mozilla-modifications-rel86)

   Note that branch was made on `Thu Nov 19 14:14:00 2020`

4. Clone `depot_tools` from [depot_tools](https://chromium.googlesource.com/chromium/tools/depot_tools.git)

        git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
        export DEPOT_TOOLS=`pwd`/depot_tools

   Now, we need to checkout a revision of `depot_tools` that corresponds to the date of
   our libwebrtc branch.  The closest `depot_tools` commit to `Thu Nov 19 14:14:00 2020` is
   `e7d1862b155ac3ccbef72c4d70629b5c88ffcb32`.  There is additional information on how to
   more automatically determine this [here](https://chromium.googlesource.com/chromium/src/+/master/docs/building_old_revisions.md).

        (cd depot_tools ; git checkout e7d1862b155ac3ccbef72c4d70629b5c88ffcb32 )

5. It is necessary to let `depot_tools` pull information into the `libwebrtc` tree as well.  This can take a while.

        (cd moz-libwebrtc ; \
         export PATH=$DEPOT_TOOLS:$PATH ; \
         export DEPOT_TOOLS_UPDATE=0 ; \
         gclient config https://github.com/mozilla/libwebrtc && \
         gclient sync -D --force --reset --with_branch_heads \
        )
  
    Note that if one uses `gclient` sync with a different output directory `$MOZ_LIBWEBRTC_GIT`
    must be set to the original clone directory, and `$MOZ_LIBWEBRTC` needs to be set to the
    directory created by `gclient sync`.

6. Now it is time to generate the build files.  The script should be run from the top
directory of our firefox tree.

        ./dom/media/webrtc/third_party_build/gn-configs/generate-gn-build-files.sh

   Debugging the generate script itself may prove useful, and one can do this by setting the DEBUG_GEN environment
   variable to a non-empty value. This will print everything that the script executes.

7. Checkin all the generated/modified files and try your build!
