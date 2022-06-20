# Generate new gn json files and moz.build files for building libwebrtc in our tree

/!\ This is only supported on Linux and macOS. If you are on Windows, you can run
the script under [WSL](https://docs.microsoft.com/en-us/windows/wsl/install).

1. Download a version of the `gn` executable that corresponds to
    `Thu Nov 19 14:14:00 2020`.  In our case, that is version `1889 (8fe02009)`.
   
   - [Linux](https://chrome-infra-packages.appspot.com/p/gn/gn/linux-amd64/+/bvBFKgehaepiKy_YhFnbiOpF38CK26N2OyE1R1jXof0C)
   - [macOS](https://chrome-infra-packages.appspot.com/p/gn/gn/mac-amd64/+/nXvMRpyJhLhisAcnRmU5s9UZqovzMAhKAvWjax-swioC)

   Find the downloaded `.zip` file, unzip and export the location of the
   executable:

        unzip gn-mac-amd64.zip && export GN=`pwd`/gn
        unzip gn-linux-amd64.zip && export GN=`pwd`/gn

2. It is time to generate the build files.  The script should be run from the
   top directory of our firefox tree.

        bash ./dom/media/webrtc/third_party_build/gn-configs/generate-gn-build-files.sh

   Debugging the generate script itself may prove useful, and one can do this by
   setting the DEBUG_GEN environment variable to a non-empty value. This will 
   print everything that the script executes.

3. Checkin all the generated/modified files and try your build!

# Adding new configurations to the build

Edit the `GnConfigGenBackend` class in `python/mozbuild/mozbuild/gn_processor.py` file.
