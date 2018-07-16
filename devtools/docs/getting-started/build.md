# Set up to build Firefox Developer Tools

These are the steps we're going to look at:

* [Getting the code](#getting-the-code)
* [Building and running locally](#building-and-running-locally)
  * [Rebuilding](#rebuilding)
  * [Artifact builds](#building-even-faster-with-artifact-builds) for even faster builds
  * [Maybe you don't even need to build](#maybe-you-dont-even-need-to-build)

## Getting the code

The code is officially hosted on [a Mercurial repository](https://hg.mozilla.org/mozilla-central/). It takes a long time to clone, because the repository is **B I G**. So be prepared to be patient.

```bash
hg clone http://hg.mozilla.org/mozilla-central
```

## Building and running locally

Fortunately, the Firefox team has made a very good job of automating the building process with bootstrap scripts and putting [documentation](https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build) together.

The very first time you are building Firefox, run:

```bash
./mach bootstrap
./mach configure
./mach build
```

After that first successful build, you can just run:

```bash
./mach build
```

If your system needs additional dependencies installed (for example, Python, or a compiler, etc), various diagnostic messages will be printed to your screen. Follow their advice and try building again.

Building also takes a long time (specially on slow computers).

**Note:** if using Windows, you might need to type `mach build` instead (without the `./`).

### Running your own compiled version of Firefox

To run the Firefox you just compiled:

```bash
./mach run
```

This will run using an empty temporary profile which is discarded when you close the browser. We will look more into [persistent development profiles later](./development-profiles.md).

### Rebuilding

Suppose you pulled the latest changes from the remote repository (or made your own changes) and want to build again.

You can ask the `mach` script to build only changed files:

```bash
./mach build faster
```

This should be faster (a matter of seconds).

Sometimes, if you haven't updated in a while, you'll be told that you need to *clobber*, or basically delete precompiled stuff and start from scratch, because there are too many changes. The way to do it is:

```bash
./mach clobber
./mach build
```

### Building even faster with artifact builds

If you are *not* going to modify any C/C++ code (which is rare when working on DevTools), you can use artifact builds. This method downloads prebuilt binary components, and then the build process becomes faster.

Create a file on the root of the repository, called `mozconfig`, with the following content:

```
# Automatically download and use compiled C++ components:
ac_add_options --enable-artifact-builds
 
# Write build artifacts to:
mk_add_options MOZ_OBJDIR=./objdir-frontend
```

And then you can follow the normal build process again (only *faster*!)

On MacOS you might want to use `MOZ_OBJDIR=./objdir-frontend.noindex` instead. Using the `.noindex` file extension prevents the Spotlight from indexing your `objdir`, which is slow.

For more information on aspects such as technical limitations of artifact builds, read the [Artifact Builds](https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Artifact_builds) page.

## Maybe you don't even need to build

Working in DevTools generally involves editing JavaScript files only. This means that often you don't even need to run `./mach build`.

Instead, what you need to do is to save the files you modified, quit Firefox, and reopen it again to use the recently saved files.

With pseudocode:

```
# 1. Build
./mach build
# 2. Run
./mach run
# 3. you try out things in the browser that opens
# 4. fully close the browser, e.g. ⌘Q in MacOS
# 5. edit JS files on the `devtools` folder, save
# 6. Back to step 2!
./mach run
```

While not as fast as the average "save file and reload the website" *web development workflow*, or newer workflows such as React's reloading, this can still be quite fast.

And certainly faster than building each time, even with artifact builds.
